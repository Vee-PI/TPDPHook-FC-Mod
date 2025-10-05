#pragma once
#include "../typedefs.h"

enum class PlaybackType
{
    NORMAL = 0,
    BACKGROUND = 1,
    LOOPING = 2,
    LOOPING_BACKGROUND = BACKGROUND | LOOPING
};

// this is either PlaySoundMem or PlayStreamSoundMem from dxlib.
// they are functionally equivalent for our purposes.
// TopPositionFlag = play from beginning(?).
// Oddly enough, you actually want PlaybackType BACKGROUND for sfx
// and LOOPING_BACKGROUND for BGM
HOOKAPI int PlaySoundMem(int SoundHandle, PlaybackType PlayType, int TopPositionFlag);

// play a sound using the game's internal queue
// use this if you care about volume consistency
HOOKAPI void play_sfx(int id);

HOOKAPI int get_sfx_handle(int id);
