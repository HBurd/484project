#include "audio.h"
#include "util.h"
#include <assert.h>
#include <string.h>

static int jack_process_callback(jack_nframes_t nframes, void *arg)
{
    AudioData *jack_data = (AudioData*)arg;

    // Check if the client is done processing
    assert(!jack_data->new_data);

    assert(nframes == jack_data->buf_size);

    // Handle input
    sample_t *left_in = (sample_t*)jack_port_get_buffer(jack_data->left_in, nframes);
    memcpy(jack_data->left_input_buffer, left_in, sizeof(sample_t) * nframes);

    sample_t *right_in = (sample_t*)jack_port_get_buffer(jack_data->right_in, nframes);
    memcpy(jack_data->right_input_buffer, right_in, sizeof(sample_t) * nframes);

    // Handle output
    sample_t *left_out = (sample_t*)jack_port_get_buffer(jack_data->left_out, nframes);
    memcpy(left_out, jack_data->left_output_buffer, sizeof(sample_t) * nframes);

    sample_t *right_out = (sample_t*)jack_port_get_buffer(jack_data->right_out, nframes);
    memcpy(right_out, jack_data->right_output_buffer, sizeof(sample_t) * nframes);

    jack_data->new_data = true;

    return 0;
}

void init_audio(AudioData *jack_data)
{
    jack_data->client = jack_client_open(
        "Phase Vocoder",
        JackNullOption,
        nullptr);

    jack_data->fs = jack_get_sample_rate(jack_data->client);

    jack_data->left_in = jack_port_register(
        jack_data->client,
        "Lin",
        JACK_DEFAULT_AUDIO_TYPE,
        JackPortIsInput,
        0);

    jack_data->right_in = jack_port_register(
        jack_data->client,
        "Rin",
        JACK_DEFAULT_AUDIO_TYPE,
        JackPortIsInput,
        0);

    jack_data->left_out = jack_port_register(
        jack_data->client,
        "Lout",
        JACK_DEFAULT_AUDIO_TYPE,
        JackPortIsOutput,
        0);

    jack_data->right_out = jack_port_register(
        jack_data->client,
        "Rout",
        JACK_DEFAULT_AUDIO_TYPE,
        JackPortIsOutput,
        0);

    int err = jack_set_process_callback(
        jack_data->client,
        jack_process_callback,
        jack_data);

    assert(!err);

    jack_activate(jack_data->client);
}

void deinit_audio(AudioData *jack_data)
{
    jack_deactivate(jack_data->client);

    jack_client_close(jack_data->client);
}

void RingBuf::write(sample_t *data, uint32_t length)
{
    uint32_t written = 0;

    while (written < length)
    {
        uint32_t length_to_write = MIN(length - written, buffer_length - marker);
        memcpy(buffer + marker, data + written, length_to_write * sizeof(sample_t));

        written += length_to_write;
        marker += length_to_write;
        if (marker == buffer_length)
        {
            marker = 0;
        }
    }
}

void buffer_add(sample_t *acc, sample_t *operand, uint32_t length)
{
    for (uint32_t i = 0; i < length; ++i)
    {
        acc[i] += operand[i];
    }
}
