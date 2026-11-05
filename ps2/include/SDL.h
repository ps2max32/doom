/* SDL.h: what is left of SDL outside the platform layer: types, the clock and
   no threads (there is one thread here; ps2/ has the rest) */
#ifndef PS2_SDL_H
#define PS2_SDL_H
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t Uint8;
typedef uint16_t Uint16;
typedef uint32_t Uint32;
typedef uint64_t Uint64;
typedef int16_t Sint16;
typedef int32_t Sint32;
typedef void SDL_mutex;
typedef void SDL_Thread;
#define SDL_VERSION_ATLEAST(a, b, c) 1
typedef struct SDL_Window SDL_Window;
typedef struct SDL_Renderer SDL_Renderer;
typedef struct SDL_PixelFormat {
  Uint32 format;
  Uint8 BitsPerPixel, BytesPerPixel;
  void *palette;
  Uint32 Rmask, Gmask, Bmask, Amask;
  Uint8 Rloss, Gloss, Bloss, Aloss, Rshift, Gshift, Bshift, Ashift;
} SDL_PixelFormat;
typedef struct SDL_Surface {
  Uint32 flags;
  SDL_PixelFormat *format;
  int w, h, pitch;
  void *pixels;
} SDL_Surface;
/* what would load a picture for a texture fails: the renderer goes without */
#define SDL_RWFromConstMem(p, n) ((void *)0)
#define SDL_LoadBMP_RW(rw, f) ((SDL_Surface *)0)
#define SDL_ConvertSurface(s, f, g) ((SDL_Surface *)0)
#define SDL_FreeSurface(s) ((void)0)
#define SDL_LockSurface(s) 0
#define SDL_UnlockSurface(s) ((void)0)
#define SDL_MUSTLOCK(s) 0
#define SDL_SWSURFACE 0
void (*mxglGetProcAddress(const char *name))(void);
#define SDL_GL_GetProcAddress(name) ((void *)mxglGetProcAddress(name))
enum { SDL_GL_BUFFER_SIZE, SDL_GL_DEPTH_SIZE, SDL_GL_MULTISAMPLEBUFFERS, SDL_GL_MULTISAMPLESAMPLES,
       SDL_GL_DOUBLEBUFFER, SDL_GL_RED_SIZE, SDL_GL_GREEN_SIZE, SDL_GL_BLUE_SIZE, SDL_GL_STENCIL_SIZE };
#define SDL_GL_SetAttribute(a, v) 0
void ps2_swap(void);
#define SDL_GL_SwapWindow(w) ps2_swap()
#define SDL_GetWindowGammaRamp(w, r, g, b) (-1)
#define SDL_SetWindowGammaRamp(w, r, g, b) (-1)
#define SDL_CalculateGammaRamp(g, r) ((void)0)
#define SDL_GL_GetAttribute(a, v) (*(v) = 0, 0)
#define SDL_GetPrefPath(o, a) strdup("/doom/")
#define SDL_free free
Uint32 SDL_GetTicks(void);
void SDL_Delay(Uint32 ms);
#define SDL_CreateMutex() ((SDL_mutex *)0)
#define SDL_DestroyMutex(m) ((void)0)
#define SDL_LockMutex(m) 0
#define SDL_UnlockMutex(m) 0
#define SDL_mutexP(m) 0
#define SDL_mutexV(m) 0
#endif
