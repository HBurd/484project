#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <time.h>       // TODO: we are using this for a posix dependency (nanosleep)

#include "audio.h"
#include "util.h"

const uint32_t BUFFER_LENGTH = 1024;

sample_t left_delay_line[44100];
sample_t right_delay_line[44100];

sample_t left_output_buffer[BUFFER_LENGTH];
sample_t right_output_buffer[BUFFER_LENGTH];

sample_t left_input_buffer[BUFFER_LENGTH];
sample_t right_input_buffer[BUFFER_LENGTH];

int main()
{
    AudioData audio_data;
    init_audio(&audio_data);

    audio_data.buf_size = BUFFER_LENGTH;
    audio_data.left_output_buffer = left_output_buffer;
    audio_data.right_output_buffer = right_output_buffer;
    audio_data.left_input_buffer = left_input_buffer;
    audio_data.right_input_buffer = right_input_buffer;

    uint32_t delay_line_marker = 0;
    uint32_t delay_line_length = 44100;

    int32_t delay_samples = 10000;

    float fb_gain = 0.5f;
    float ff_gain = 0.7f;
    float dry_gain = 1.0f;

    while (true)
    {
        if (audio_data.new_data)
        {
            // update delay line
            for (int i = 0; i < BUFFER_LENGTH; ++i)
            {
                float left_delayed = left_delay_line[modulo(delay_line_marker - delay_samples, delay_line_length)];
                float right_delayed = right_delay_line[modulo(delay_line_marker - delay_samples, delay_line_length)];

                left_delay_line[delay_line_marker] = 
                    audio_data.left_input_buffer[i]
                    + fb_gain * left_delayed;

                right_delay_line[delay_line_marker] = 
                    audio_data.right_input_buffer[i]
                    + fb_gain * right_delayed;

                left_output_buffer[i] =
                    dry_gain * audio_data.left_input_buffer[i]
                    + ff_gain * left_delayed;

                right_output_buffer[i] =
                    dry_gain * audio_data.right_input_buffer[i]
                    + ff_gain * left_delayed;

                ++delay_line_marker;
                if (delay_line_marker == delay_line_length) delay_line_marker = 0;
            }

            audio_data.new_data = false;
        }
        timespec sleep_time;
        sleep_time.tv_sec = 0;
        sleep_time.tv_nsec = 10000;     // 10 us
        nanosleep(&sleep_time, nullptr);
    }

    deinit_audio(&audio_data);

    return 0;
}
