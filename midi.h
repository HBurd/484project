#pragma once

namespace MidiCCIndex
{
    enum
    {
        DryGain = 1,
        FFGain,
        FBGain,
        DelayTime,
        ModFreq,
        ModInt,
        ModSrc,
        Algorithm,
        Pitch
    };
}

void midi_init();
void midi_loop();
