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
    };
}

void midi_init();
void midi_loop();
