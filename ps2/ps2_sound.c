/* ps2_sound.c: the game's own mixer (SDL/i_sound.c: effects and the OPL music)
   on maxsdk's stream (mx/snd.h). SDL calls the mixer from a thread of its own;
   here ps2_audio_pump does, once a frame, while the stream has room. */
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <mx/mx.h>
#include <mx/cpu.h>
#include <mx/snd.h>
#include <mx/timer.h>
#include "SDL.h"
#include "SDL_audio.h"

static void (*mix)(void *, Uint8 *, int);
static void *mix_data;
static int slice;     /* bytes the mixer makes at a time */
static int playing;
static Uint8 buf[8192] __attribute__((aligned(64)));

int SDL_OpenAudio(SDL_AudioSpec *want, SDL_AudioSpec *got) {
  (void)got;
  if (mx_snd_init() < 0 || mx_snd_stream_format((u32)want->freq, 16, want->channels) < 0)
    return -1;
  mix = want->callback, mix_data = want->userdata;
  slice = want->samples * want->channels * 2;
  if (slice > (int)sizeof buf)
    slice = sizeof buf;
  return 0;
}

void SDL_PauseAudio(int pause) { playing = !pause; }

void SDL_CloseAudio(void) {
  if (mix)
    mx_snd_stream_stop();
  mix = NULL, playing = 0;
}

/* for the report at the end of a test run */
static u32 slices, peak, mix_us, mix_us_max;

void ps2_audio_pump(void) {
  if (!playing || !mix)
    return;
  while (mx_snd_stream_room() >= (u32)slice) {
    u32 t = mx_count();
    memset(buf, 0, (size_t)slice);
    mix(mix_data, buf, slice);
    u32 us = (mx_count() - t) / (MX_EE_HZ / 1000000u);
    mix_us += us;
    if (us > mix_us_max)
      mix_us_max = us;
    for (int i = 0; i < slice / 2; i++) {
      int v = ((short *)buf)[i];
      if (v < 0)
        v = -v;
      if ((u32)v > peak)
        peak = (u32)v;
    }
    slices++;
    mx_snd_stream_queue(buf, (u32)slice);
  }
}

void ps2_audio_report(void) {
  mx_printf("ps2: audio: %u slices of %d bytes, peak %u, mixing %u us a slice, longest %u\n", slices, slice,
            peak, slices ? mix_us / slices : 0, mix_us_max);
}

/* the emulated PC speaker (snd_pcspeaker) is not built */
int PCSound_Init(void *callback) { (void)callback; return 0; }
void PCSound_Mix_Callback(void *udata, Uint8 *stream, int len) { (void)udata, (void)stream, (void)len; }
