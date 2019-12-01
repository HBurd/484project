#include "midi.h"
#include <assert.h>
#include <alsa/asoundlib.h>

#include <iostream>
#include "dsp.h"        // For adjusting current patch

snd_seq_t *handle;

void midi_init()
{
    int err;
    err = snd_seq_open(&handle, "default", SND_SEQ_OPEN_INPUT, 0);
    assert(err == 0);
    snd_seq_set_client_name(handle, "Phase Vocoder");
    snd_seq_create_simple_port(handle, "Parameter Control", SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE, SND_SEQ_PORT_TYPE_MIDI_GENERIC);
}

void midi_loop()
{
    snd_seq_event *event;
    while(true)
    {
        // TODO: error checking
        snd_seq_event_input(handle, &event);
        if (event && event->type == SND_SEQ_EVENT_CONTROLLER)
        {
            switch (event->data.control.param)
            {
                case MidiCCIndex::DryGain:
                    current_patch.dry_gain = event->data.control.value / 127.0f;
                    break;
                case MidiCCIndex::FFGain:
                    current_patch.ff_gain = event->data.control.value / 127.0f;
                    break;
                case MidiCCIndex::FBGain:
                    current_patch.fb_gain = event->data.control.value / 127.0f;
                    break;
                case MidiCCIndex::DelayTime:
                    current_patch.delay_time = event->data.control.value / 127.0f;
                    break;
            }
        }
    }
}
