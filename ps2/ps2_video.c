/* ps2_video.c: the screen, the clock and the pad of PrBoom+ on maxsdk, in
   place of SDL/i_video.c and SDL/i_joy.c. The renderer is PrBoom+'s OpenGL one
   on libgl; the picture the TV's mode. */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <mx/mx.h>
#include <mx/frame.h>
#include <mx/gs.h>
#include <mx/hires.h>
#include <mx/pad.h>
#include <mxgl.h>

#include "doomstat.h"
#include "doomdef.h"
#include "doomtype.h"
#include "v_video.h"
#include "r_draw.h"
#include "r_main.h"
#include "d_main.h"
#include "d_event.h"
#include "i_video.h"
#include "i_joy.h"
#include "m_argv.h"
#include "m_misc.h"
#include "st_stuff.h"
#include "am_map.h"
#include "lprintf.h"
#include "e6y.h"
#include "gl_struct.h"
#include "gl_intern.h"
#include "runcfg.h"

void ps2_audio_pump(void);
void ps2_audio_report(void);

void mx_exit(int status) __attribute__((noreturn));
void M_ChangeFOV(void);
void R_InitMeltRes(void);
void R_InitSpritesRes(void);
void R_InitPlanesRes(void);
void R_InitVisplanesRes(void);
void deh_changeCompTranslucency(void);

int gl_colorbuffer_bits = 16, gl_depthbuffer_bits = 16;
SDL_Window *sdl_window;
SDL_Renderer *sdl_renderer;
int use_fullscreen = 1, desired_fullscreen = 1, exclusive_fullscreen, render_vsync = 1;
int render_screen_multiply = 1, integer_scaling, vanilla_keymap;
int renderW, renderH, process_affinity_mask, process_priority;
dboolean window_focused = true;
const char *screen_resolutions_list[2] = {"640x448", NULL};
const char *screen_resolution = "640x448";
const char *sdl_video_window_pos = "center";

/* ---- the clock: the EE's counter, wrapped into 64 bits ------------------- */

static u64 cycles(void) {
  static u32 last, high;
  u32 c = mx_count();
  if (c < last)
    high++;
  last = c;
  return (u64)high << 32 | c;
}
Uint32 SDL_GetTicks(void) { return (Uint32)(cycles() / (MX_EE_HZ / 1000)); }
void SDL_Delay(Uint32 ms) {
  u64 end = cycles() + (u64)ms * (MX_EE_HZ / 1000);
  while (cycles() < end)
    ;
}

/* ---- the screen ---------------------------------------------------------- */

static int field_rate = 60;

static void start_mode(void) {
  int mode = MXGL_SD | MXGL_BEAM | MXGL_RGB24;
#if PS2_MODE >= 0
  mode = PS2_MODE;
#endif
  if (mxgl_set_mode(mode) < 0)
    mx_printf("ps2: beam racing: not enough memory\n");
  mx_frame_vsync(1); /* a frame a field */
  field_rate = mxgl_mode() == MXGL_PAL ? 50 : 60;
}

void I_PreInitGraphics(void) {
  mxgl_init();
  mxgl_show_stats = PS2_STATS;
  mxgl_texture_quality(MXGL_TEX_AUTO);
  start_mode();
}
void I_InitScreenResolution(void) {
  int w, h;
  mxgl_screen(&w, &h);
  V_InitMode(VID_MODEGL);
  SCREENWIDTH = w;
  SCREENHEIGHT = h;
  SCREENPITCH = SCREENWIDTH;
  V_DestroyUnusedTrueColorPalettes();
  V_FreeScreens();
  for (int i = 0; i < 5; i++) {
    if (i == 3)
      continue;
    screens[i].width = SCREENWIDTH;
    screens[i].height = SCREENHEIGHT;
    screens[i].byte_pitch = SCREENPITCH;
    screens[i].short_pitch = SCREENPITCH / V_GetModePixelDepth(VID_MODE16);
    screens[i].int_pitch = SCREENPITCH / V_GetModePixelDepth(VID_MODE32);
  }
  R_InitMeltRes();
  R_InitSpritesRes();
  R_InitBuffersRes();
  R_InitPlanesRes();
  R_InitVisplanesRes();
  lprintf(LO_INFO, "I_InitScreenResolution: %dx%d\n", SCREENWIDTH, SCREENHEIGHT);
}
void I_UpdateRenderSize(void) { renderW = SCREENWIDTH, renderH = SCREENHEIGHT; }
video_mode_t I_GetModeFromString(const char *s) { return VID_MODEGL; }
void I_UpdateVideoMode(void) {
  I_UpdateRenderSize();
  R_ExecuteSetViewSize();
  V_SetPalette(0);
  ST_SetResolution();
  AM_SetResolution();
  gld_Init(SCREENWIDTH, SCREENHEIGHT);
  M_ChangeFOV();
  deh_changeCompTranslucency();
}
void I_InitGraphics(void) {
  static int done;
  if (done)
    return;
  done = 1;
  I_UpdateVideoMode();
  mx_pad_init();
}
void I_ShutdownGraphics(void) {}
void I_SetWindowCaption(void) {}
void I_SetWindowIcon(void) {}
void I_SetPalette(int pal) {}
void I_UpdateNoBlit(void) {}
void I_StartFrame(void) {}
void UpdateGrab(void) {}
int I_ScreenShot(const char *fname) { return 0; }
unsigned char *I_GrabScreen(void) { return NULL; }
void I_FinishUpdate(void) { gld_Finish(); }

/* the end of a frame (gld_Finish): frames in each second's fields */
static int frames, sec_frames, fps_min = 1000, dropped;
static u32 sec_at;
void ps2_swap(void) {
  mxgl_end_frame();
  mxgl_frame_done();
  ps2_audio_pump();
  frames++;
  sec_frames++;
  u32 v = mx_gs.vsync;
  if (!sec_at)
    sec_at = v;
  else if (v - sec_at >= (u32)field_rate) {
    int f = sec_frames * field_rate / (int)(v - sec_at);
    mx_printf("ps2: frame %d: %d fps\n", frames, f);
    if (frames > 240) {
      if (f < fps_min)
        fps_min = f;
      dropped += (int)(v - sec_at) - sec_frames;
    }
    sec_at = v;
    sec_frames = 0;
  }
  if (PS2_FRAMES && frames >= PS2_FRAMES) {
    int r = mxgl_screenshot("doom.tga");
    mx_printf("ps2: %d frames, slowest second %d fps, %d fields dropped, picture %d\n", frames, fps_min, dropped, r);
    mx_printf("ps2: GS late %u, EE late %u fields\n", mx_gs.missed, mx_gs.starved);
    if (mxgl_mode_flags() & MXGL_BEAM)
      mx_printf("ps2: beam: %u late, %u held, %u too large, longest %u %u %u %u us\n", mx_hires_stats.late,
                mx_hires_stats.held, mx_hires_stats.rec_full, mx_hires_stats.pass_us[0],
                mx_hires_stats.pass_us[1], mx_hires_stats.pass_us[2], mx_hires_stats.pass_us[3]);
    ps2_audio_report();
    mx_exit(0);
  }
}

int gluBuild2DMipmaps(GLenum target, GLint fmt, GLsizei w, GLsizei h, GLenum format, GLenum type, const void *data) {
  glTexImage2D(target, 0, fmt, w, h, 0, format, type, data);
  return 0;
}

/* after the config: what this screen and libgl need */
extern int render_aspect, render_stretch_hud_default, render_stretch_hud, render_wipescreen, gl_skymode;
extern int snd_samplerate, use_experimental_music;
extern const char *snd_midiplayer;

void ps2_defaults(void) {
  render_aspect = 3; /* the TV's 640 x 448 or 640 x 512 fill a 4:3 screen */
  render_stretch_hud_default = render_stretch_hud = patch_stretch_full;
  gl_skymode = skytype_skydome;   /* the standard sky is glTexGen, not in libgl */
  render_wipescreen = 0;          /* a copy of the screen: not with beam racing */
  gl_lightmode_default = gl_lightmode_fogbased; /* the GS's fog: distance as in Doom, and free */
  /* the effects are 11025 Hz; the music is the OPL synth, from the WAD's GENMIDI */
  snd_samplerate = 22050;
  use_experimental_music = 1;
  snd_midiplayer = "opl2";
}

/* ---- the pad: keys and the mouse the game knows --------------------------- */

int usejoystick;
int joyleft, joyright, joyup, joydown;
void I_InitJoystick(void) {}
void I_PollJoystick(void) {}

static const struct {
  u32 bit;
  int key;
} pad_keys[] = {
    {MX_PAD_UP, KEYD_UPARROW},    {MX_PAD_DOWN, KEYD_DOWNARROW}, {MX_PAD_LEFT, KEYD_LEFTARROW},
    {MX_PAD_RIGHT, KEYD_RIGHTARROW}, {MX_PAD_R1, KEYD_RCTRL},    {MX_PAD_L1, ' '},
    {MX_PAD_CROSS, KEYD_ENTER},   {MX_PAD_CIRCLE, KEYD_BACKSPACE}, {MX_PAD_SQUARE, KEYD_RSHIFT},
    {MX_PAD_START, KEYD_ESCAPE},  {MX_PAD_SELECT, KEYD_TAB},     {MX_PAD_R2, KEYD_MWHEELUP},
    {MX_PAD_L2, KEYD_MWHEELDOWN},
};

static void key(int k, int down) {
  event_t e = {0};
  e.type = down ? ev_keydown : ev_keyup;
  e.data1 = k;
  D_PostEvent(&e);
}

/* the keys held by the test script */
static u32 script_buttons(int *turn) {
  *turn = 0;
#if PS2_PLAY == 2
  return frames > 300 && frames % 60 < 8 ? MX_PAD_CROSS : 0;
#elif PS2_PLAY
  int s = frames / 60;
  *turn = (s % 3 == 2) ? ((s / 3) & 1 ? 30 : -30) : 0;
  return MX_PAD_UP | (s % 4 == 3 ? MX_PAD_R1 : 0) | (s % 5 == 4 ? MX_PAD_L1 : 0);
#else
  return 0;
#endif
}

void I_StartTic(void) {
  static u32 last;
  static int stick_y, stick_x;
  int turn;
  u32 held = mx_pad_buttons(0) | script_buttons(&turn);
  for (unsigned i = 0; i < sizeof pad_keys / sizeof pad_keys[0]; i++)
    if ((held ^ last) & pad_keys[i].bit)
      key(pad_keys[i].key, (held & pad_keys[i].bit) != 0);
  last = held;
  u8 st[4];
  if (mx_pad_sticks(0, st)) {
    int rx = st[0] - 128, lx = st[2] - 128, ly = st[3] - 128;
    if (rx > 24 || rx < -24)
      turn += rx / 3;
    int y = ly < -48 ? -1 : ly > 48 ? 1 : 0, x = lx < -48 ? -1 : lx > 48 ? 1 : 0;
    if (y != stick_y) {
      if (stick_y)
        key(stick_y < 0 ? KEYD_UPARROW : KEYD_DOWNARROW, 0);
      if (y)
        key(y < 0 ? KEYD_UPARROW : KEYD_DOWNARROW, 1);
      stick_y = y;
    }
    if (x != stick_x) {
      if (stick_x)
        key(stick_x < 0 ? 'a' : 'd', 0);
      if (x)
        key(x < 0 ? 'a' : 'd', 1);
      stick_x = x;
    }
  }
  if (turn) {
    event_t e = {0};
    e.type = ev_mouse;
    e.data2 = turn * 8;
    D_PostEvent(&e);
  }
}

/* ---- main ----------------------------------------------------------------- */

int doom_main(int argc, char **argv);
int main(int argc, char **argv) {
  static char line[256] = "doom -iwad doom2.wad -nomouse " PS2_ARGS;
  static char *args[32];
  int n = 0;
  for (char *p = strtok(line, " "); p && n < 31; p = strtok(NULL, " "))
    args[n++] = p;
  chdir("/doom");
  setenv("HOME", "/doom", 1);
  setenv("DOOMWADDIR", "/doom", 1);
  return doom_main(n, args);
}

/* what is not here: video capture, UMAPINFO (a C++ parser, for some custom WADs) and GL
   that only optional features call */
#include "i_capture.h"
#include "umapinfo.h"
const char *cap_soundcommand = "", *cap_videocommand = "", *cap_muxcommand = "", *cap_tempfile1 = "", *cap_tempfile2 = "";
int cap_remove_tempfiles, cap_fps, cap_frac, cap_wipescreen, capturing_video;
void I_CapturePrep(const char *fn) {}
void I_CaptureFrame(void) {}
void I_CaptureFinish(void) {}
struct MapList Maps;
int ParseUMapInfo(const unsigned char *lump, size_t size, void (*err)(const char *fmt, ...)) { return 0; }
void *screen;
void glCopyTexSubImage2D(GLenum t, GLint l, GLint xo, GLint yo, GLint x, GLint y, GLsizei w, GLsizei h) {}
void glGetTexImage(GLenum t, GLint l, GLenum f, GLenum ty, void *p) {}
void glGetTexLevelParameteriv(GLenum t, GLint l, GLenum n, GLint *p) { *p = 0; }
void glTexGenf(GLenum c, GLenum n, GLfloat v) {}
void glTexGenfv(GLenum c, GLenum n, const GLfloat *v) {}
