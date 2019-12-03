#include "dsp.h"
#include "audio.h"
#include "util.h"
#include "kiss_fft.h"

#include <thread>
#include <math.h>
#include <iostream>
#include <assert.h>

Patch current_patch;
std::mutex patch_mutex;

constexpr uint32_t NUM_CHANNELS = 2;

constexpr uint32_t SAMPLE_RATE = 44100;

uint32_t current_delay_line_length = 44100;

// TODO: Right now it's just luck that this happens to be the same length as a jack frame
constexpr uint32_t BUFFER_LENGTH = 1024;


sample_t delay_output_buf[NUM_CHANNELS][BUFFER_LENGTH];

constexpr uint32_t PV_BLOCK_SIZE = 1024;
uint32_t hop_size = PV_BLOCK_SIZE / 4;

sample_t pv_output_buf[NUM_CHANNELS][BUFFER_LENGTH];


sample_t hanw[PV_BLOCK_SIZE];

struct Routings
{
    sample_t *pv_input[2] = {};
    sample_t *delay_input[2] = {};
    sample_t *delay_fb_input[2] = {};
};

kiss_fft_cfg kiss_fwd_cfg;
kiss_fft_cfg kiss_bwd_cfg;

static void pv_process(AudioData *audio_data, Routings routings, Patch patch)
{
    constexpr uint32_t PV_INPUT_BUF_LEN = 2 * PV_BLOCK_SIZE;
    static sample_t pv_input_buf[NUM_CHANNELS][PV_INPUT_BUF_LEN];

    // We have to copy our fft input into a new buffer with imaginary values (dumb)
    // TODO: I think KISS FFT provides a real-input FFT in the tools
    static kiss_fft_cpx fft_buf[NUM_CHANNELS][PV_BLOCK_SIZE];

    static kiss_fft_cpx freqs[NUM_CHANNELS][PV_BLOCK_SIZE];

    // Give some extra room for pitch shifting
    static sample_t partial_windows[NUM_CHANNELS][PV_BLOCK_SIZE * 2];

    static float last_phase[NUM_CHANNELS][PV_BLOCK_SIZE];
    static float adjusted_phase[NUM_CHANNELS][PV_BLOCK_SIZE];

    if (routings.pv_input[0] && routings.pv_input[1])
    {
        for (uint32_t c = 0; c < NUM_CHANNELS; ++c)
        {
            // Make room for the next block of data to get written in
            memcpy(pv_input_buf[c], pv_input_buf[c] + BUFFER_LENGTH, BUFFER_LENGTH * sizeof(sample_t));

            // Copy in the next block of data
            memcpy(pv_input_buf[c] + BUFFER_LENGTH, routings.pv_input[c], BUFFER_LENGTH * sizeof(sample_t));

            // Copy remaining partial windows into output buffer (to be overlapped)
            memcpy(pv_output_buf[c], partial_windows[c], BUFFER_LENGTH * sizeof(sample_t));
            memcpy(partial_windows[c], partial_windows[c] + BUFFER_LENGTH, BUFFER_LENGTH * sizeof(sample_t));
            memset(partial_windows[c] + BUFFER_LENGTH, 0, BUFFER_LENGTH * sizeof(sample_t));
        }

        for (uint32_t h = 0; h < PV_BLOCK_SIZE / hop_size; ++h)
        {
            uint32_t block_start = h * hop_size;

            for (uint32_t c = 0; c < NUM_CHANNELS; ++c)
            {
                // Copy into buffer to perform fft
                for (uint32_t i = 0; i < PV_BLOCK_SIZE; ++i)
                {
                    fft_buf[c][i].r = hanw[i] * pv_input_buf[c][block_start + i];
                    fft_buf[c][i].i = 0.0f;
                }

                // Circular shift
                for (uint32_t i = 0; i < PV_BLOCK_SIZE / 2; ++i)
                {
                    sample_t temp = fft_buf[c][i].r;
                    fft_buf[c][i].r = fft_buf[c][i + PV_BLOCK_SIZE / 2].r;
                    fft_buf[c][i + PV_BLOCK_SIZE / 2].r = temp;
                }

                kiss_fft(kiss_fwd_cfg, fft_buf[c], freqs[c]);
                // TODO: We don't actually need to deal with anything past the Nyquist freq
                // TODO: LP filter
                for (uint32_t k = 0; k < PV_BLOCK_SIZE; k++)
                {
                    float bin_phase = cplx_angle(freqs[c][k].r, freqs[c][k].i);
                    float bin_mag = cplx_mag(freqs[c][k].r, freqs[c][k].i);

                    float bin_freq = 2.0f * M_PI * k / PV_BLOCK_SIZE;
                    float target_phase = last_phase[c][k] + bin_freq * hop_size;
                    float phase_diff = bin_phase - target_phase;
                    float phase_increment = hop_size * bin_freq + princarg(phase_diff);
                    float instant_freq = phase_increment / hop_size;
                    float new_phase = adjusted_phase[c][k] + instant_freq * hop_size * patch.pitch;
                    last_phase[c][k] = bin_phase;
                    cplx_polar_to_rect(&freqs[c][k].r, &freqs[c][k].i, bin_mag, new_phase);
                    adjusted_phase[c][k] = new_phase;
                }
                kiss_fft(kiss_bwd_cfg, freqs[c], fft_buf[c]);

                // Circular shift
                for (uint32_t i = 0; i < PV_BLOCK_SIZE / 2; ++i)
                {
                    float FFT_SCALE_FACTOR = (2.0f * hop_size * patch.pitch) / (PV_BLOCK_SIZE * PV_BLOCK_SIZE);

                    sample_t temp = fft_buf[c][i].r;
                    fft_buf[c][i].r = fft_buf[c][i + PV_BLOCK_SIZE / 2].r * FFT_SCALE_FACTOR;
                    fft_buf[c][i + PV_BLOCK_SIZE / 2].r = temp * FFT_SCALE_FACTOR;
                }

                for (uint32_t i = 0; i < PV_BLOCK_SIZE - block_start; ++i)
                {
                    // We're resampling in here
                    float sample_pos = i * patch.pitch;
                    uint32_t sample_idx = (uint32_t)sample_pos;
                    uint32_t next_sample_idx = sample_idx + 1;
                    float t = sample_pos - sample_idx;

                    if (next_sample_idx < PV_BLOCK_SIZE)
                    {
                        float han_sampled = lerp(hanw[sample_idx], hanw[next_sample_idx], t);
                        pv_output_buf[c][i + block_start] +=
                            han_sampled * lerp(fft_buf[c][sample_idx].r, fft_buf[c][next_sample_idx].r, t);
                    }
                    else if (next_sample_idx == PV_BLOCK_SIZE)
                    {
                        // Lerp towards zero at the end of the block
                        float han_sampled = lerp(hanw[sample_idx], 0.0f, t);
                        pv_output_buf[c][i + block_start] +=
                            han_sampled * lerp(fft_buf[c][sample_idx].r, 0.0f, t);
                    }
                }

                for (uint32_t i = PV_BLOCK_SIZE - block_start; i < 2 * PV_BLOCK_SIZE; ++i)
                {
                    // We're resampling in here
                    float sample_pos = i * patch.pitch;
                    uint32_t sample_idx = (uint32_t)sample_pos;
                    uint32_t next_sample_idx = sample_idx + 1;
                    float t = sample_pos - sample_idx;


                    if (next_sample_idx < PV_BLOCK_SIZE)
                    {
                        float han_sampled = lerp(hanw[sample_idx], hanw[next_sample_idx], t);
                        partial_windows[c][i - PV_BLOCK_SIZE + block_start] += 
                            han_sampled * lerp(fft_buf[c][sample_idx].r, fft_buf[c][next_sample_idx].r, t);
                    }
                    else if (next_sample_idx == PV_BLOCK_SIZE)
                    {
                        // Lerp towards zero at the end of the block
                        float han_sampled = lerp(hanw[sample_idx], 0.0f, t);
                        partial_windows[c][i - PV_BLOCK_SIZE + block_start] += 
                            han_sampled * lerp(fft_buf[c][sample_idx].r, 0.0f, t);
                    }
                }
            }
        }
    }
}

float delay_modulate(Patch patch)
{
    static float t = 0.0f;
    float output = sinf(t);

    t += patch.delay_mod_freq / SAMPLE_RATE;
    if(t >= 2 * M_PI) t = 0.0f;
    return patch.delay_mod_amplitude * powf(2.0f, output);
}


static void tape_delay_process(AudioData *audio_data, Routings routings, Patch patch)
{
    constexpr uint32_t DELAY_LINE_LENGTH = 44100;

    static sample_t left_delay_line[DELAY_LINE_LENGTH];
    static sample_t right_delay_line[DELAY_LINE_LENGTH];

    static float last_delay_time = patch.delay_time;
    static float last_write_head;
    static float write_head;

    for (uint32_t i = 0; i < BUFFER_LENGTH; ++i)
    {
        // Calculate current delay time
        float current_delay_time;
        {
            float t = (float)i / BUFFER_LENGTH;
            current_delay_time = lerp(last_delay_time, patch.delay_time, t);
        }

        // BE CAREFUL: delay_modulate uses a static variable for time, so do this once for both channels
        current_delay_time *= delay_modulate(patch);

        float write_head_increment = current_delay_line_length / (current_delay_time * SAMPLE_RATE);

        // Determine next FF sample
        sample_t left_ff_sample;
        sample_t right_ff_sample;
        {
            float read_head = write_head + 1.0f;
            if (read_head >= DELAY_LINE_LENGTH) read_head -= DELAY_LINE_LENGTH;

            uint32_t sample_idx = (uint32_t)read_head;
            uint32_t next_sample_idx = wrap(sample_idx + 1, DELAY_LINE_LENGTH);
            float t = read_head - sample_idx;
            left_ff_sample = lerp(left_delay_line[sample_idx], left_delay_line[next_sample_idx], t);
            right_ff_sample = lerp(right_delay_line[sample_idx], right_delay_line[next_sample_idx], t);
        }

        // write output sample
        {
            delay_output_buf[0][i] = left_ff_sample;
            delay_output_buf[1][i] = right_ff_sample;
        }

        // write to delay_line
        // LOSSLESS DELAY COMMENTED OUT
        /*
        {
            uint32_t index_to_write = ceilf(last_write_head);
            float write_head_unwrapped = last_write_head + write_head_increment;
            while (index_to_write < write_head_unwrapped)
            {
                float t = (index_to_write - last_write_head) / write_head_increment;
                left_delay_line[wrap(index_to_write, DELAY_LINE_LENGTH)] +=
                    lerp(0.0f, routings.delay_input[0][i], t);
                right_delay_line[wrap(index_to_write, DELAY_LINE_LENGTH)] +=
                    lerp(0.0f, routings.delay_input[1][i], t);
                ++index_to_write;
            }

            float next_write_head_unwrapped = write_head_unwrapped + write_head_increment;
            while (index_to_write < next_write_head_unwrapped)
            {
                float t = (index_to_write - write_head_unwrapped) / write_head_increment;
                left_delay_line[wrap(index_to_write, DELAY_LINE_LENGTH)] =
                    patch.fb_gain * left_delay_line[wrap(index_to_write, DELAY_LINE_LENGTH)]
                    + lerp(routings.delay_input[0][i], 0.0f, t);
                right_delay_line[wrap(index_to_write, DELAY_LINE_LENGTH)] =
                    patch.fb_gain * right_delay_line[wrap(index_to_write, DELAY_LINE_LENGTH)]
                    + lerp(routings.delay_input[1][i], 0.0f, t);
                ++index_to_write;
            }
        }
        */
        {
            uint32_t index_to_write = ceilf(last_write_head);
            float write_head_unwrapped = last_write_head + write_head_increment;
            while (index_to_write < write_head_unwrapped)
            {
                float t = (index_to_write - last_write_head) / write_head_increment;
                left_delay_line[wrap(index_to_write, DELAY_LINE_LENGTH)] +=
                    lerp(0.0f, routings.delay_input[0][i] + patch.fb_gain * routings.delay_fb_input[0][i], t);
                right_delay_line[wrap(index_to_write, DELAY_LINE_LENGTH)] +=
                    lerp(0.0f, routings.delay_input[1][i] + patch.fb_gain * routings.delay_fb_input[1][i], t);
                ++index_to_write;
            }

            float next_write_head_unwrapped = write_head_unwrapped + write_head_increment;
            while (index_to_write < next_write_head_unwrapped)
            {
                float t = (index_to_write - write_head_unwrapped) / write_head_increment;
                left_delay_line[wrap(index_to_write, DELAY_LINE_LENGTH)] =
                    lerp(routings.delay_input[0][i] + patch.fb_gain * routings.delay_fb_input[0][i], 0.0f, t);
                right_delay_line[wrap(index_to_write, DELAY_LINE_LENGTH)] =
                    lerp(routings.delay_input[1][i] + patch.fb_gain * routings.delay_fb_input[1][i], 0.0f, t);
                ++index_to_write;
            }
        }

        last_write_head = write_head;
        write_head += write_head_increment;
        if (write_head >= DELAY_LINE_LENGTH) write_head -= DELAY_LINE_LENGTH;

        last_delay_time = patch.delay_time;
    }
}

static void process_audio_frame(AudioData *audio_data)
{
    //int32_t target_delay = (current_patch.delay_samples + last_delay_samples) / 2;
    //for (int i = 0; i < BUFFER_LENGTH; ++i)
    //{
    //    float t = (float)i / BUFFER_LENGTH;
    //    float delay_samples_offset = lerp(0, target_delay - last_delay_samples, t);

    //    int32_t delay_samples_whole = last_delay_samples + (int32_t)delay_samples_offset;
    //    float delay_samples_fractional = delay_samples_offset - (int32_t)delay_samples_offset;

    //    uint32_t delay_index = modulo(delay_line_marker - delay_samples_whole, DELAY_LINE_LENGTH);
    //    uint32_t next_delay_index = modulo(delay_index + 1, DELAY_LINE_LENGTH);

    //    float left_delayed = lerp(left_delay_line[delay_index], left_delay_line[next_delay_index], delay_samples_fractional);
    //    float right_delayed = lerp(right_delay_line[delay_index], right_delay_line[next_delay_index], delay_samples_fractional);

    //    // update delay lines
    //    left_delay_line[delay_line_marker] = 
    //        audio_data->left_input_buffer[i]
    //        + current_patch.fb_gain * left_delayed;
    //    right_delay_line[delay_line_marker] = 
    //        audio_data->right_input_buffer[i]
    //        + current_patch.fb_gain * right_delayed;

    //    // prepare next output frame
    //    left_output_buffer[i] =
    //        current_patch.dry_gain * audio_data->left_input_buffer[i]
    //        + current_patch.ff_gain * left_delayed;
    //    right_output_buffer[i] =
    //        current_patch.dry_gain * audio_data->right_input_buffer[i]
    //        + current_patch.ff_gain * left_delayed;

    //    ++delay_line_marker;
    //    if (delay_line_marker == DELAY_LINE_LENGTH) delay_line_marker = 0;
    //}
    //last_delay_samples = current_patch.delay_samples;
    
    Routings routings;

    while (true)
    {
        // TODO: If we keep track of time we can figure out roughly how long to wait and save some cpu
        while (!audio_data->new_data)
        {
            timespec sleep_time;
            sleep_time.tv_sec = 0;
            sleep_time.tv_nsec = 10000; // 10 us
            nanosleep(&sleep_time, nullptr);
        }

        // Sample the current patch
        patch_mutex.lock();
        Patch patch = current_patch;
        patch_mutex.unlock();

        switch (patch.algorithm)
        {
            case RoutingAlgorithm::FF_PV:
                routings.delay_input[0] = audio_data->left_input_buffer;
                routings.delay_input[1] = audio_data->right_input_buffer;
                routings.delay_fb_input[0] = delay_output_buf[0];
                routings.delay_fb_input[1] = delay_output_buf[1];
                routings.pv_input[0] = delay_output_buf[0];
                routings.pv_input[1] = delay_output_buf[1];
                audio_data->left_output_buffer = pv_output_buf[0];
                audio_data->right_output_buffer = pv_output_buf[1];
                break;
            case RoutingAlgorithm::FB_PV: // not implemented yet
                routings.delay_input[0] = audio_data->left_input_buffer;
                routings.delay_input[1] = audio_data->right_input_buffer;
                routings.delay_fb_input[0] = pv_output_buf[0];
                routings.delay_fb_input[1] = pv_output_buf[1];
                routings.pv_input[0] = delay_output_buf[0];
                routings.pv_input[1] = delay_output_buf[1];
                audio_data->left_output_buffer = pv_output_buf[0];
                audio_data->right_output_buffer = pv_output_buf[1];
                break;
            case RoutingAlgorithm::NO_PV:
            default:
                routings.delay_input[0] = audio_data->left_input_buffer;
                routings.delay_input[1] = audio_data->right_input_buffer;
                routings.delay_fb_input[0] = delay_output_buf[0];
                routings.delay_fb_input[1] = delay_output_buf[1];
                routings.pv_input[0] = nullptr;
                routings.pv_input[1] = nullptr;
                audio_data->left_output_buffer = delay_output_buf[0];
                audio_data->right_output_buffer = delay_output_buf[1];
                break;
        }

        tape_delay_process(audio_data, routings, patch);
        pv_process(audio_data, routings, patch);
        
        // Now do dry/wet mixing in the output buffer
        for (uint32_t i = 0; i < BUFFER_LENGTH; ++i)
        {
            audio_data->left_output_buffer[i] =
                patch.ff_gain * audio_data->left_output_buffer[i]
                + patch.dry_gain * audio_data->left_input_buffer[i];
            audio_data->right_output_buffer[i] =
                patch.ff_gain * audio_data->right_output_buffer[i]
                + patch.dry_gain * audio_data->right_input_buffer[i];
        }

        audio_data->new_data = false;
    }
}

void han_init(sample_t *hanw, uint32_t len)
{
    for (uint32_t i = 0; i < len; ++i)
    {
        hanw[i] = 0.5f - 0.5f * cosf(2.0f * M_PI * i / len);
    }
}

void dsp_init()
{
    static AudioData audio_data;

    // TODO: these are currently only used for the first call of processing function
    static sample_t left_output_buffer[BUFFER_LENGTH];
    static sample_t right_output_buffer[BUFFER_LENGTH];

    static sample_t left_input_buffer[BUFFER_LENGTH];
    static sample_t right_input_buffer[BUFFER_LENGTH];

    audio_data.buf_size = BUFFER_LENGTH;
    audio_data.left_output_buffer = left_output_buffer;
    audio_data.right_output_buffer = right_output_buffer;
    audio_data.left_input_buffer = left_input_buffer;
    audio_data.right_input_buffer = right_input_buffer;
    audio_data.dsp_callback = process_audio_frame;

    std::thread(process_audio_frame, &audio_data).detach();

    init_audio(&audio_data);

    kiss_fwd_cfg = kiss_fft_alloc(PV_BLOCK_SIZE, 0, nullptr, nullptr);
    kiss_bwd_cfg = kiss_fft_alloc(PV_BLOCK_SIZE, 1, nullptr, nullptr);

    han_init(hanw, ARRAY_LENGTH(hanw));
}
