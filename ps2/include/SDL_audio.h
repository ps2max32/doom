/* SDL_audio.h: the audio device of ps2_sound.c */
#ifndef PS2_SDL_AUDIO_H
#define PS2_SDL_AUDIO_H
#include "SDL.h"
#define AUDIO_S16LSB 0x8010
#define AUDIO_S16MSB 0x9010
#define SDL_INIT_AUDIO 0x10
typedef struct SDL_AudioSpec {
  int freq;
  Uint16 format;
  Uint8 channels, silence;
  Uint16 samples;
  void (*callback)(void *userdata, Uint8 *stream, int len);
  void *userdata;
} SDL_AudioSpec;
#define SDL_InitSubSystem(f) 0
#define SDL_GetError() "audsrv"
#define SDL_LockAudio() ((void)0)
#define SDL_UnlockAudio() ((void)0)
int SDL_OpenAudio(SDL_AudioSpec *want, SDL_AudioSpec *got);
void SDL_PauseAudio(int pause);
void SDL_CloseAudio(void);
#endif
