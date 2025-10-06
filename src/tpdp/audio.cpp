#include "audio.h"
#include "../hook.h"

HOOKAPI int PlaySoundMem(int SoundHandle, PlaybackType PlayType, int TopPositionFlag)
{
    auto func = RVA(0x20bda0).ptr<int(__cdecl*)(int, int, int)>();
    return func(SoundHandle, (int)PlayType, TopPositionFlag);
}

HOOKAPI void play_sfx(int id)
{
    auto pos = RVA(0x995240).ptr<uint32_t*>();
    auto buf = RVA(0x995648).ptr<uint8_t*>();
    if(*pos < 0x80)
    {
        buf[*pos] = (uint8_t)id;
        *pos += 1;
    }
    return;
}

HOOKAPI int get_sfx_handle(int id)
{
    auto buf = RVA(0x995248).ptr<uint32_t*>();
    return buf[id & 0xff];
}
