//
// sdl2_surface.cpp — the SDL2 surface and texture calls NXEngine-evo makes
// that circle-libsdl2 does not implement.
//
// The shim's own surface is a staging buffer and nothing more: it allocates
// 32-bit XRGB8888 pixels and frees them, because the shim's renderer draws
// from textures alone. NXEngine-evo needs more than that. It loads its
// artwork from Windows bitmap files, marks black as transparent with a
// colour key, and turns the result into a texture; one place fills a
// rectangle of a surface with a flat colour. This file is that missing
// middle, written against the shim's public API and nothing else.
//
// EVERY SURFACE HERE IS 32 BITS PER PIXEL, XRGB8888. That is the only format
// the shim allocates and the only one its textures accept, so the bitmap
// loader expands 1-, 4-, 8- and 24-bit files into it rather than handing back
// a surface in the file's own depth. This matters to the engine's copy of
// SDL2_gfx's zoom (src/graphics/zoom.cpp), which has a separate path for
// 8-bit palette surfaces that reaches into format->palette: with every
// surface arriving 32-bit, that path is never taken.
//
// THE COLOUR KEY has nowhere to live in a shim surface — the pixel format
// record is one immutable object shared by every surface, and SDL2 keeps the
// key inside the private blit map. So the key is kept in the surface's own
// `map` pointer, which belongs to the SDL implementation and is what this
// file is. The value is stored in the pointer itself rather than in
// allocated memory, so a surface freed by the shim takes its key with it and
// there is nothing left to leak.
//
#include <SDL2/SDL.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace
{

// The colour key, carried in SDL_Surface::map. Bit 32 marks "a key is set",
// so a key of zero — which is what black is, and black is the key Cave
// Story's artwork uses — is distinct from no key at all.
const uintptr_t KEY_PRESENT = (uintptr_t)1 << 32;

inline bool SurfaceKey(const SDL_Surface *s, Uint32 *key)
{
    uintptr_t v = (uintptr_t)s->map;
    if (!(v & KEY_PRESENT))
        return false;
    if (key != nullptr)
        *key = (Uint32)(v & 0xFFFFFFFFu);
    return true;
}

// The rectangle actually written, once the caller's has been clipped to the
// surface. Returns false when nothing is left.
bool ClipToSurface(SDL_Surface *s, const SDL_Rect *in, SDL_Rect *out)
{
    SDL_Rect r = (in != nullptr) ? *in : SDL_Rect{ 0, 0, s->w, s->h };
    if (r.x < 0) { r.w += r.x; r.x = 0; }
    if (r.y < 0) { r.h += r.y; r.y = 0; }
    if (r.x + r.w > s->w) r.w = s->w - r.x;
    if (r.y + r.h > s->h) r.h = s->h - r.y;
    if (r.w <= 0 || r.h <= 0)
        return false;
    *out = r;
    return true;
}

} // namespace

extern "C" {

// ---- pixel values -----------------------------------------------------------

Uint32 SDL_MapRGB(const SDL_PixelFormat *format, Uint8 r, Uint8 g, Uint8 b)
{
    if (format == nullptr)
        return 0;
    return ((Uint32)r >> format->Rloss) << format->Rshift
         | ((Uint32)g >> format->Gloss) << format->Gshift
         | ((Uint32)b >> format->Bloss) << format->Bshift
         | format->Amask;
}

// ---- locking ----------------------------------------------------------------
//
// Nothing here is ever hardware memory or run-length encoded, so a lock has
// nothing to do. SDL_MUSTLOCK is false for every surface this port makes,
// which is why the engine's zoom never calls these at all; they exist so that
// code which does call them unconditionally still works.

int SDL_LockSurface(SDL_Surface *surface)
{
    if (surface == nullptr)
    {
        SDL_SetError("no surface to lock");
        return -1;
    }
    surface->locked++;
    return 0;
}

void SDL_UnlockSurface(SDL_Surface *surface)
{
    if (surface != nullptr && surface->locked > 0)
        surface->locked--;
}

// ---- the colour key ---------------------------------------------------------

int SDL_SetColorKey(SDL_Surface *surface, int flag, Uint32 key)
{
    if (surface == nullptr)
    {
        SDL_SetError("no surface to key");
        return -1;
    }
    surface->map = flag ? (struct SDL_BlitMap *)(KEY_PRESENT | (uintptr_t)key)
                        : nullptr;
    return 0;
}

int SDL_GetColorKey(SDL_Surface *surface, Uint32 *key)
{
    if (surface == nullptr)
    {
        SDL_SetError("no surface to read a key from");
        return -1;
    }
    if (!SurfaceKey(surface, key))
    {
        SDL_SetError("surface has no colour key");
        return -1;
    }
    return 0;
}

// ---- filling and copying ----------------------------------------------------

int SDL_FillRect(SDL_Surface *dst, const SDL_Rect *rect, Uint32 color)
{
    if (dst == nullptr || dst->pixels == nullptr)
    {
        SDL_SetError("no surface to fill");
        return -1;
    }

    SDL_Rect r;
    if (!ClipToSurface(dst, rect, &r))
        return 0;

    for (int y = 0; y < r.h; y++)
    {
        Uint32 *row = (Uint32 *)((Uint8 *)dst->pixels + (size_t)(r.y + y) * dst->pitch)
                    + r.x;
        for (int x = 0; x < r.w; x++)
            row[x] = color;
    }
    return 0;
}

// A straight 32-bit copy, honouring the source's colour key by leaving keyed
// pixels alone. Both surfaces are XRGB8888 by construction, so there is no
// format conversion to do and none is attempted: a caller handing this two
// different formats gets an error rather than a wrong picture.
int SDL_UpperBlit(SDL_Surface *src, const SDL_Rect *srcrect,
                  SDL_Surface *dst, SDL_Rect *dstrect)
{
    if (src == nullptr || dst == nullptr)
    {
        SDL_SetError("blit needs two surfaces");
        return -1;
    }
    if (src->format->BytesPerPixel != 4 || dst->format->BytesPerPixel != 4)
    {
        SDL_SetError("only 32-bit surfaces can be blitted");
        return -1;
    }

    SDL_Rect s;
    if (!ClipToSurface(src, srcrect, &s))
        return 0;

    SDL_Rect d = { dstrect ? dstrect->x : 0, dstrect ? dstrect->y : 0, s.w, s.h };
    // Clipping the destination shrinks the source region by the same amount,
    // from the same edges, so the two stay in step.
    if (d.x < 0) { s.x -= d.x; s.w += d.x; d.w += d.x; d.x = 0; }
    if (d.y < 0) { s.y -= d.y; s.h += d.y; d.h += d.y; d.y = 0; }
    if (d.x + d.w > dst->w) { d.w = dst->w - d.x; s.w = d.w; }
    if (d.y + d.h > dst->h) { d.h = dst->h - d.y; s.h = d.h; }
    if (d.w <= 0 || d.h <= 0)
        return 0;

    Uint32 key = 0;
    const bool keyed = SurfaceKey(src, &key);

    for (int y = 0; y < d.h; y++)
    {
        const Uint32 *sp = (const Uint32 *)((const Uint8 *)src->pixels
                            + (size_t)(s.y + y) * src->pitch) + s.x;
        Uint32 *dp = (Uint32 *)((Uint8 *)dst->pixels
                            + (size_t)(d.y + y) * dst->pitch) + d.x;
        if (keyed)
        {
            for (int x = 0; x < d.w; x++)
                if ((sp[x] & 0x00FFFFFFu) != (key & 0x00FFFFFFu))
                    dp[x] = sp[x];
        }
        else
        {
            memcpy(dp, sp, (size_t)d.w * 4);
        }
    }

    if (dstrect != nullptr)
        *dstrect = d;
    return 0;
}

// ---- wrapping existing pixels -----------------------------------------------
//
// SDL2 makes this surface point at the caller's memory. This one COPIES it
// instead, converting from the masks the caller states into the one format
// the shim has. The reason is ownership: the shim's SDL_FreeSurface frees
// whatever `pixels` points at, and the only caller here — the window icon,
// built from a constant array compiled into the program — would have its
// static data handed to free(). A copy is a few kilobytes and cannot do that.

SDL_Surface *SDL_CreateRGBSurfaceFrom(void *pixels, int width, int height,
                                      int depth, int pitch, Uint32 Rmask,
                                      Uint32 Gmask, Uint32 Bmask, Uint32 Amask)
{
    if (depth != 32 || pixels == nullptr)
    {
        SDL_SetError("only 32-bit pixel data can be wrapped");
        return nullptr;
    }

    SDL_Surface *surface = SDL_CreateRGBSurface(0, width, height, 32, 0, 0, 0, 0);
    if (surface == nullptr)
        return nullptr;

    // Where each channel sits in the caller's arrangement. A mask of zero
    // means the channel is absent, which for alpha means fully opaque.
    auto shift_of = [](Uint32 mask) {
        int s = 0;
        if (mask == 0) return -1;
        while (((mask >> s) & 1) == 0) s++;
        return s;
    };
    const int rs = shift_of(Rmask), gs = shift_of(Gmask), bs = shift_of(Bmask);

    for (int y = 0; y < height; y++)
    {
        const Uint32 *sp = (const Uint32 *)((const Uint8 *)pixels + (size_t)y * pitch);
        Uint32 *dp = (Uint32 *)((Uint8 *)surface->pixels + (size_t)y * surface->pitch);
        for (int x = 0; x < width; x++)
        {
            const Uint32 v = sp[x];
            const Uint32 r = (rs < 0) ? 0 : ((v & Rmask) >> rs) & 0xFF;
            const Uint32 g = (gs < 0) ? 0 : ((v & Gmask) >> gs) & 0xFF;
            const Uint32 b = (bs < 0) ? 0 : ((v & Bmask) >> bs) & 0xFF;
            dp[x] = (r << 16) | (g << 8) | b;
        }
    }
    (void)Amask;
    return surface;
}

} // extern "C"
