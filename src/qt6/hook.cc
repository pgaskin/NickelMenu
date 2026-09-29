#include "hook.h"
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <elf.h>
#include <link.h>
#include <sched.h>
#include <sys/mman.h>
#include <unistd.h>

static_assert(sizeof(uintptr_t) == 4, "ARM32 only");
#if !defined(__arm__) || __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error The Qt 6 backend supports little-endian ARM only
#endif

namespace nm {
int protection(uintptr_t address, size_t length) {
    FILE *maps = fopen("/proc/self/maps", "re");
    if (!maps) return -1;
    char line[1024], permissions[5];
    unsigned long start, end;
    int result = -1;
    while (fgets(line, sizeof(line), maps)) {
        if (sscanf(line, "%lx-%lx %4s", &start, &end, permissions) == 3 &&
            address >= start && address < end && length <= end - address) {
            result = (permissions[0] == 'r' ? PROT_READ : 0) |
                     (permissions[1] == 'w' ? PROT_WRITE : 0) |
                     (permissions[2] == 'x' ? PROT_EXEC : 0);
            break;
        }
    }
    fclose(maps);
    return result;
}

namespace {
unsigned mutation_lock = 0;
struct MutationLock {
    MutationLock() {
        while (__atomic_exchange_n(&mutation_lock, 1, __ATOMIC_ACQUIRE)) sched_yield();
    }
    ~MutationLock() { __atomic_store_n(&mutation_lock, 0, __ATOMIC_RELEASE); }
};
struct Image {
    uintptr_t base;
    const Elf32_Phdr *phdr;
    size_t count;
    bool found;
    bool contains(uintptr_t address, size_t length, unsigned type = PT_LOAD,
                  unsigned flags = PF_R) const {
        for (size_t i = 0; i < count; ++i) {
            const auto &p = phdr[i];
            const uintptr_t start = base + p.p_vaddr;
            if (p.p_type == type && (p.p_flags & flags) == flags &&
                address >= start && address - start < p.p_memsz &&
                length <= p.p_memsz - (address - start)) return true;
        }
        return false;
    }
    uintptr_t dynamic_pointer(uintptr_t value, size_t length) const {
        // glibc relocates some DT_* pointers in memory. Accept only an address
        // proven to be inside this image, whether absolute or base-relative.
        if (contains(value, length)) return value;
        if (value <= UINTPTR_MAX - base && contains(base + value, length)) return base + value;
        return 0;
    }
};
int find_image(dl_phdr_info *info, size_t, void *data) {
    auto &image = *static_cast<Image *>(data);
    if (info->dlpi_addr != image.base) return 0;
    image.phdr = info->dlpi_phdr;
    image.count = info->dlpi_phnum;
    image.found = true;
    return 1;
}
bool read_build_id(const Image &image, char output[41]) {
    for (size_t i = 0; i < image.count; ++i) {
        const auto &p = image.phdr[i];
        if (p.p_type != PT_NOTE || !image.contains(image.base + p.p_vaddr, p.p_memsz)) continue;
        const auto *bytes = reinterpret_cast<const unsigned char *>(image.base + p.p_vaddr);
        size_t offset = 0;
        while (p.p_memsz - offset >= sizeof(Elf32_Nhdr)) {
            Elf32_Nhdr note;
            memcpy(&note, bytes + offset, sizeof(note));
            offset += sizeof(note);
            if (note.n_namesz > p.p_memsz - offset) break;
            const size_t names = (static_cast<size_t>(note.n_namesz) + 3) & ~size_t(3);
            if (names > p.p_memsz - offset) break;
            if (note.n_descsz > p.p_memsz - offset - names) break;
            const size_t desc = (static_cast<size_t>(note.n_descsz) + 3) & ~size_t(3);
            if (desc > p.p_memsz - offset - names) break;
            if (note.n_type == NT_GNU_BUILD_ID && note.n_namesz == 4 && note.n_descsz == 20 &&
                memcmp(bytes + offset, "GNU\0", 4) == 0) {
                for (size_t j = 0; j < 20; ++j)
                    snprintf(output + 2*j, 3, "%02x", bytes[offset + names + j]);
                output[40] = 0;
                return true;
            }
            offset += names + desc;
        }
    }
    return false;
}

const char *exchange(Hook &hook, uintptr_t expected, uintptr_t desired) {
    // A caller can reach the wrapper immediately after publication. Serialize
    // its restoration with the installer's final read-only mprotect.
    MutationLock lock;
    if (!hook.slot || protection(hook.page, hook.page_size) != hook.protection)
        return "REFUSED: slot page protection changed";
    if (__atomic_load_n(hook.slot, __ATOMIC_ACQUIRE) != expected)
        return "REFUSED: slot destination changed";
    if (mprotect(reinterpret_cast<void *>(hook.page), hook.page_size,
                 hook.protection | PROT_WRITE) != 0) return "ERROR: mprotect writable failed";
    const bool changed = __atomic_compare_exchange_n(hook.slot, &expected, desired, false,
                                                     __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    if (mprotect(reinterpret_cast<void *>(hook.page), hook.page_size, hook.protection) != 0) {
        // Best effort rollback, without overwriting a competing hook. Never
        // report success if the page could not be returned to read-only.
        if (changed) __atomic_compare_exchange_n(hook.slot, &desired, expected, false,
                                                 __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
        if (mprotect(reinterpret_cast<void *>(hook.page), hook.page_size, hook.protection) != 0)
            return "ERROR: protection restore AND retry failed; reboot required";
        return "ERROR: protection restore failed; slot rolled back";
    }
    return changed ? nullptr : "REFUSED: concurrent slot change";
}
}

const char *prepare(const Spec &spec, uintptr_t replacement, Hook &out) {
    out = {};
    void *handle = dlopen(spec.library, RTLD_NOW | RTLD_NOLOAD);
    if (!handle) return "REFUSED: target library is not already loaded";
    struct Close { void *p; ~Close() { dlclose(p); } } close{handle};
    link_map *map = nullptr;
    if (dlinfo(handle, RTLD_DI_LINKMAP, &map) != 0 || !map) return "REFUSED: no link map";
    Image image{static_cast<uintptr_t>(map->l_addr), nullptr, 0, false};
    dl_iterate_phdr(find_image, &image);
    char build_id[41]{};
    if (!image.found || !read_build_id(image, build_id)) return "REFUSED: missing runtime build ID";
    if (spec.build_id && spec.build_id[0] &&
        (strlen(spec.build_id) != 40 || strcmp(build_id, spec.build_id) != 0))
        return "REFUSED: runtime build ID mismatch";
    const uintptr_t destination = reinterpret_cast<uintptr_t>(dlsym(handle, spec.symbol));
    if (!(destination & 1) || !image.contains(destination & ~uintptr_t(1), 2, PT_LOAD, PF_R | PF_X) ||
        (spec.symbol_value && destination != image.base + spec.symbol_value))
        return "REFUSED: unexpected function definition or Thumb bit";
    if (!(replacement & 1)) return "REFUSED: replacement is not Thumb";

    uintptr_t rel = 0, symbols = 0, strings = 0;
    size_t rel_size = 0, str_size = 0, sym_size = 0;
    bool rel_type = false, now = false, terminated = false;
    for (size_t i = 0; i < image.count; ++i) {
        const auto &p = image.phdr[i];
        if (p.p_type != PT_DYNAMIC || !image.contains(image.base + p.p_vaddr, p.p_memsz)) continue;
        const auto *dynamic = reinterpret_cast<const Elf32_Dyn *>(image.base + p.p_vaddr);
        for (size_t j = 0; j < p.p_memsz / sizeof(*dynamic); ++j) {
            const auto &d = dynamic[j];
            if (d.d_tag == DT_NULL) { terminated = true; break; }
            switch (d.d_tag) {
                case DT_JMPREL: rel = d.d_un.d_ptr; break;
                case DT_PLTRELSZ: rel_size = d.d_un.d_val; break;
                case DT_PLTREL: rel_type = d.d_un.d_val == DT_REL; break;
                case DT_SYMTAB: symbols = d.d_un.d_ptr; break;
                case DT_SYMENT: sym_size = d.d_un.d_val; break;
                case DT_STRTAB: strings = d.d_un.d_ptr; break;
                case DT_STRSZ: str_size = d.d_un.d_val; break;
                case DT_BIND_NOW: now = true; break;
                case DT_FLAGS: now = now || (d.d_un.d_val & DF_BIND_NOW); break;
                case DT_FLAGS_1: now = now || (d.d_un.d_val & DF_1_NOW); break;
            }
        }
    }
    if (!terminated || !now || !rel_type || !rel_size || rel_size % sizeof(Elf32_Rel) ||
        sym_size != sizeof(Elf32_Sym) || !str_size) return "REFUSED: unsupported dynamic table";
    rel = image.dynamic_pointer(rel, rel_size);
    symbols = image.dynamic_pointer(symbols, sizeof(Elf32_Sym));
    strings = image.dynamic_pointer(strings, str_size);
    if (!rel || !symbols || !strings) return "REFUSED: unmapped dynamic table pointers";
    uintptr_t slot = 0;
    for (size_t i = 0; i < rel_size / sizeof(Elf32_Rel); ++i) {
        const auto &r = reinterpret_cast<const Elf32_Rel *>(rel)[i];
        if (ELF32_R_TYPE(r.r_info) != R_ARM_JUMP_SLOT) continue;
        const size_t index = ELF32_R_SYM(r.r_info);
        if (index > (UINTPTR_MAX - symbols) / sizeof(Elf32_Sym)) return "REFUSED: symbol index overflow";
        const uintptr_t address = symbols + index * sizeof(Elf32_Sym);
        if (!image.contains(address, sizeof(Elf32_Sym))) return "REFUSED: unmapped symbol";
        const auto &s = *reinterpret_cast<const Elf32_Sym *>(address);
        if (s.st_name >= str_size) return "REFUSED: invalid symbol string";
        const auto *name = reinterpret_cast<const char *>(strings + s.st_name);
        if (!memchr(name, 0, str_size - s.st_name)) return "REFUSED: unterminated symbol string";
        if (strcmp(name, spec.symbol) != 0) continue;
        if (slot || ELF32_ST_TYPE(s.st_info) != STT_FUNC || ELF32_ST_BIND(s.st_info) != STB_GLOBAL ||
            s.st_shndx == SHN_UNDEF || image.base + s.st_value != destination)
            return "REFUSED: ambiguous or unexpected jump-slot symbol";
        if (spec.slot_offset && r.r_offset != spec.slot_offset) return "REFUSED: unexpected slot offset";
        slot = image.base + r.r_offset;
    }
    if (!slot || slot % alignof(uintptr_t) || !image.contains(slot, sizeof(uintptr_t)) ||
        !image.contains(slot, sizeof(uintptr_t), PT_GNU_RELRO, 0)) return "REFUSED: slot is not mapped RELRO";
    const long size = sysconf(_SC_PAGESIZE);
    if (size <= 0 || (size & (size - 1))) return "REFUSED: unsupported page size";
    const uintptr_t page = slot & ~(static_cast<uintptr_t>(size) - 1);
    const int prot = protection(page, size);
    if (prot != PROT_READ) return "REFUSED: expected read-only, non-executable GOT page";
    if (__atomic_load_n(reinterpret_cast<uintptr_t *>(slot), __ATOMIC_ACQUIRE) != destination)
        return "REFUSED: slot does not point to original function";
    void *pinned = dlopen(spec.library, RTLD_NOW | RTLD_NOLOAD | RTLD_NODELETE);
    if (!pinned) return "REFUSED: could not pin original function's library";
    dlclose(pinned);
    out.slot = reinterpret_cast<uintptr_t *>(slot);
    out.original = destination;
    out.replacement = replacement;
    out.page = page;
    out.page_size = static_cast<size_t>(size);
    out.protection = prot;
    memcpy(out.build_id, build_id, sizeof(out.build_id));
    out.symbol_value = destination - image.base;
    out.slot_offset = slot - image.base;
    return nullptr;
}
const char *install(Hook &hook) { return exchange(hook, hook.original, hook.replacement); }
const char *restore(Hook &hook) { return exchange(hook, hook.replacement, hook.original); }
}
