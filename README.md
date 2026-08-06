# pi-nxengine-evo

**NXEngine-evo — the Cave Story engine — running directly on a Raspberry Pi
with no operating system.** The board powers on and the game is what boots: no
Linux, no desktop, no launcher, and nothing else running beside it.

It builds for the Raspberry Pi 3, Pi 4 and Pi 5, all three from one source
tree.

## What this is

[NXEngine-evo](https://github.com/nxengine/nxengine-evo) is an ordinary SDL2
application. This repository is the thin layer that lets it run with nothing
underneath: a [Circle](https://github.com/rsta2/circle) kernel that brings the
board up, and [circle-libsdl2](https://github.com/Xalior/circle-libsdl2), an
SDL2 implementation built on Circle's bare-metal drivers.

The game's own source is not copied or modified here. It is a submodule,
pinned at an upstream commit, and the build reads it without ever writing to
it. Everything the engine needs that the SDL2 layer does not already provide
is added beside it, in `host/`.

Three processor cores are given separate work:

- **Core 0** owns the hardware. Circle's world lives here — interrupts, USB,
  the SD card, sound — and no other core touches a device.
- **Core 1** runs the game and nothing else.
- **Core 2** puts finished frames on the screen. The game draws at its own
  resolution and never learns the display's; the picture is scaled once, at
  the end, onto whatever the screen is really showing.

## Building

You need a Linux or macOS machine, GNU make, and the Arm GNU toolchain for
`aarch64-none-elf` (release 15.2.Rel1). Put its `bin` directory on your `PATH`,
or unpack it into `toolchains/` in this repository.

```sh
git clone --recursive https://github.com/Xalior/pi-nxengine-evo.git
cd pi-nxengine-evo
make deps       # long: builds newlib and libc++ from source, once per board
make kernels    # the three board images
make verify     # confirms each image exists and is not empty
```

`make deps` is the slow step, and it is slow once. It builds a complete C and
C++ world for each board, because each board's world is compiled for its own
processor.

The images land in `host/build/<board>/`:

| Board | Image |
|---|---|
| Pi 3 | `host/build/rpi3/kernel8.img` |
| Pi 4 | `host/build/rpi4/kernel8-rpi4.img` |
| Pi 5 | `host/build/rpi5/kernel_2712.img` |

## Putting it on a card

```sh
make card
```

That builds the card into `build/sd-card/` for you to copy onto FAT32 media.
It fetches the Raspberry Pi firmware at the revision Circle is built against
and checks every file against a hash, stages the three kernel images under the
names each board's firmware looks for, writes the boot configuration, and
copies the engine's own `data/` directory out of the upstream checkout. Given
a mounted FAT32 volume instead, `tools/mkcard /Volumes/YOUR-CARD` writes
straight to it.

The same repository state always produces the same card, and the script ends
by reading back what actually landed rather than trusting that the copies
worked.

The card ends up laid out like this:

| On the card | What it is |
|---|---|
| `config.txt`, `cmdline.txt` | boot configuration, read by the firmware |
| `kernel8.img`, `kernel8-rpi4.img`, `kernel_2712.img` | one image per board; the firmware picks its own |
| `data/` | everything the engine reads |
| `nxengine/` | settings, saved games and the log — created on the first run |

## The game's data files, which you must supply

**Cave Story's own files are not in this repository and are not on the card
the build makes.** They belong to the game's author, Daisuke "Pixel" Amaya,
and are not this project's to distribute. Without them the engine starts and
then stops, because there is no game to load.

They come out of the original freeware release of Cave Story, which Pixel
published at no charge and which is still distributed at no charge. Two
places to get it:

- **[The Cave Story Tribute Site](https://www.cavestory.org/)** — the
  long-running community site, which hosts the original Japanese release and
  the English translation by Aeon Genesis.
- **The Internet Archive**, which keeps copies of the same original release.

Do not use a purchased version. *Cave Story+* and the other paid re-releases
are separate commercial products with different data, and taking files out of
one is neither necessary nor permitted.

The engine does not read the original files directly. It reads files that
`nxextract` — a small program that is part of NXEngine-evo — produces from
`Doukutsu.exe`. Build and run that on your desktop machine, following
[NXEngine-evo's own build instructions](https://github.com/nxengine/nxengine-evo/wiki),
in the directory holding your copy of Cave Story. It writes a `data/`
directory. Copy the contents of that directory into the card's `data/`
directory, beside the files `make card` already put there.

## The fan pin in `cmdline.txt`

One card boots any of the three boards, so all three read the same
`cmdline.txt`. It carries `socmaxtemp=70`, the temperature in degrees Celsius
at which the board starts protecting itself by slowing the processor down.

Adding `gpiofanpin=<pin>` changes what happens at that temperature: the fan on
that pin is switched on and the processor is left at full speed, instead of
being slowed. Pin 45 is where a Raspberry Pi 5 Case Fan or Active Cooler is
wired. On a Pi 3 or a Pi 4 the pin depends on how you wired your own fan.

## What is built, and what is not yet proven

The three kernel images build and link, and the SD card the build stages is
complete apart from Cave Story's own files. **Nothing here has been run on a
board yet**, so treat everything below as what the code is written to do
rather than as what has been seen to happen.

Built and expected to work, because the SDL2 layer underneath it is proven by
other games on the same boards: fullscreen software rendering, HDMI audio,
USB keyboards and USB game controllers.

Two parts of the port were checked against a reference decoder on a desktop
machine, byte for byte, rather than left to the first boot to discover: the
PNG reader in `host/sdl2_image.cpp` on the engine's own font atlases, and the
bitmap reader in `host/sdl2_bmp.cpp` on the 24-bit and 8-bit images the
engine ships.

Known to be missing, and why:

- **Replacement soundtracks.** The optional Ogg Vorbis soundtracks need a
  Vorbis decoder, and there is none on this machine. Choosing one of those
  music directories in the options menu leaves the game silent; the original
  Organya soundtrack is synthesised by the engine itself and needs no decoder,
  so that is the one that plays.
- **Screenshots.** The finished picture is assembled on the presentation core
  and handed straight to the display, so there is nothing for the screenshot
  key to read back.
- **Rumble.** The engine drives force feedback through `SDL_Haptic`, which the
  SDL2 layer does not implement.
- **Rotated drawing.** `SDL_RenderCopyEx` here mirrors but does not rotate.
  The engine only ever asks it to mirror.

There is no GPU driver on bare metal, so everything is drawn by the processor.
That is the design rather than a limitation: it is what makes one build run
across three generations of board.

## What `host/` contains

The kernel is the smallest part of it. Most of `host/` is the difference
between what NXEngine-evo asks SDL for and what circle-libsdl2 provides, and
each file says at the top what it is answering for:

| File | What it is for |
|---|---|
| `kernel.cpp`, `main.cpp` | bring the board up, elect the cores, call the game |
| `circle_syscalls.cpp` | file access from a core that may not touch the card |
| `sdl2_bmp.cpp` | `SDL_LoadBMP`, which is how every piece of artwork is read |
| `sdl2_image.cpp` | `IMG_Load`, with a PNG reader and a DEFLATE decompressor, because there is no libpng or zlib here |
| `sdl2_mixer.cpp` | the SDL_mixer channel mixer the engine plays its sound through |
| `sdl2_audiocvt.cpp` | the sample-format and rate conversion the engine's sound synthesis needs |
| `sdl2_surface.cpp`, `sdl2_texture.cpp` | surfaces, colour keys, and drawing a sprite mirrored |
| `sdl2_paths.cpp` | where the game's files are, on a machine with no shell |
| `sdl2_render.cpp`, `sdl2_keyname.cpp`, `circle_stubs.cpp` | the smaller gaps |

## License

The code in this repository — the kernel layer in `host/` and the build — is
released under the GNU Lesser General Public License, version 3. See
[LICENSE](LICENSE).

The submodules are other people's work and carry their own terms, and two of
them matter before you distribute anything you build here:

- **NXEngine-evo** is released under the GNU General Public License, version 3.
- **Circle** is released under the GNU General Public License, version 3.

Building a kernel image here combines them, so the image is covered by the GNU
General Public License, version 3. Doing that for yourself is straightforward;
redistributing the result means satisfying those terms, which among other
things means offering the complete corresponding source.

Cave Story is the work of Daisuke "Pixel" Amaya. This project is not
affiliated with him, with Nicalis, or with any publisher of the game.
