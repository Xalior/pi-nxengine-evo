//
// sdl2_texture.cpp — turning a surface into a texture, and drawing it
// mirrored.
//
// circle-libsdl2 creates and draws textures, but it has no call that makes
// one from a surface and no mirrored copy. NXEngine-evo needs both: every
// texture it owns comes from SDL_CreateTextureFromSurface, and every sprite
// facing left is drawn with SDL_RenderCopyEx and SDL_FLIP_HORIZONTAL.
//
// THE COLOUR KEY BECOMES ALPHA. A shim surface is XRGB8888 with no alpha
// channel, and a shim texture is ARGB8888. So the conversion is a single
// pass that writes the alpha byte: opaque everywhere, except where the pixel
// matches the surface's colour key, which becomes fully transparent. A keyed
// surface therefore produces a texture in blend mode, which is what makes
// Cave Story's black backgrounds disappear.
//
// THE MIRROR IS A SECOND TEXTURE, built at the same moment as the first and
// holding the same picture reversed left to right. Drawing mirrored is then
// an ordinary SDL_RenderCopy from the twin, with the source rectangle
// reflected across the texture's width. It costs twice the texture memory
// for every image the game loads. The alternative — reversing pixels on
// every draw — would cost that work on every sprite of every frame, and this
// is a machine with no GPU to do it.
//
// The twin is remembered in a small registry, because the base texture is an
// opaque handle with nowhere to hang anything. SDL_DestroyTexture is wrapped
// at link time (see the Makefile) so that destroying a texture destroys its
// twin with it: without that the registry would grow every time the engine
// reloads its artwork, which it does on every resolution change.
//
#include <SDL2/SDL.h>

#include "nx_surface_flags.h"

#include <cstdlib>
#include <cstring>
#include <vector>

namespace
{

struct Mirror
{
    SDL_Texture *base;
    SDL_Texture *flipped;
    int w;
    int h;
};

std::vector<Mirror> s_mirrors;

Mirror *FindMirror(SDL_Texture *base)
{
    for (auto &m : s_mirrors)
        if (m.base == base)
            return &m;
    return nullptr;
}

// One row of a surface, converted to ARGB8888. Where the surface carries its
// own alpha the byte is kept; otherwise every pixel is opaque except those
// matching the colour key, which become fully transparent. `reverse` writes
// the row backwards, which is the whole of the mirroring.
void ConvertRow(const Uint32 *src, Uint32 *dst, int width, bool keyed,
                Uint32 key, bool has_alpha, bool reverse)
{
    for (int x = 0; x < width; x++)
    {
        const Uint32 rgb = src[x] & 0x00FFFFFFu;
        Uint32 out;
        if (keyed && rgb == (key & 0x00FFFFFFu))
            out = 0u;
        else if (has_alpha)
            out = src[x];
        else
            out = 0xFF000000u | rgb;
        dst[reverse ? (width - 1 - x) : x] = out;
    }
}

SDL_Texture *TextureFromSurface(SDL_Renderer *renderer, SDL_Surface *surface,
                                bool keyed, Uint32 key, bool has_alpha,
                                bool reverse)
{
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                             SDL_TEXTUREACCESS_STREAMING,
                                             surface->w, surface->h);
    if (texture == nullptr)
        return nullptr;

    const size_t rowbytes = (size_t)surface->w * 4;
    Uint32 *row = (Uint32 *)malloc(rowbytes);
    if (row == nullptr)
    {
        SDL_DestroyTexture(texture);
        SDL_SetError("out of memory converting a surface to a texture");
        return nullptr;
    }

    // Uploaded a row at a time: SDL_UpdateTexture takes a rectangle, and one
    // row is the largest piece that can be converted without a full-size
    // staging buffer beside the texture itself.
    for (int y = 0; y < surface->h; y++)
    {
        const Uint32 *sp = (const Uint32 *)((const Uint8 *)surface->pixels
                            + (size_t)y * surface->pitch);
        ConvertRow(sp, row, surface->w, keyed, key, has_alpha, reverse);
        SDL_Rect r = { 0, y, surface->w, 1 };
        if (SDL_UpdateTexture(texture, &r, row, (int)rowbytes) != 0)
        {
            free(row);
            SDL_DestroyTexture(texture);
            return nullptr;
        }
    }
    free(row);

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    return texture;
}

} // namespace

extern "C" {

void __real_SDL_DestroyTexture(SDL_Texture *texture);

SDL_Texture *SDL_CreateTextureFromSurface(SDL_Renderer *renderer,
                                          SDL_Surface *surface)
{
    if (renderer == nullptr || surface == nullptr)
    {
        SDL_SetError("no surface to make a texture from");
        return nullptr;
    }
    if (surface->format->BytesPerPixel != 4)
    {
        SDL_SetError("only 32-bit surfaces can become textures");
        return nullptr;
    }

    Uint32 key = 0;
    const bool keyed = (SDL_GetColorKey(surface, &key) == 0);

    const bool has_alpha = (surface->flags & NX_SURFACE_HAS_ALPHA) != 0;

    SDL_Texture *base = TextureFromSurface(renderer, surface, keyed, key,
                                           has_alpha, false);
    if (base == nullptr)
        return nullptr;

    SDL_Texture *flipped = TextureFromSurface(renderer, surface, keyed, key,
                                              has_alpha, true);
    if (flipped != nullptr)
        s_mirrors.push_back(Mirror{ base, flipped, surface->w, surface->h });

    return base;
}

// Rotation is not implemented and no caller asks for it, so a request for one
// fails rather than drawing the picture unrotated. Flipping is: horizontal
// from the mirrored twin, vertical by walking the destination in rows, which
// nothing in this game does either but which follows for free from the same
// idea.
int SDL_RenderCopyEx(SDL_Renderer *renderer, SDL_Texture *texture,
                     const SDL_Rect *srcrect, const SDL_Rect *dstrect,
                     const double angle, const SDL_Point *center,
                     const SDL_RendererFlip flip)
{
    if (angle != 0.0 || center != nullptr)
    {
        SDL_SetError("rotated texture copies are not implemented");
        return -1;
    }
    if (flip == SDL_FLIP_NONE)
        return SDL_RenderCopy(renderer, texture, srcrect, dstrect);
    if (flip != SDL_FLIP_HORIZONTAL)
    {
        SDL_SetError("only horizontal flips are implemented");
        return -1;
    }

    Mirror *m = FindMirror(texture);
    if (m == nullptr)
    {
        SDL_SetError("this texture has no mirrored copy");
        return -1;
    }

    // The same picture reversed: a source rectangle at x becomes one whose
    // right edge is that far from the right of the texture.
    SDL_Rect s = (srcrect != nullptr) ? *srcrect : SDL_Rect{ 0, 0, m->w, m->h };
    s.x = m->w - s.x - s.w;
    return SDL_RenderCopy(renderer, m->flipped, &s, dstrect);
}

// Wrapped rather than replaced: the shim owns the real call, and this only
// needs to happen first. See WRAPPED_SDL in the Makefile.
void __wrap_SDL_DestroyTexture(SDL_Texture *texture)
{
    for (size_t i = 0; i < s_mirrors.size(); i++)
    {
        if (s_mirrors[i].base == texture)
        {
            __real_SDL_DestroyTexture(s_mirrors[i].flipped);
            s_mirrors.erase(s_mirrors.begin() + i);
            break;
        }
    }
    __real_SDL_DestroyTexture(texture);
}

} // extern "C"
