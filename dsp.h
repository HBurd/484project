#pragma once

#include "audio.h"

enum class ModulationType
{
    Sine,
    Triangle,
    //LFNoise,
    //HFNoise,
    Sawtooth,
};
    

struct Patch
{
    float fb_gain = 0.5f;
    float ff_gain = 0.7f;
    float dry_gain = 1.0f;

    ModulationType delay_mod_type = ModulationType::Sine;
    float delay_mod_amplitude = 0.1f;
    float delay_time = 0.5f;
};

extern Patch current_patch;

void dsp_init();
