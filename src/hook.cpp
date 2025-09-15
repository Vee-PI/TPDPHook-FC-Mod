#include "hook.h"
#include "log.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

// low level memory patching

uintptr_t RVA::base_ = 0;

bool patch_memory(void *dst, const void *src, std::size_t sz)
{
    DWORD oldprotect;

    if(VirtualProtect(dst, sz, PAGE_EXECUTE_READWRITE, &oldprotect))
    {
        memcpy(dst, src, sz);

        VirtualProtect(dst, sz, oldprotect, &oldprotect);

        return true;
    }

    LOG_FATAL() << L"VirtualProtect failed:\n"
                << std::hex << std::showbase
                << L"src: " << (uintptr_t)src << L"\n"
                << L"dst: " << (uintptr_t)dst << L"\n"
                << L"size: " << sz;

    return false; // unreachable
}

bool patch_call(void *mem, const void *dest_addr, bool force)
{
    if(!force)
    {
        unsigned int opcode = *(uint8_t*)mem;
        if(opcode != 0xE8u)
        {
            LOG_FATAL() << std::hex << std::showbase << L"Failed to patch call, unexpected opcode: " << opcode << L"\n"
                        << L"address: " << (uintptr_t)mem << L"\n"
                        << L"dst: " << (uintptr_t)dest_addr;
        }
    }

    uint8_t buf[] = { 0xE8, 0, 0, 0, 0 }; // call disp32
    auto disp = ((uintptr_t)dest_addr - (uintptr_t)mem) - sizeof(buf); // address of dest relative to the *next* instruction
    memcpy(&buf[1], &disp, 4);
    return patch_memory(mem, buf, sizeof(buf));
}

bool patch_call(void *mem, const void *dest_addr)
{
    return patch_call(mem, dest_addr, false);
}

bool write_call(void *mem, const void *dest_addr)
{
    return patch_call(mem, dest_addr, true);
}

bool patch_jump(void *mem, const void *dest_addr)
{
    uint8_t buf[] = { 0xE9, 0, 0, 0, 0 };
    auto disp = ((uintptr_t)dest_addr - (uintptr_t)mem) - sizeof(buf);
    memcpy(&buf[1], &disp, 4);
    return patch_memory(mem, buf, sizeof(buf));
}

void scan_and_replace(const void *orig_mem, const void *new_mem, std::size_t sz)
{
    auto code = RVA(0x1000).ptr<uint8_t*>();
    auto count = 0;

    std::size_t i = 0;
    while(i < (0x3c0600 - sz)) // loop the games code section
    {
        if(memcmp(&code[i], orig_mem, sz) == 0)
        {
            patch_memory(&code[i], new_mem, sz);
            ++count;
            i += sz;
            continue;
        }
        ++i;
    }

    LOG_TRACE() << L"Patched " << count << " blocks of size: " << sz;
}

void scan_and_replace_range(const void *orig_mem, const void *new_mem, std::size_t sz, void *range_start, std::size_t range_len)
{
    auto code = (uint8_t*)range_start;
    auto count = 0;

    std::size_t i = 0;
    while(i < range_len) // loop the games code section
    {
        if(memcmp(&code[i], orig_mem, sz) == 0)
        {
            patch_memory(&code[i], new_mem, sz);
            ++count;
            i += sz;
            continue;
        }
        ++i;
    }

    LOG_TRACE() << L"Patched " << count << " blocks of size: " << sz;
}

// only handles 4-byte displacements
// also technically unsafe since it doesn't actually disassemble the code
// so collisions/false positives are possible but unlikely
void scan_and_replace_call(const void *orig_addr, const void *new_addr, bool patch_jumps)
{
    auto code = RVA(0x1000).ptr<uint8_t*>();
    auto count = 0;
    auto oaddr = (uintptr_t)orig_addr;
    auto naddr = (uintptr_t)new_addr;

    std::size_t i = 0;
    while(i < (0x3c0600 - 5)) // loop the games code section
    {
        auto opcode = code[i];
        if((opcode == 0xe8) || (opcode == 0xe9))
        {
            if((uintptr_t)(&code[i + 5] + *(uintptr_t*)&code[i + 1]) == oaddr)
            {
                if(opcode == 0xe9)
                {
                    if(!patch_jumps)
                    {
                        LOG_TRACE() << L"Possible indirect call from " << std::hex << std::showbase << (uintptr_t)&code[i] << L" to " << oaddr;
                        continue;
                    }
                    LOG_TRACE() << L"Patching indirect call from " << std::hex << std::showbase << (uintptr_t)&code[i] << L" to " << oaddr;
                }
                auto disp = naddr - (uintptr_t)&code[i + 5];
                patch_memory(&code[i + 1], &disp, 4);
                ++count;
                i += 5;
                continue;
            }
        }
        ++i;
    }

    LOG_TRACE() << L"Patched " << count << " calls to: " << std::hex << std::showbase << oaddr;
}
