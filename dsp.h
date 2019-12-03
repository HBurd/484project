#pragma once

#include "audio.h"
#include <mutex>

enum class ModulationType
{
    Sine,
    Triangle,
    //LFNoise,
    //HFNoise,
    Sawtooth,
};

enum class RoutingAlgorithm : uint32_t
{
    NO_PV,
    FF_PV,
    FB_PV,
};

struct Patch
{
    float fb_gain = 0.0f;
    float ff_gain = 0.0f;
    float dry_gain = 1.0f;

    float delay_time = 0.5f;

    ModulationType delay_mod_type = ModulationType::Sine;
    float delay_mod_amplitude = 0.1f;
    float delay_mod_freq = 1.0f;        // in rad/s

    float pitch = 0.0f;

    RoutingAlgorithm algorithm = RoutingAlgorithm::NO_PV;
};

extern Patch current_patch;
extern std::mutex patch_mutex;

void dsp_init();
