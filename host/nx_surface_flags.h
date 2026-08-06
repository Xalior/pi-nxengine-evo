//
// nx_surface_flags.h — private surface flags for this port's SDL2 layer.
//
// A circle-libsdl2 surface is always XRGB8888: 32 bits per pixel with the
// top byte unused and the pixel format's alpha mask zero. Most pictures on
// this card need nothing more, because their transparency is a colour key.
// The engine's font atlases are the exception — they are PNG files with a
// real alpha channel, and the letters have soft edges.
//
// So an image loader may fill the top byte with alpha anyway, and set this
// flag on the surface to say that it did. SDL_CreateTextureFromSurface then
// carries that byte into the texture instead of overwriting it with opaque.
// The flag lives in SDL_Surface::flags, which belongs to the SDL
// implementation, and this port is that implementation.
//
#ifndef _nx_surface_flags_h
#define _nx_surface_flags_h

// Clear of every flag SDL2 itself defines (SDL_PREALLOC, SDL_RLEACCEL,
// SDL_DONTFREE, SDL_SIMD_ALIGNED are all in the low bits).
#define NX_SURFACE_HAS_ALPHA 0x40000000u

#endif
