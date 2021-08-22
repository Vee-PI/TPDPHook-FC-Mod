#pragma once
#include <cstdint>
#include <type_traits>
#include "typedefs.h"

// Class for converting Relative Virtual Addresses to arbitrary pointer types
class HOOKAPI RVA
{
private:
    static uintptr_t base_;
    void *val_;

public:
    RVA() = default;
    RVA(uintptr_t val) { val_ = reinterpret_cast<void*>(val + base_); }
    RVA(const RVA&) = default;
    RVA(RVA&&) = default;

    RVA& operator=(const RVA&) = default;
    RVA& operator=(RVA&&) = default;

    operator uintptr_t() const { return reinterpret_cast<uintptr_t>(val_); }
    operator void*() const { return val_; }
    operator const void*() const { return val_; }

    uintptr_t rva() const { return reinterpret_cast<uintptr_t>(val_) - base_; }

    template<typename T = void*>
    std::enable_if_t<std::is_pointer_v<T>, T> ptr() const { return reinterpret_cast<T>(val_); }

    static void base(uintptr_t base) { base_ = base; }
    static uintptr_t base() { return base_; }
};

// functions for writing to write-protected memory (such as the code or read-only data sections)
// and convenience functions for redirecting jumps/calls
HOOKAPI bool patch_memory(void *dst, const void *src, std::size_t sz);
HOOKAPI bool patch_call(void *mem, const void *dest_addr);
HOOKAPI bool write_call(void *mem, const void *dest_addr);
HOOKAPI bool patch_jump(void *mem, const void *dest_addr);

// careful with these
HOOKAPI void scan_and_replace(const void *orig_mem, const void *new_mem, std::size_t sz);
HOOKAPI void scan_and_replace_call(const void *orig_addr, const void *new_addr, bool patch_jumps = false);
