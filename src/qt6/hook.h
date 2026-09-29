#pragma once
#include <cstdint>
#include <cstddef>

namespace nm {
struct Spec {
    const char *library;
    // Optional firmware pin. Null/empty accepts the runtime build ID only after
    // all symbol, relocation, mapping, RELRO and destination checks succeed.
    const char *build_id;
    const char *symbol;
    uintptr_t symbol_value; // includes the Thumb bit; zero means discover
    uintptr_t slot_offset; // zero means discover
};
struct Hook {
    uintptr_t *slot;
    uintptr_t original;
    uintptr_t replacement;
    uintptr_t page;
    size_t page_size;
    int protection;
    char build_id[41];
    uintptr_t symbol_value;
    uintptr_t slot_offset;
};
// Returns nullptr on success; otherwise a static diagnostic. Uses a validated
// RELRO jump slot; no executable code is patched.
const char *prepare(const Spec &, uintptr_t replacement, Hook &);
const char *install(Hook &);
const char *restore(Hook &);
int protection(uintptr_t address, size_t length);
}
