#pragma once

namespace MidiCCIndex
{
    enum
    {
        DryGain   = 1,
        FFGain    = 2,
        FBGain    = 3,
        DelayTime = 4,
        ModFreq   = 5,
        ModInt    = 6,
        ModSrc    = 7,
        Algorithm = 8,
        Pitch     = 9,
        HopSize   = 10,
    };
}

void midi_init();
void midi_loop();
