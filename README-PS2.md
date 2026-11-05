# Doom on maxsdk (PS2)

PrBoom+ (GPL) with its OpenGL renderer, built for the PS2 with `libs/maxsdk` and `libs/libgl`
(clang, R5900) by `Makefile`. `prboom2/` is upstream, with the changes under `__PS2__`; `ps2/`
has the platform layer in place of `prboom2/src/SDL/`, except the game's own mixer
(`SDL/i_sound.c`), which runs on a small SDL audio shim.

```
make [FRAMES=n] [ARGS="-warp 1 -skill 3"]   $(OUT)/doom.elf and prboom-plus.wad (build/apps/doom)
```

The game reads its data from `/doom` on the host: put `prboom-plus.wad` and your own IWAD there
(`doom2.wad` by default; `ARGS="-iwad doom.wad"` for another). It fits in the 32 MB of a normal
PS2, without virtual memory.

`FRAMES=n` ends the program after n frames with a picture (`host:doom.tga`), the slowest second,
the fields dropped, the GS and beam racing counters and the audio's, then the test end marker.
`ARGS` is added to the command line. Other variables: `PLAY=1` walks by script, `STATS=1` shows
libgl's counters (L3 + R3 too), `MODE=n` sets libgl's mode (1 NTSC, 2 PAL, 3 1080i, +256 beam racing,
+512 24-bit).

## Picture

The default is the TV's mode with beam racing and 24-bit colour, a frame a field. The game's
defaults are set in `ps2_defaults` (`ps2/ps2_video.c`), after the config is read:

- 4:3 aspect and the status bar stretched to the screen: 640 x 448 and 640 x 512 fill a 4:3 TV.
- The sky is the sky dome: the standard one needs `glTexGen`, not in libgl.
- Fog based lighting: the GS's fog gives distance as in Doom, at no cost.
- No screen wipe: it copies the screen, which beam racing has not got.
- The stencil is used for flooding gaps of missing textures only where libgl has one
  (`GL_STENCIL_BITS`): not with beam racing, where such gaps show as without a stencil.

## Sound

Effects and music are mixed by the game at 22050 Hz, once a frame, into maxsdk's PCM stream
(`ps2/ps2_sound.c`). The music is the game's OPL synth, with the instruments in the IWAD's
`GENMIDI`; it takes about 8% of the EE. There is no sound during a level's load.

## Not done

- A video menu for the mode, beam racing and colour, as Quake and Duke have.
- 1080i needs another aspect and status bar.
- UMAPINFO (its parser is C++): some community WADs use it.
