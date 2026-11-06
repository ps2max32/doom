# Doom (PrBoom+, with its OpenGL renderer) on maxsdk: the PS2 port.
#
#   make [OUT=<dir>] [FRAMES=n] [ARGS="-warp 1 1"]   $(OUT)/doom.elf
#
# prboom2/ is upstream, the changes under __PS2__; ps2/ has the platform layer
# in place of prboom2/src/SDL/.

HERE     := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
PS2MAX   ?= $(abspath $(HERE)/../..)
MAXSDK   ?= $(PS2MAX)/libs/maxsdk
MAXSDK_OUT ?= $(PS2MAX)/build/libs/maxsdk
MXGL     ?= $(PS2MAX)/libs/libgl
LIBOUT   := $(PS2MAX)/build/apps/doom
OUT      ?= $(LIBOUT)
MX_MEMORY ?= phys
# under mx-kernel, all in core (docs/kernel.md in maxsdk)
MX_KERNEL ?= 1
all: $(OUT)/doom.elf $(OUT)/prboom-plus.wad
include $(MXGL)/mk/mxgl.mk
include $(MAXSDK)/mk/maxsdk.mk
MX_CORE_OBJS = $(MXGL_OBJS)

PICOLIBC_SRC ?= $(PS2MAX)/libs/picolibc
PICOLIBC := $(LIBOUT)/picolibc/install
PICOLIBC_A := $(PICOLIBC)/lib/libc.a
LIBRT    = $(MAXSDK_OUT)/librt.a
LIBMXC_OBJS = $(OUT)/libc/glue.o $(MAXSDK_OUT)/libc/compat.c.o $(MAXSDK_OUT)/libc/atomic.c.o \
              $(MAXSDK_OUT)/libc/malloc.c.o
LIBC_LIBS = $(LIBMXC_OBJS) $(PICOLIBC_A) $(MX_LIB) $(LIBRT)

TARGET   := --target=mips64el -mcpu=r5900 -mabi=n32 -fno-pic -mno-abicalls -G0
OPT      ?= -O2
SRC      := $(HERE)/prboom2/src
P        := $(HERE)/ps2
DCFLAGS  := $(TARGET) $(OPT) -std=gnu99 -w -MMD -fno-common -ffunction-sections -fdata-sections \
            -fno-strict-aliasing -fsigned-char -ffast-math -fapprox-func -mvu0 \
            -nostdlibinc -isystem $(P)/include -isystem $(PICOLIBC)/include -D__PS2__ -D_GNU_SOURCE \
            -DNDEBUG -DHAVE_CONFIG_H -DGL_GLEXT_PROTOTYPES -include $(P)/ps2_math.h \
            -I$(P) -I$(SRC) -I$(SRC)/MUSIC -I$(OUT)/gen -I$(MXGL_INC) -I$(MAXSDK)/ee/include \
            $(MXGL_DEFS) $(EXTRA_CFLAGS)

COMMON   := am_map doomdef doomstat dstrings d_deh d_items d_main e6y f_finale f_finale2 f_wipe \
            g_game g_overflow hu_lib hu_stuff hu_tracers info i_glob i_pcsound lprintf \
            md5 m_argv m_bbox m_cheat m_menu m_misc m_io m_random p_ceilng p_checksum p_doors \
            p_enemy p_floor p_genlin p_inter p_lights p_map p_maputl p_mobj p_plats p_pspr p_saveg \
            p_setup p_sight p_spec p_switch p_telept p_tick p_user r_bsp r_data r_demo r_draw \
            r_filter r_fps r_main r_patch r_plane r_segs r_sky r_things sc_man sounds statdump \
            st_lib st_stuff s_advsound s_sound tables version v_video wi_stuff w_wad z_bmalloc \
            z_zone d_client w_memcache memio mus2mid \
            $(addprefix MUSIC/,dbopl opl opl_queue oplplayer midifile alsaplayer dumbplayer flplayer \
              madplayer portmidiplayer vorbisplayer)
GLSRC    := $(notdir $(basename $(wildcard $(SRC)/gl_*.c)))
PS2SRC   := ps2_video ps2_sound ps2_posix ps2_softfp
SDLSRC   := i_main i_system i_sound
OBJS     := $(patsubst %,$(OUT)/d/%.o,$(COMMON) $(GLSRC)) $(patsubst %,$(OUT)/ps2/%.o,$(PS2SRC)) \
            $(patsubst %,$(OUT)/s/%.o,$(SDLSRC))

CDEPS    := $(PICOLIBC_A) $(OUT)/gen/runcfg.h
$(OUT)/d/%.o: $(SRC)/%.c $(CDEPS)
	@mkdir -p $(dir $@)
	$(CC) $(DCFLAGS) -c $< -o $@
$(OUT)/s/%.o: $(SRC)/SDL/%.c $(CDEPS)
	@mkdir -p $(dir $@)
	$(CC) $(DCFLAGS) -Dmain=doom_main -c $< -o $@
$(OUT)/ps2/%.o: $(P)/%.c $(CDEPS)
	@mkdir -p $(dir $@)
	$(CC) $(DCFLAGS) -c $< -o $@

$(OUT)/libc/glue.o: $(MAXSDK)/ee/libc/glue.c $(PICOLIBC_A) $(HERE)/Makefile
	@mkdir -p $(dir $@)
	$(CC) $(filter-out -ffreestanding,$(CFLAGS)) -nostdlibinc -isystem $(PICOLIBC)/include \
	  -Dread=mx_glue_read -Dwrite=mx_glue_write -D_exit=mx_glue_exit -c $< -o $@

# FRAMES: end after that many frames with a picture (host:doom.tga); PLAY: walk by
# scripted pad input; STATS: libgl's counters on (or R3); ARGS: more of the command
# line; MODE: libgl's mode whatever the settings (1 NTSC, 2 PAL, 3 1080i, +256 beam
# racing, +512 24-bit)
RUNCFG   := $(OUT)/gen/runcfg.h
$(shell mkdir -p $(OUT)/gen; printf '#define PS2_FRAMES %s\n#define PS2_ARGS "%s"\n#define PS2_PLAY %s\n#define PS2_STATS %s\n#define PS2_MODE (%s)\n' \
  '$(or $(FRAMES),0)' '$(ARGS)' '$(or $(PLAY),0)' '$(or $(STATS),0)' '$(or $(MODE),-1)' > $(RUNCFG).new; \
  cmp -s $(RUNCFG).new $(RUNCFG) || mv $(RUNCFG).new $(RUNCFG); rm -f $(RUNCFG).new)

$(MXGL_OBJS): $(PICOLIBC_A)
$(OUT)/doom.elf: $(OBJS) $(MXGL_OBJS) $(HLSL_OBJS) $(MX_LIB) $(MX_CRT0) $(MX_LD) $(LIBC_LIBS)
	$(call link,$@,-z muldefs --error-limit=0 --defsym=__stack_size=0x100000 $(MX_CRT0) $(OBJS) $(MXGL_OBJS) $(HLSL_OBJS) $(LIBC_LIBS))

$(PICOLIBC_A):
	@mkdir -p $(LIBOUT)/picolibc
	sed "s|@LLVM@|$(LLVM)|g" $(MAXSDK)/ee/libc/cross-r5900.txt.in > $(LIBOUT)/picolibc/cross.txt
	meson setup --wipe $(LIBOUT)/picolibc/build-meson $(PICOLIBC_SRC) \
	  --cross-file $(LIBOUT)/picolibc/cross.txt --prefix=$(PICOLIBC) -Dmultilib=false \
	  -Dpicocrt=false -Dsemihost=false -Dtests=false -Dthread-local-storage=false \
	  -Dposix-io=true -Dspecsdir=none -Dio-long-long=true -Dbuildtype=release
	ninja -C $(LIBOUT)/picolibc/build-meson install

# prboom-plus.wad: the engine's own data, by upstream's tool on the host
DATA     := $(HERE)/prboom2/data
$(OUT)/rdatawad: $(wildcard $(DATA)/rd_*.c $(DATA)/rd_*.h)
	cc -O1 -w -I$(P) -DHAVE_CONFIG_H -o $@ $(wildcard $(DATA)/rd_*.c)
$(OUT)/prboom-plus.wad: $(OUT)/rdatawad $(HERE)/ps2/wadcmd.txt
	$(OUT)/rdatawad -I $(DATA) $$(cat $(HERE)/ps2/wadcmd.txt) -o $@

clean:
	rm -rf $(filter-out $(LIBOUT)/picolibc,$(wildcard $(OUT)/*))

.PHONY: all clean

-include $(shell find $(OUT) -name '*.d' -not -path '*/picolibc/*' 2>/dev/null)
