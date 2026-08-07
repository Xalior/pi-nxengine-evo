#
# NXEngine-evo as a bootable Raspberry Pi kernel image.
#
# One board per invocation (RAPI_BOARD, default rpi5). The board selects
# three things at once: the circle-stdlib world the kernel is compiled and
# linked against, the shim archive built against that world, and — through
# the world's Config.mk — the CPU tuning and RASPPI value. Each board's
# newlib and libc++ are built for its own architecture, so an image is
# never portable between boards.
#
#   make                     the default board's image
#   make RAPI_BOARD=rpi3     that board's image
#   make clean-board         drop just this board's build/<board>/ tree
#
# Everything this invocation generates lands under build/<board>/: the
# fixed-name kernel objects, every engine object, and the final
# .elf/.map/.lst/.img. The object names are shared across boards but their
# contents are not (different -mcpu, different -DRASPPI, different world),
# so a flat build directory would silently relink one board's objects into
# another board's kernel — a corrupt image with no build error. A directory
# per board keeps all three isolated: no `make clean` between boards, and
# three boards can build at the same time without touching each other.
#
# NXEngine-evo's own source tree (../nxengine-evo) is never written into: it
# is read-only input, and every object it produces is redirected here.
#
# Prerequisite: the board's world and shim archive exist —
# `make -C ../circle-libsdl2 deps`.
#

include ../mk/toolchain.mk

BOARDS      = rpi3 rpi4 rpi5
RAPI_BOARD ?= rpi5

ROOT = ..
SHIM = $(ROOT)/circle-libsdl2
NX   = $(ROOT)/nxengine-evo

OBJDIR = build/$(RAPI_BOARD)

# Circle's own image names, which the Raspberry Pi firmware selects between
# from the model sections of config.txt: the Pi 3 world boots kernel8.img,
# the Pi 4 world kernel8-rpi4.img, the Pi 5 world kernel_2712.img.
IMAGE_rpi3 = kernel8
IMAGE_rpi4 = kernel8-rpi4
IMAGE_rpi5 = kernel_2712
IMAGE = $(IMAGE_$(RAPI_BOARD))

# Before the world is included, so an unknown board is named as an unknown
# board. After the include it arrives as a missing Config.mk under a directory
# whose name nobody recognises.
ifeq ($(filter $(RAPI_BOARD),$(BOARDS)),)
$(error unknown RAPI_BOARD '$(RAPI_BOARD)' — expected one of: $(BOARDS))
endif

# The board's circle-stdlib world. A `make deps` that has not been run, or has
# not finished, leaves this file absent; make would then report that it has no
# rule to make a Config.mk, which describes the symptom and not the cause.
# WHERE THE BUILT WORLD COMES FROM.
#
# The default is this game's own nested shim, so a clone of this repository
# alone builds and uses its own worlds and nothing needs setting. A parent
# that keeps ONE set of built worlds for several games — they are gigabytes
# each, and identical when the pin and the configuration match — points
# CIRCLE_WORLDS at them.
CIRCLE_WORLDS ?= $(SHIM)
WORLD = $(CIRCLE_WORLDS)/circle-stdlib-$(RAPI_BOARD)
ifeq ($(wildcard $(WORLD)/Config.mk),)
$(error the $(RAPI_BOARD) circle-stdlib world is not configured ($(WORLD)/Config.mk is missing) — run 'make deps-$(RAPI_BOARD)' at the repository root)
endif

include $(WORLD)/Config.mk

TARGET = $(OBJDIR)/$(IMAGE)

# Stated explicitly because the first rule this file sees comes from an
# included makefile, and that would otherwise decide the default goal.
.DEFAULT_GOAL := image

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------

# This project's own code: the kernel wrapper, the syscall layer, and the
# parts of SDL2 that circle-libsdl2 does not implement and this game needs.
# Each file says at its top what it is answering for.
HOST_SRCS = main.cpp kernel.cpp defaults.cpp circle_syscalls.cpp \
	nx_pngfuncs.cpp
HOST_OBJS = $(addprefix $(OBJDIR)/,$(HOST_SRCS:.cpp=.o))

# NXEngine-evo's source list, as its own CMakeLists.txt globs it for the `nx`
# target, with two differences:
#
#   src/extract/  is absent. It builds a separate program, nxextract, which
#                 turns a Cave Story installation into the engine's own data
#                 files. That is done once, on a desktop, before the card is
#                 written — see the README.
#   graphics/pngfuncs.cpp
#                 is absent, and nx_pngfuncs.cpp above answers for it. Upstream's
#                 copy needs libpng, which this machine does not have.
#
NX_SRCS = \
	caret.cpp console.cpp debug.cpp floattext.cpp game.cpp input.cpp  \
	inventory.cpp main.cpp map_system.cpp map.cpp niku.cpp object.cpp  \
	ObjManager.cpp p_arms.cpp player.cpp playerstats.cpp profile.cpp  \
	ResourceManager.cpp screeneffect.cpp settings.cpp slope.cpp stageboss.cpp  \
	stagedata.cpp statusbar.cpp trig.cpp tsc.cpp \
	TextBox/ItemImage.cpp TextBox/SaveSelect.cpp TextBox/StageSelect.cpp  \
	TextBox/TextBox.cpp TextBox/YesNoPrompt.cpp \
	autogen/AssignSprites.cpp \
	common/glob.cpp common/InitList.cpp common/misc.cpp common/myfnmatch.cpp \
	Utils/Logger.cpp \
	endgame/credits.cpp endgame/CredReader.cpp endgame/island.cpp  \
	endgame/misc.cpp \
	graphics/Font.cpp graphics/Renderer.cpp graphics/Sprites.cpp  \
	graphics/Surface.cpp graphics/Tileset.cpp graphics/zoom.cpp \
	intro/intro.cpp intro/title.cpp \
	pause/dialog.cpp pause/message.cpp pause/mods.cpp pause/objects.cpp  \
	pause/options.cpp pause/pause.cpp \
	siflib/sectSprites.cpp siflib/sectStringArray.cpp siflib/sif.cpp  \
	siflib/sifloader.cpp \
	sound/Ogg.cpp sound/Organya.cpp sound/Pixtone.cpp sound/SoundManager.cpp \
	i18n/minibidi.cpp i18n/translate.cpp \
	ai/ai.cpp ai/balrog_common.cpp ai/IrregularBBox.cpp \
	ai/almond/almond.cpp \
	ai/boss/balfrog.cpp ai/boss/ballos.cpp ai/boss/core.cpp  \
	ai/boss/heavypress.cpp ai/boss/ironhead.cpp ai/boss/omega.cpp  \
	ai/boss/sisters.cpp ai/boss/undead_core.cpp ai/boss/x.cpp \
	ai/egg/egg.cpp ai/egg/egg2.cpp ai/egg/igor.cpp \
	ai/final_battle/balcony.cpp ai/final_battle/doctor_common.cpp  \
	ai/final_battle/doctor_frenzied.cpp ai/final_battle/doctor.cpp  \
	ai/final_battle/final_misc.cpp ai/final_battle/misery.cpp  \
	ai/final_battle/sidekicks.cpp \
	ai/first_cave/first_cave.cpp \
	ai/hell/ballos_misc.cpp ai/hell/ballos_priest.cpp ai/hell/hell.cpp \
	ai/last_cave/last_cave.cpp \
	ai/maze/balrog_boss_missiles.cpp ai/maze/critter_purple.cpp  \
	ai/maze/gaudi.cpp ai/maze/labyrinth_m.cpp ai/maze/maze.cpp  \
	ai/maze/pooh_black.cpp \
	ai/npc/balrog.cpp ai/npc/curly_ai.cpp ai/npc/curly.cpp ai/npc/misery.cpp  \
	ai/npc/npcguest.cpp ai/npc/npcplayer.cpp ai/npc/npcregu.cpp \
	ai/oside/oside.cpp \
	ai/plantation/plantation.cpp \
	ai/sand/curly_boss.cpp ai/sand/puppy.cpp ai/sand/sand.cpp  \
	ai/sand/toroko_frenzied.cpp \
	ai/sym/smoke.cpp ai/sym/sym.cpp \
	ai/village/balrog_boss_running.cpp ai/village/ma_pignon.cpp  \
	ai/village/village.cpp \
	ai/weapons/blade.cpp ai/weapons/bubbler.cpp ai/weapons/fireball.cpp  \
	ai/weapons/missile.cpp ai/weapons/nemesis.cpp ai/weapons/polar_mgun.cpp  \
	ai/weapons/snake.cpp ai/weapons/spur.cpp ai/weapons/weapons.cpp  \
	ai/weapons/whimstar.cpp \
	ai/weed/balrog_boss_flying.cpp ai/weed/frenzied_mimiga.cpp  \
	ai/weed/weed.cpp

NX_OBJS = $(addprefix $(OBJDIR)/nx/,$(NX_SRCS:.cpp=.o))

OBJS = $(HOST_OBJS) $(NX_OBJS)

STANDARD = -std=gnu++17

# Stated, not inherited: this is Circle's own default, written down so the
# build's optimization level is a fact in the file rather than a forensic
# reconstruction.
OPTIMIZE = -O2

# Dependency files are written as a side effect of compiling rather than by
# rules of their own, and Circle's Rules.mk is told to keep its hands off
# (CHECK_DEPS) so its `-M -MG` .d rules are not defined for these objects.
#
# -MG lists a header make has no rule for as a prerequisite of the object,
# which stops the build; GNU make 3.81 stops with exit 2 and prints not one
# line saying why, which is why this build asks for 4.0 in mk/toolchain.mk. A
# .d left over from an earlier build naming a header that has since moved or
# been deleted stops it the same way, and is why "delete the board's object
# directory" kept being the cure.
#
# -MD writes the dependency file alongside the object it belongs to, so the
# two can never disagree about the flags that produced them, and -MP adds a
# rule with no recipe for every header named, so a header that disappears
# means a rebuild instead of a dead stop.
DEPS       = $(OBJS:.o=.d)
CHECK_DEPS = 0
DEPFLAGS   = -MD -MP

include $(CIRCLEHOME)/Rules.mk

# The world must be the multicore one circle-libsdl2 configures: the shim's
# core-split machinery cannot exist without it, and an object compiled
# against a single-core world links without complaint and then misbehaves.
# Every object here inherits the flag through Rules.mk's CPPFLAGS, so this
# check covers the whole build.
ifeq ($(findstring ARM_ALLOW_MULTI_CORE,$(DEFINE)),)
$(error the payload requires the multicore circle-stdlib world — build it with 'make -C ../circle-libsdl2 deps')
endif

# The engine writes `#include <SDL.h>`, `<SDL_image.h>` and `<SDL_mixer.h>`,
# the spellings a desktop sdl2-config include path provides, and the shim's
# SDL2/ directory answers all three. Nothing of this project's own goes ahead
# of it: a header here named after one of the library's would shadow it, and
# a cut-down copy that declares a narrower API — or the same call with a
# different signature — produces no error anywhere, because a header defines
# no symbols for the --whole-archive link below to catch. The engine's own
# root and deps/ are on the path because its CMakeLists.txt puts them there:
# the header-only libraries it vendors — nlohmann/json, utf8, spdlog — are
# included by bare name.
INCLUDE := -I $(SHIM)/include -I $(SHIM)/include/SDL2 \
	-I $(NX) -I $(NX)/deps $(CIRCLE_STDLIB_INCLUDES) $(INCLUDE)

# The engine asks the preprocessor which operating system it is on, and
# refuses to compile for one it does not recognise: ResourceManager.cpp ends
# its platform ladder with #error, and the directory reader picks its POSIX
# implementation the same way. This world is the POSIX one — newlib, with
# stat, opendir and readdir all present through Circle's glue — so it is
# named as such here rather than by editing the upstream tree.
DEFINE += -D__unix__

# The engine's logger is spdlog, which reads the local time zone offset out of
# struct tm's tm_gmtoff. Newlib has no such member — the machine has no time
# zone database and no zone to be in — so spdlog's own switch for a platform
# without one is set, and it prints "+??:??" where the offset would go.
DEFINE += -DSPDLOG_NO_TZ_OFFSET

# EVERYTHING THIS GAME TOUCHES LIVES IN ONE DIRECTORY ON THE CARD. A card
# carries several games, and two of them writing a settings file into the FAT
# root would each silently overwrite the other's. Under /games/ rather than at
# the root so a dozen of them do not sprawl across it.
#
# A build-time constant, not a boot switch: a card's layout is decided when the
# card is staged, and a game that could be relocated at boot would only mean
# two places to look for the same file.
#
# The engine asks SDL where it lives — SDL_GetBasePath for the resources it
# reads and SDL_GetPrefPath for saved games and settings — so both are
# answered with this, via SDL2Circle_DeclareBasePath (kernel.cpp), which
# tells the library once and lets it compose both answers from it. Upstream
# is never told about it and needs no patch: it asks, and it is told.
RAPI_GAME_DIR ?= /games/nxengine-evo
DEFINE        += -DRAPI_GAME_DIR='"$(RAPI_GAME_DIR)"'

# The shim comes in whole, every member, rather than only the members the
# link happens to need. An object file linked straight into the kernel beats
# an archive member of the same name and does it silently, so a stub in this
# directory left behind after the library implemented the real thing would
# quietly win — the game would keep calling the stub and nothing would say
# so. Pulling the archive in whole turns that into a duplicate-symbol error
# at link time, which is the only moment it can still be cheap to fix.
SHIM_LIB = $(SHIM)/libSDL2-$(RAPI_BOARD).a

LIBS = --whole-archive $(SHIM_LIB) --no-whole-archive \
	$(CIRCLE_STDLIB_LIBS)

# The shim archive is built in circle-libsdl2 and never here — it is a shared
# dependency, and a rule here would rebuild it at whatever board and crossing
# count this makefile happens to be running with. This rule exists only for the
# case where it is absent: it runs then, and only then, because a target with a
# recipe and no prerequisites is up to date as soon as the file exists.
$(SHIM_LIB):
	@echo "$@ is missing: the $(RAPI_BOARD) shim archive has not been built."
	@echo "Run: $(MAKE) -C $(SHIM) libSDL2-$(RAPI_BOARD).a BOARD=$(RAPI_BOARD)"
	@exit 1

# ---------------------------------------------------------------------------
# Calls redirected at link time
# ---------------------------------------------------------------------------
#
# The game runs on the application core, and newlib's Circle glue drives
# FatFs and the SD card straight from wherever it is called — which off core
# 0 is illegal. circle_syscalls.cpp puts the shim's any-core I/O service
# underneath the C library instead; these flags are what put it there.
#
# --wrap rather than redefinition, because redefinition cannot work here:
# the glue defines _open, _read, _write, _close, _lseek, _fstat, opendir,
# readdir, closedir, dup and more in a SINGLE object file. Redefining part
# of that set either collides at link time or, worse, leaves the originals
# linked in beside the replacements with no diagnostic. --wrap renames the
# references instead and leaves the vendored glue untouched — and it is what
# keeps __real_* reachable, which is how the service performs the real work
# once it has reached core 0.
#
# Every name here must also appear in circle_syscalls.cpp: a wrap with no
# __wrap_ definition is an undefined reference, so the link is the check.
WRAPPED_SYSCALLS = \
	_open _close _read _write _lseek _fstat _stat lstat _unlink _rename \
	_fcntl mkdir chdir getcwd dup dup2 \
	opendir readdir closedir rewinddir

# The link is driven by the cross ld directly (Circle's Rules.mk sets
# LD = <prefix>ld for GCC builds), so these are plain linker options, not
# compiler-driver -Wl, ones.
LDFLAGS += $(addprefix --wrap=,$(WRAPPED_SYSCALLS))

# ---------------------------------------------------------------------------
# Compilation
# ---------------------------------------------------------------------------

# This project's own objects, board-scoped. These more-specific rules win
# over Rules.mk's in-place %.o/%.d, so nothing is written beside the source.
$(OBJDIR)/%.o: %.cpp | $(OBJDIR)
	@echo "  CPP   $@"
	@$(CPP) $(CPPFLAGS) $(DEPFLAGS) -c -o $@ $<

# The engine's objects, mirroring its source tree under $(OBJDIR)/nx/. The
# source tree itself is the upstream submodule, and is never written to.
$(OBJDIR)/nx/%.o: $(NX)/src/%.cpp
	@mkdir -p $(dir $@)
	@echo "  CPP   $@"
	@$(CPP) $(CPPFLAGS) $(DEPFLAGS) -c -o $@ $<

# The engine's main() is the program's entry point on a desktop, but here
# main() belongs to the Circle kernel (main.cpp in this directory), which
# calls the engine once the board is up. Renaming the entry point through the
# preprocessor for this one translation unit keeps the upstream source
# untouched: the definition is renamed and the kernel calls the new name.
$(OBJDIR)/nx/main.o: $(NX)/src/main.cpp
	@mkdir -p $(dir $@)
	@echo "  CPP   $@ (entry point renamed)"
	@$(CPP) $(CPPFLAGS) $(DEPFLAGS) -Dmain=nx_main -c -o $@ $<

$(OBJDIR):
	@mkdir -p $(OBJDIR)

-include $(DEPS)

# The image link rule, from the shim: it links with a script that keeps the
# TLS sections adjacent, which binutils 2.44 and later require and which
# libc++'s threading support needs.
# The image link rule. sdl-app.mk lets the caller choose the linker script,
# so this build supplies its own: the shim's script with room made at image
# offset 0x800 for the patchable-defaults block. Everything else about the
# script — including the TLS-section adjacency binutils 2.44 and later
# require — is inherited from the shim's.
SDL_APP_LDSCRIPT = $(CURDIR)/nxengine-evo-defaults.ld
include $(SHIM)/sdl-app.mk

# The other half of the guarantee the linker script's ASSERTs begin: check
# the BYTES of the finished image, not just the link. A pre-boot writer
# refuses to stamp an image whose magic is not at 0x800, so an image that
# lost it would boot and then quietly ignore everything ever sent to it —
# including --rapi-perf and --rapi-debug-uart, which are how the board is
# measured and driven when nobody is at the bench. Better no image than that.
.PHONY: image
image: $(TARGET).img
	@if [ "`dd if=$(TARGET).img bs=4 skip=512 count=1 2>/dev/null`" = "PM8D" ]; \
	then \
		echo "  GATE  $(TARGET).img defaults block at 0x800: present"; \
	else \
		echo "  GATE  $(TARGET).img: defaults block ABSENT at 0x800 — image deleted"; \
		rm -f $(TARGET).img; \
		exit 1; \
	fi

# Board-scoped clean: drop just this board's tree, leaving the other two
# boards' images and objects untouched.
.PHONY: clean-board
clean-board:
	rm -rf $(OBJDIR)

# This board from nothing. An incremental build is a decision made from
# timestamps, and a timestamp says when a file was written rather than what is
# in it; anything read off the resulting image describes what the last build
# happened to put there. Take a measurement that has to be right off one of
# these.
.PHONY: rebuild
rebuild:
	+@$(NOT_DRY_RUN)
	@$(MAKE) clean-board RAPI_BOARD=$(RAPI_BOARD)
	@$(MAKE) image RAPI_BOARD=$(RAPI_BOARD)
