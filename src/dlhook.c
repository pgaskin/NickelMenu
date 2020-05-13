#define _GNU_SOURCE // dladdr, dladdr1, dlinfo, Dl_info
#include <dlfcn.h>
#include <elf.h>
#include <link.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "dlhook.h"
#include "util.h"

#ifndef ELFW
#define ELFW(type) _ElfW(ELF, __ELF_NATIVE_CLASS, type)
#endif

#if __x86_64__
#define R_JUMP_SLOT R_X86_64_JUMP_SLOT
#else
#if __arm__
#define R_JUMP_SLOT R_ARM_JUMP_SLOT
#else
#error Please define the architecture-specific R_*_JUMP_SLOT type.
#endif
#endif

// prevent GCC from giving us warnings everywhere about the format specifiers for the ELF typedefs
#pragma GCC diagnostic ignored "-Wformat"

void *nm_dlhook(void *handle, const char *symname, void *target, char **err_out) {
    #define NM_ERR_RET NULL

    NM_ASSERT(handle && symname && target, "BUG: required arguments are null");

    // the link_map conveniently gives use the base address without /proc/maps, and it gives us a pointer to dyn
    struct link_map *lm;
    NM_ASSERT(!dlinfo(handle, RTLD_DI_LINKMAP, &lm), "could not get link_map for lib");
    NM_LOG("lib %s is mapped at %lx", lm->l_name, lm->l_addr);

    // stuff extracted from DT_DYNAMIC
    struct {
        bool plt_is_rela;
        union {
            ElfW(Addr) _;
            ElfW(Rel)  *rel;
            ElfW(Rela) *rela;
        } plt;
        ElfW(Xword) plt_sz;
        ElfW(Xword) plt_ent_sz;
        ElfW(Sym)   *sym;
        const char  *str;
    } dyn = {0};

    // parse DT_DYNAMIC
    for (size_t i = 0; lm->l_ld[i].d_tag != DT_NULL; i++) {
        switch (lm->l_ld[i].d_tag) {
        case DT_PLTREL:   dyn.plt_is_rela = lm->l_ld[i].d_un.d_val == DT_RELA;          break; // .rel.plt - is Rel or Rela
        case DT_JMPREL:   dyn.plt._       = lm->l_ld[i].d_un.d_ptr;                     break; // .rel.plt - offset from base
        case DT_PLTRELSZ: dyn.plt_sz      = lm->l_ld[i].d_un.d_val;                     break; // .rel.plt - size in bytes
        case DT_RELENT:   if (!dyn.plt_ent_sz) dyn.plt_ent_sz = lm->l_ld[i].d_un.d_val; break; // .rel.plt - entry size if Rel
        case DT_RELAENT:  if (!dyn.plt_ent_sz) dyn.plt_ent_sz = lm->l_ld[i].d_un.d_val; break; // .rel.plt - entry size if Rela
        case DT_SYMTAB:   dyn.sym         = (ElfW(Sym)*)(lm->l_ld[i].d_un.d_val);       break; // .dynsym  - offset
        case DT_STRTAB:   dyn.str         = (const char*)(lm->l_ld[i].d_un.d_val);      break; // .dynstr  - offset
        }
    }
    NM_LOG("DT_DYNAMIC: plt_is_rela=%d plt=%p plt_sz=%lu plt_ent_sz=%lu sym=%p str=%p", dyn.plt_is_rela, (void*)(dyn.plt)._, dyn.plt_sz, dyn.plt_ent_sz, dyn.sym, dyn.str);
    NM_ASSERT(dyn.plt_ent_sz, "plt_ent_sz is zero");
    NM_ASSERT(dyn.plt_sz%dyn.plt_ent_sz == 0, ".rel.plt length is not a multiple of plt_ent_sz");
    NM_ASSERT((dyn.plt_is_rela ? sizeof(*dyn.plt.rela) : sizeof(*dyn.plt.rel)) == dyn.plt_ent_sz, "size mismatch (%lu != %lu)", dyn.plt_is_rela ? sizeof(*dyn.plt.rela) : sizeof(*dyn.plt.rel), dyn.plt_ent_sz);

    // parse the dynamic symbol table, resolve symbols to relocations, then GOT entries
    for (size_t i = 0; i < dyn.plt_sz/dyn.plt_ent_sz; i++) {
        // note: Rela is a superset of Rel
        ElfW(Rel) *rel = dyn.plt_is_rela
            ? (ElfW(Rel)*)(&dyn.plt.rela[i])
            : &dyn.plt.rel[i];
        NM_ASSERT(ELFW(R_TYPE)(rel->r_info) == R_JUMP_SLOT, "not a jump slot relocation (R_TYPE=%lu)", ELFW(R_TYPE)(rel->r_info));

        ElfW(Sym) *sym = &dyn.sym[ELFW(R_SYM)(rel->r_info)];
        const char *str = &dyn.str[sym->st_name];
        if (strcmp(str, symname))
            continue;

        void **gotoff = (void**)(lm->l_addr + rel->r_offset);
        NM_LOG("found symbol %s (gotoff=%p [mapped=%p])", str, (void*)(rel->r_offset), gotoff);

        NM_ASSERT(ELFW(ST_TYPE)(sym->st_info) != STT_GNU_IFUNC, "STT_GNU_IFUNC not implemented (gotoff=%p)", (void*)(rel->r_offset));
        NM_ASSERT(ELFW(ST_TYPE)(sym->st_info) == STT_FUNC, "not a function symbol (ST_TYPE=%d) (gotoff=%p)", ELFW(ST_TYPE)(sym->st_info), (void*)(rel->r_offset));
        NM_ASSERT(ELFW(ST_BIND)(sym->st_info) == STB_GLOBAL, "not a globally bound symbol (ST_BIND=%d) (gotoff=%p)", ELFW(ST_BIND)(sym->st_info), (void*)(rel->r_offset));

        // TODO: figure out why directly getting the offset from the GOT was broken on ARM, but not x86
        NM_LOG("ensuring the symbol is loaded");
        void *orig = dlsym(handle, symname);
        NM_ASSERT(orig, "could not dlsym symbol");

        // remove memory protection (to bypass RELRO if it is enabled)
        // note: this doesn't seem to be used on the Kobo, but we might as well stay on the safe side (plus, I test this on my local machine too)
        // note: the only way to read the current memory protection is to parse /proc/maps, but there's no harm in unprotecting it again if it's not protected
        // note: we won't put it back afterwards, as if full RELRO (i.e. RTLD_NOW) wasn't enabled, it would cause segfaults when resolving symbols later on
        NM_LOG("removing memory protection");
        long pagesize = sysconf(_SC_PAGESIZE);
        NM_ASSERT(pagesize != -1, "could not get memory page size");
        void *gotpage = (void*)((size_t)(gotoff) & ~(pagesize-1));
        NM_ASSERT(!mprotect(gotpage, pagesize, PROT_READ|PROT_WRITE), "could not set memory protection of page %p containing %p to PROT_READ|PROT_WRITE", gotpage, gotoff);

        // replace the target offset
        NM_LOG("patching symbol");
        //void *orig = *gotoff;
        *gotoff = target;
        NM_LOG("successfully patched symbol %s (orig=%p, new=%p)", str, orig, target);
        NM_RETURN_OK(orig);
    }

    NM_RETURN_ERR("could not find symbol");
    #undef NM_err_ret
}
