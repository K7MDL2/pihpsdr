/* Copyright (C)
* 2017 - John Melton, G0ORX/N6LYT
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*
*/
#ifndef _RECEIVER_H
#define _RECEIVER_H

#include <gtk/gtk.h>
#ifdef PORTAUDIO
#include "portaudio.h"
#endif
#ifdef ALSA
#include <alsa/asoundlib.h>
#endif
#ifdef PULSEAUDIO
#include <pulse/pulseaudio.h>
#include <pulse/simple.h>
#endif

enum _audio_t {
    STEREO=0,
    LEFT,
    RIGHT
};

typedef enum _audio_t audio_t;

typedef struct _receiver {
  gint id;
  GMutex mutex;
  GMutex display_mutex;

  gint ddc;
  gint adc;

  gdouble volume;

  gint agc;
  gdouble agc_gain;
  gdouble agc_slope;
  gdouble agc_hang_threshold;
  gdouble agc_hang;
  gdouble agc_thresh;
  gint fps;
  gint displaying;
  audio_t audio_channel;
  gint sample_rate;
  gint buffer_size;
  gint fft_size;
  gint pixels;
  gint samples;
  gint output_samples;
  gdouble *iq_input_buffer;
  gdouble *audio_output_buffer;
  gint audio_buffer_size;
  gint audio_index;
  guint32 audio_sequence;
  gfloat *pixel_samples;
  gint display_panadapter;
  gint display_waterfall;
  gint update_timer_id;
  gdouble meter;

  gdouble hz_per_pixel;

  gint dither;
  gint random;
  gint preamp;

  gint nb;
  gint nb2;
  gint nr;
  gint nr2;
  gint anf;
  gint snb;

  int nr_agc;
  int nr2_gain_method;
  int nr2_npe_method;
  int nr2_ae;


  gint alex_antenna;
  gint alex_attenuation;

  gint filter_low;
  gint filter_high;

  gint width;
  gint height;

  GtkWidget *panel;
  GtkWidget *panadapter;
  GtkWidget *waterfall;

  gint panadapter_low;
  gint panadapter_high;
  gint panadapter_step;

  gint waterfall_low;
  gint waterfall_high;
  gint waterfall_automatic;
  cairo_surface_t *panadapter_surface;
  GdkPixbuf *pixbuf;
  gint local_audio;
  gint mute_when_not_active;
  gint audio_device;
  gchar *audio_name;
#ifdef PORTAUDIO
  PaStream *playstream;
  gint local_audio_buffer_inpt;    // pointer in audio ring-buffer
  gint local_audio_buffer_outpt;   // pointer in audio ring-buffer
  float *local_audio_buffer;
#endif
#ifdef ALSA
  snd_pcm_t *playback_handle;
  snd_pcm_format_t local_audio_format;
  void *local_audio_buffer;        // different formats possible, so void*
#endif
#ifdef PULSEAUDIO
  pa_simple *playstream;
  gboolean output_started;
  float *local_audio_buffer;
#endif
  gint local_audio_buffer_size;
  gint local_audio_buffer_offset;
  GMutex local_audio_mutex;

  gint low_latency;

  gint squelch_enable;
  gdouble squelch;

  gint deviation;

  gint64 waterfall_frequency;
  gint waterfall_sample_rate;
  gint waterfall_pan;
  gint waterfall_zoom;

  gint mute_radio;

  gdouble *buffer;
  void *resampler;
  gdouble *resample_buffer;
  gint resample_buffer_size;

  gint fexchange_errors;

  gint zoom;
  gint pan;

  gint x;
  gint y;

  // two variables that implement the new
  // "mute first RX IQ samples after TX/RX transition"
  // feature that is relevant for HermesLite-II and STEMlab
  // (and possibly some other radios)
  //
  guint txrxcount;
  guint txrxmax;
} RECEIVER;

extern RECEIVER *create_pure_signal_receiver(int id, int buffer_size,int sample_rate,int pixels);
extern RECEIVER *create_receiver(int id, int buffer_size, int fft_size, int pixels, int fps, int width, int height);
extern void receiver_change_sample_rate(RECEIVER *rx,int sample_rate);
extern void receiver_change_adc(RECEIVER *rx,int adc);
extern void receiver_frequency_changed(RECEIVER *rx);
extern void receiver_mode_changed(RECEIVER *rx);
extern void receiver_filter_changed(RECEIVER *rx);
extern void receiver_vfo_changed(RECEIVER *rx);
extern void receiver_change_zoom(RECEIVER *rx,double zoom);
extern void receiver_change_pan(RECEIVER *rx,double pan);

extern void set_mode(RECEIVER* rx,int m);
extern void set_filter(RECEIVER *rx);
extern void set_agc(RECEIVER *rx, int agc);
extern void set_offset(RECEIVER *rx, long long offset);
extern void set_deviation(RECEIVER *rx);

extern void add_iq_samples(RECEIVER *rx, double i_sample,double q_sample);
extern void add_div_iq_samples(RECEIVER *rx, double i0,double q0, double i1, double q1);

extern void reconfigure_receiver(RECEIVER *rx,int height);

extern void receiver_save_state(RECEIVER *rx);

extern gboolean receiver_button_press_event(GtkWidget *widget, GdkEventButton *event, gpointer data);
extern gboolean receiver_button_release_event(GtkWidget *widget, GdkEventButton *event, gpointer data);
extern gboolean receiver_motion_notify_event(GtkWidget *widget, GdkEventMotion *event, gpointer data);
extern gboolean receiver_scroll_event(GtkWidget *widget, GdkEventScroll *event, gpointer data);

extern void set_displaying(RECEIVER *rx,int state);

extern void receiver_restore_state(RECEIVER *rx);

extern void receiver_set_active(RECEIVER *rx);

#ifdef CLIENT_SERVER
extern void receiver_create_remote(RECEIVER *rx);
extern void receiver_remote_update_display(RECEIVER *rx);
#endif

#endif
