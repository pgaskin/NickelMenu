#pragma once
#include <cstdint>
#include <cstddef>

namespace nm {
struct Spec {
    const char *library;
    const char *build_id; // exactly 40 lowercase hex digits
    const char *symbol;
    uintptr_t symbol_value; // includes the Thumb bit; zero means discover (fixture only)
    uintptr_t slot_offset; // zero means discover (fixture only)
};
struct Hook {
    uintptr_t *slot;
    uintptr_t original;
    uintptr_t replacement;
    uintptr_t page;
    size_t page_size;
    int protection;
};
// Returns nullptr on success; otherwise a static diagnostic. Uses a validated
// RELRO jump slot; no executable code is patched.
const char *prepare(const Spec &, uintptr_t replacement, Hook &);
const char *install(Hook &);
const char *restore(Hook &);
int protection(uintptr_t address, size_t length);
}
