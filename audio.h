#pragma once

#include <jack/jack.h>
#include <stdint.h>
#include <atomic>

typedef jack_default_audio_sample_t sample_t;

struct AudioData
{
    uint32_t fs;
    std::atomic_bool new_data;

    // buffers have to be set manually
    uint32_t buf_size = 0;
    sample_t *left_input_buffer;
    sample_t *right_input_buffer;
    sample_t *left_output_buffer;
    sample_t *right_output_buffer;
    
    void (*dsp_callback)(AudioData *);

    jack_client_t *client;
    jack_port_t *left_in;
    jack_port_t *right_in;
    jack_port_t *left_out;
    jack_port_t *right_out;
};

void init_audio(AudioData *audio_data);
void deinit_audio(AudioData *audio_data);
