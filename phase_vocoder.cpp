#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <iostream>

#include "dsp.h"
#include "midi.h"
#include "util.h"

/*

static void update_dry_gain(GtkAdjustment *adjustment, gpointer user_data)
{
    current_patch.dry_gain = gtk_adjustment_get_value(adjustment);
}

static void update_delay_time(GtkAdjustment *adjustment, gpointer user_data)
{
    current_patch.delay_samples = (int32_t)gtk_adjustment_get_value(adjustment);
}

static void update_ff_gain(GtkAdjustment *adjustment, gpointer user_data)
{
    current_patch.ff_gain = gtk_adjustment_get_value(adjustment);
}

static void update_fb_gain(GtkAdjustment *adjustment, gpointer user_data)
{
    current_patch.fb_gain = gtk_adjustment_get_value(adjustment);
}

// TODO: I hate that I have to do this, and it inspires little faith in gtk's ability
// to cleanly solve problems. But I'll stick with this for now until I can find a better
// UI solution.

void add_new_slider(GtkWidget *slider_box,
                    const char *label,
                    float low_bound,
                    float high_bound,
                    float initial,
                    void (*callback)(GtkAdjustment*, gpointer))
{
    GtkWidget *label_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_container_add(GTK_CONTAINER(slider_box), label_box);

    GtkWidget *dry_label = gtk_label_new(label);
    gtk_container_add(GTK_CONTAINER(label_box), dry_label);

    GtkAdjustment *adjustment = gtk_adjustment_new(initial, low_bound, high_bound, 0.01, 0.01, 0.01);
    g_signal_connect (adjustment, "value-changed", G_CALLBACK (callback), NULL);
    GtkWidget *dry_slider = gtk_scale_new(GTK_ORIENTATION_HORIZONTAL, adjustment);
    gtk_container_add(GTK_CONTAINER(label_box), dry_slider);
}

static void activate(GtkApplication *gtk_app, gpointer user_data)
{
    midi_init();
    dsp_init();

    GtkWidget *window = gtk_application_window_new(gtk_app);
    gtk_window_set_title(GTK_WINDOW(window), "Phase Vocoder");
    gtk_window_set_default_size(GTK_WINDOW(window), 256, 256);

    GtkWidget *slider_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_container_add(GTK_CONTAINER(window), slider_box);

    add_new_slider(slider_box, "Dry", 0.0f, 1.0f, current_patch.dry_gain, update_dry_gain);
    add_new_slider(slider_box, "Wet", 0.0f, 1.0f, current_patch.ff_gain, update_ff_gain);
    add_new_slider(slider_box, "Feedback", -1.0f, 1.0f, current_patch.fb_gain, update_fb_gain);
    add_new_slider(slider_box, "Delay", 1.0f, 44100.0f, current_patch.delay_samples, update_delay_time);

    gtk_widget_show_all(window);


    // TODO: Not sure when this gets called now
    // deinit_audio(&audio_data);
}

int main(int argc, char **argv)
{
    GtkApplication *gtk_app;
    gtk_app = gtk_application_new("i.dont.like.gtk", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(gtk_app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(gtk_app), argc, argv);
    g_object_unref(gtk_app);

    return status;
}

*/

int main()
{
    midi_init();
    dsp_init();
    midi_loop();
}
