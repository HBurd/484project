#include "dsp.h"
#include "audio.h"
#include "util.h"

#include <thread>
#include <math.h>
#include <iostream>

Patch current_patch;

constexpr uint32_t SAMPLE_RATE = 44100;
constexpr uint32_t DELAY_LINE_LENGTH = 44100;

sample_t left_delay_line[DELAY_LINE_LENGTH];
sample_t right_delay_line[DELAY_LINE_LENGTH];

uint32_t current_delay_line_length = 44100;

// TODO: Right now it's just luck that this happens to be the same length as a jack frame
constexpr uint32_t BUFFER_LENGTH = 1024;

sample_t left_output_buffer[BUFFER_LENGTH];
sample_t right_output_buffer[BUFFER_LENGTH];

sample_t left_input_buffer[BUFFER_LENGTH];
sample_t right_input_buffer[BUFFER_LENGTH];

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
    
    float write_head = 0.0f;
    float last_write_head = 0.0f;
    float last_delay_time = current_patch.delay_time;

    while (true)
    {
        while (!audio_data->new_data)
        {
            timespec sleep_time;
            sleep_time.tv_sec = 0;
            sleep_time.tv_nsec = 10000; // 10 us
            nanosleep(&sleep_time, nullptr);
        }

        float target_delay_time = current_patch.delay_time;

        for (uint32_t i = 0; i < BUFFER_LENGTH; ++i)
        {
            // Calculate current delay time
            float current_delay_time;
            {
                float t = (float)i / BUFFER_LENGTH;
                current_delay_time = lerp(last_delay_time, current_delay_time, t);
            }

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
                audio_data->left_output_buffer[i] = 
                    current_patch.dry_gain * audio_data->left_input_buffer[i]
                    + current_patch.ff_gain * left_ff_sample;
                audio_data->right_output_buffer[i] =
                    current_patch.dry_gain * audio_data->right_input_buffer[i]
                    + current_patch.ff_gain * right_ff_sample;
            }

            // write to delay_line
            {
                uint32_t index_to_write = ceilf(last_write_head);
                float write_head_unwrapped = last_write_head + write_head_increment;
                while (index_to_write < write_head_unwrapped)
                {
                    float t = (index_to_write - last_write_head) / write_head_increment;
                    left_delay_line[wrap(index_to_write, DELAY_LINE_LENGTH)] +=
                        lerp(0.0f, audio_data->left_input_buffer[i], t);
                    right_delay_line[wrap(index_to_write, DELAY_LINE_LENGTH)] +=
                        lerp(0.0f, audio_data->right_input_buffer[i], t);
                    ++index_to_write;
                }

                float next_write_head_unwrapped = write_head_unwrapped + write_head_increment;
                while (index_to_write < next_write_head_unwrapped)
                {
                    float t = (index_to_write - write_head_unwrapped) / write_head_increment;
                    left_delay_line[wrap(index_to_write, DELAY_LINE_LENGTH)] =
                        current_patch.fb_gain * left_delay_line[wrap(index_to_write, DELAY_LINE_LENGTH)]
                        + lerp(audio_data->left_input_buffer[i], 0.0f, t);
                    right_delay_line[wrap(index_to_write, DELAY_LINE_LENGTH)] =
                        current_patch.fb_gain * right_delay_line[wrap(index_to_write, DELAY_LINE_LENGTH)]
                        + lerp(audio_data->right_input_buffer[i], 0.0f, t);
                    ++index_to_write;
                }
            }

            last_write_head = write_head;
            write_head += write_head_increment;
            if (write_head >= DELAY_LINE_LENGTH) write_head -= DELAY_LINE_LENGTH;

            last_delay_time = target_delay_time;
        }

        audio_data->new_data = false;
    }

}

void dsp_init()
{
    static AudioData audio_data;

    audio_data.buf_size = BUFFER_LENGTH;
    audio_data.left_output_buffer = left_output_buffer;
    audio_data.right_output_buffer = right_output_buffer;
    audio_data.left_input_buffer = left_input_buffer;
    audio_data.right_input_buffer = right_input_buffer;
    audio_data.dsp_callback = process_audio_frame;

    std::thread(process_audio_frame, &audio_data).detach();

    init_audio(&audio_data);
}
