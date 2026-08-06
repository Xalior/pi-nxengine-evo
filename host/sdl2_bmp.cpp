//
// sdl2_bmp.cpp — SDL_LoadBMP_RW, the way every piece of Cave Story artwork
// gets into memory.
//
// The game's sprite sheets, tilesets, backdrops and portraits are Windows
// bitmap files (named .pbm, but bitmaps all the same), and Surface::loadImage
// reads every one of them with SDL_LoadBMP. circle-libsdl2 has no bitmap
// reader, so this is it.
//
// WHAT IS READ: uncompressed bitmaps with a BITMAPINFOHEADER or later, at 1,
// 4, 8, 24 or 32 bits per pixel, stored either way up. Run-length encoded and
// bitfield-encoded bitmaps are refused with a message rather than decoded
// approximately; nothing Cave Story ships uses them.
//
// The surface handed back is always 32-bit XRGB8888 — the only format the
// shim allocates — with a palette expanded into it. That also keeps the
// engine's copy of SDL2_gfx's zoom on its 32-bit path, which is the one that
// does not reach into a palette that shim surfaces do not have.
//
#include <SDL2/SDL.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace
{

inline uint16_t LE16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

inline uint32_t LE32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

} // namespace

extern "C" {

SDL_Surface *SDL_LoadBMP_RW(SDL_RWops *src, int freesrc)
{
    if (src == nullptr)
    {
        SDL_SetError("no stream to read a bitmap from");
        return nullptr;
    }

    const Sint64 filesize = SDL_RWsize(src);
    uint8_t *data = nullptr;
    if (filesize > 14)
    {
        data = (uint8_t *)malloc((size_t)filesize);
        if (data != nullptr
            && SDL_RWread(src, data, 1, (size_t)filesize) != (size_t)filesize)
        {
            free(data);
            data = nullptr;
        }
    }
    if (freesrc)
        SDL_RWclose(src);
    if (data == nullptr)
    {
        SDL_SetError("cannot read the bitmap");
        return nullptr;
    }

    SDL_Surface *surface = nullptr;
    do
    {
        if (data[0] != 'B' || data[1] != 'M')
        {
            SDL_SetError("not a bitmap file");
            break;
        }

        const uint32_t pixel_offset = LE32(data + 10);
        const uint32_t header_size = LE32(data + 14);
        if (header_size < 40 || (size_t)(14 + header_size) > (size_t)filesize)
        {
            SDL_SetError("bitmap header is too old or truncated");
            break;
        }

        const uint8_t *ih = data + 14;
        const int32_t width = (int32_t)LE32(ih + 4);
        const int32_t signed_height = (int32_t)LE32(ih + 8);
        const uint16_t bpp = LE16(ih + 14);
        const uint32_t compression = LE32(ih + 16);
        uint32_t colours = LE32(ih + 32);

        // A negative height means the rows are stored top row first.
        const bool top_down = signed_height < 0;
        const int32_t height = top_down ? -signed_height : signed_height;

        if (width <= 0 || height <= 0)
        {
            SDL_SetError("bitmap has no picture in it");
            break;
        }
        if (compression != 0)
        {
            SDL_SetError("compressed bitmaps are not read");
            break;
        }
        if (bpp != 1 && bpp != 4 && bpp != 8 && bpp != 24 && bpp != 32)
        {
            SDL_SetError("%d-bit bitmaps are not read", (int)bpp);
            break;
        }

        // The palette, for the depths that have one. A zero count means the
        // full palette for that depth.
        uint32_t palette[256];
        memset(palette, 0, sizeof(palette));
        if (bpp <= 8)
        {
            if (colours == 0 || colours > 256u)
                colours = 1u << bpp;
            const uint8_t *pal = data + 14 + header_size;
            if ((size_t)(pal - data) + (size_t)colours * 4 > (size_t)filesize)
            {
                SDL_SetError("bitmap palette is truncated");
                break;
            }
            for (uint32_t i = 0; i < colours; i++)
                palette[i] = ((uint32_t)pal[i * 4 + 2] << 16)     // red
                           | ((uint32_t)pal[i * 4 + 1] << 8)      // green
                           | (uint32_t)pal[i * 4];                // blue
        }

        // Rows are padded out to a multiple of four bytes.
        const size_t rowbytes = (((size_t)width * bpp + 31) / 32) * 4;
        if ((size_t)pixel_offset + rowbytes * (size_t)height > (size_t)filesize)
        {
            SDL_SetError("bitmap pixel data is truncated");
            break;
        }

        surface = SDL_CreateRGBSurface(0, width, height, 32, 0, 0, 0, 0);
        if (surface == nullptr)
            break;

        for (int32_t y = 0; y < height; y++)
        {
            const uint8_t *row = data + pixel_offset
                               + rowbytes * (size_t)(top_down ? y : (height - 1 - y));
            Uint32 *dst = (Uint32 *)((Uint8 *)surface->pixels
                            + (size_t)y * surface->pitch);
            for (int32_t x = 0; x < width; x++)
            {
                switch (bpp)
                {
                case 1:
                    dst[x] = palette[(row[x >> 3] >> (7 - (x & 7))) & 1];
                    break;
                case 4:
                    dst[x] = palette[(x & 1) ? (row[x >> 1] & 0x0F)
                                             : (row[x >> 1] >> 4)];
                    break;
                case 8:
                    dst[x] = palette[row[x]];
                    break;
                case 24:
                    dst[x] = ((uint32_t)row[x * 3 + 2] << 16)
                           | ((uint32_t)row[x * 3 + 1] << 8)
                           | (uint32_t)row[x * 3];
                    break;
                default:
                    dst[x] = LE32(row + x * 4) & 0x00FFFFFFu;
                    break;
                }
            }
        }
    } while (false);

    free(data);
    return surface;
}

} // extern "C"
