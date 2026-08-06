//
// sdl2_image.cpp — IMG_Load, for the one format this game needs.
//
// NXEngine-evo loads two kinds of PNG: the font atlases, which carry a real
// alpha channel and are what every line of text on screen is drawn from, and
// one lighting sprite. It asks IMG_Init for PNG support and stops if it is
// missing, so a stub would leave the game with no way to start.
//
// There is no libpng here and no zlib either, so both are in this file: a
// DEFLATE decompressor and a PNG reader on top of it. Together they are a
// few hundred lines, which is the price of the game having text.
//
// WHAT IS READ: PNG files at 8 bits per channel, not interlaced, in any of
// the five colour types — greyscale, truecolour, palette, greyscale with
// alpha, truecolour with alpha — with a tRNS chunk honoured for palette
// images. Anything else is refused with a message naming what it was, rather
// than decoded approximately. Nothing in the game's own data needs more:
// every PNG it ships is 8-bit truecolour with alpha.
//
// The surface handed back is the shim's only surface format, XRGB8888, with
// the alpha byte filled in and NX_SURFACE_HAS_ALPHA set so that the texture
// conversion keeps it.
//
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include "nx_surface_flags.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace
{

// ---------------------------------------------------------------------------
// DEFLATE (RFC 1951), decompressing into a buffer of known size.
//
// The size is always known here: a PNG's uncompressed data is exactly one
// filter byte plus one row of samples for every row of the image. So there is
// no growing buffer and no guessing — the decompressor either fills the
// buffer it was given or reports that the stream was malformed.
// ---------------------------------------------------------------------------

struct Bitstream
{
    const uint8_t *in;
    size_t inlen;
    size_t incnt;
    long bitbuf;
    int bitcnt;
};

// A canonical Huffman table, in the form that makes decoding a walk down the
// code lengths: how many codes there are of each length, and the symbols in
// order.
struct Huffman
{
    short *count;       // [0 .. MAXBITS]
    short *symbol;
};

const int MAXBITS = 15;
const int MAXLCODES = 286;
const int MAXDCODES = 30;
const int FIXLCODES = 288;

// The next `need` bits, least significant first. Returns -1 past the end of
// the input.
int Bits(Bitstream *s, int need)
{
    long val = s->bitbuf;
    while (s->bitcnt < need)
    {
        if (s->incnt == s->inlen)
            return -1;
        val |= (long)(s->in[s->incnt++]) << s->bitcnt;
        s->bitcnt += 8;
    }
    s->bitbuf = val >> need;
    s->bitcnt -= need;
    return (int)(val & ((1L << need) - 1));
}

int Decode(Bitstream *s, const Huffman *h)
{
    int code = 0, first = 0, index = 0;
    for (int len = 1; len <= MAXBITS; len++)
    {
        const int b = Bits(s, 1);
        if (b < 0)
            return -1;
        code |= b;
        const int count = h->count[len];
        if (code - count < first)
            return h->symbol[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return -1;
}

// Build a table from a list of code lengths. Returns 0 for a complete code,
// a positive number for an incomplete one (legal only in the single-symbol
// case), and a negative number for an over-subscribed one.
int Construct(Huffman *h, const short *length, int n)
{
    for (int len = 0; len <= MAXBITS; len++)
        h->count[len] = 0;
    for (int symbol = 0; symbol < n; symbol++)
        h->count[length[symbol]]++;
    if (h->count[0] == n)
        return 0;

    int left = 1;
    for (int len = 1; len <= MAXBITS; len++)
    {
        left <<= 1;
        left -= h->count[len];
        if (left < 0)
            return left;
    }

    short offs[MAXBITS + 1];
    offs[1] = 0;
    for (int len = 1; len < MAXBITS; len++)
        offs[len + 1] = offs[len] + h->count[len];
    for (int symbol = 0; symbol < n; symbol++)
        if (length[symbol] != 0)
            h->symbol[offs[length[symbol]]++] = (short)symbol;
    return left;
}

int Codes(Bitstream *s, const Huffman *lencode, const Huffman *distcode,
          uint8_t *out, size_t outlen, size_t *outpos)
{
    static const short lens[29] = {
        3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
        35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
    static const short lext[29] = {
        0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
        3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
    static const short dists[30] = {
        1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
        257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193,
        12289, 16385, 24577 };
    static const short dext[30] = {
        0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
        7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };

    for (;;)
    {
        int symbol = Decode(s, lencode);
        if (symbol < 0)
            return symbol;

        if (symbol < 256)
        {
            if (*outpos >= outlen)
                return -1;
            out[(*outpos)++] = (uint8_t)symbol;
        }
        else if (symbol == 256)
        {
            return 0;
        }
        else
        {
            symbol -= 257;
            if (symbol >= 29)
                return -1;
            const int extra_len = Bits(s, lext[symbol]);
            if (extra_len < 0)
                return -1;
            const int len = lens[symbol] + extra_len;

            symbol = Decode(s, distcode);
            if (symbol < 0 || symbol >= 30)
                return -1;
            const int extra_dist = Bits(s, dext[symbol]);
            if (extra_dist < 0)
                return -1;
            const size_t dist = (size_t)(dists[symbol] + extra_dist);

            if (dist > *outpos || *outpos + (size_t)len > outlen)
                return -1;
            for (int i = 0; i < len; i++)
            {
                out[*outpos] = out[*outpos - dist];
                (*outpos)++;
            }
        }
    }
}

int Stored(Bitstream *s, uint8_t *out, size_t outlen, size_t *outpos)
{
    s->bitbuf = 0;
    s->bitcnt = 0;
    if (s->incnt + 4 > s->inlen)
        return -1;
    const unsigned len = s->in[s->incnt] | ((unsigned)s->in[s->incnt + 1] << 8);
    // The block length is stored twice, the second time inverted.
    if (s->in[s->incnt + 2] != (~len & 0xFF)
        || s->in[s->incnt + 3] != ((~len >> 8) & 0xFF))
        return -1;
    s->incnt += 4;
    if (s->incnt + len > s->inlen || *outpos + len > outlen)
        return -1;
    memcpy(out + *outpos, s->in + s->incnt, len);
    s->incnt += len;
    *outpos += len;
    return 0;
}

int Fixed(Bitstream *s, uint8_t *out, size_t outlen, size_t *outpos)
{
    static short lencnt[MAXBITS + 1], lensym[FIXLCODES];
    static short distcnt[MAXBITS + 1], distsym[MAXDCODES];
    static Huffman lencode = { lencnt, lensym };
    static Huffman distcode = { distcnt, distsym };
    static bool built = false;

    if (!built)
    {
        short lengths[FIXLCODES];
        int symbol = 0;
        for (; symbol < 144; symbol++) lengths[symbol] = 8;
        for (; symbol < 256; symbol++) lengths[symbol] = 9;
        for (; symbol < 280; symbol++) lengths[symbol] = 7;
        for (; symbol < FIXLCODES; symbol++) lengths[symbol] = 8;
        Construct(&lencode, lengths, FIXLCODES);
        for (symbol = 0; symbol < MAXDCODES; symbol++) lengths[symbol] = 5;
        Construct(&distcode, lengths, MAXDCODES);
        built = true;
    }
    return Codes(s, &lencode, &distcode, out, outlen, outpos);
}

int Dynamic(Bitstream *s, uint8_t *out, size_t outlen, size_t *outpos)
{
    static const short order[19] = {
        16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

    const int nlen_raw = Bits(s, 5);
    const int ndist_raw = Bits(s, 5);
    const int ncode_raw = Bits(s, 4);
    if (nlen_raw < 0 || ndist_raw < 0 || ncode_raw < 0)
        return -1;
    const int nlen = nlen_raw + 257;
    const int ndist = ndist_raw + 1;
    const int ncode = ncode_raw + 4;
    if (nlen > MAXLCODES || ndist > MAXDCODES)
        return -1;

    short lengths[MAXLCODES + MAXDCODES];
    memset(lengths, 0, sizeof(lengths));
    for (int index = 0; index < ncode; index++)
    {
        const int v = Bits(s, 3);
        if (v < 0)
            return -1;
        lengths[order[index]] = (short)v;
    }

    short lencnt[MAXBITS + 1], lensym[MAXLCODES + MAXDCODES];
    Huffman lencode = { lencnt, lensym };
    if (Construct(&lencode, lengths, 19) != 0)
        return -1;

    int index = 0;
    while (index < nlen + ndist)
    {
        int symbol = Decode(s, &lencode);
        if (symbol < 0)
            return -1;

        if (symbol < 16)
        {
            lengths[index++] = (short)symbol;
            continue;
        }

        short len = 0;
        int repeat;
        if (symbol == 16)
        {
            if (index == 0)
                return -1;
            len = lengths[index - 1];
            repeat = 3 + Bits(s, 2);
        }
        else if (symbol == 17)
        {
            repeat = 3 + Bits(s, 3);
        }
        else
        {
            repeat = 11 + Bits(s, 7);
        }
        if (index + repeat > nlen + ndist)
            return -1;
        while (repeat-- > 0)
            lengths[index++] = len;
    }

    short dcnt[MAXBITS + 1], dsym[MAXDCODES];
    Huffman distcode = { dcnt, dsym };
    if (Construct(&lencode, lengths, nlen) < 0)
        return -1;
    if (Construct(&distcode, lengths + nlen, ndist) < 0)
        return -1;

    return Codes(s, &lencode, &distcode, out, outlen, outpos);
}

// Decompresses a zlib stream (RFC 1950: a two-byte header, then DEFLATE)
// into a buffer whose size is already known. The trailing Adler-32 is not
// checked: a PNG carries a CRC on every chunk already.
bool ZlibInflate(const uint8_t *in, size_t inlen, uint8_t *out, size_t outlen)
{
    if (inlen < 2)
        return false;

    Bitstream s;
    s.in = in;
    s.inlen = inlen;
    s.incnt = 2;                // past the zlib header
    s.bitbuf = 0;
    s.bitcnt = 0;

    size_t outpos = 0;
    int last, type;
    do
    {
        last = Bits(&s, 1);
        type = Bits(&s, 2);
        if (last < 0 || type < 0)
            return false;

        int err;
        switch (type)
        {
        case 0:  err = Stored(&s, out, outlen, &outpos); break;
        case 1:  err = Fixed(&s, out, outlen, &outpos); break;
        case 2:  err = Dynamic(&s, out, outlen, &outpos); break;
        default: return false;
        }
        if (err != 0)
            return false;
    } while (!last);

    return outpos == outlen;
}

// ---------------------------------------------------------------------------
// PNG
// ---------------------------------------------------------------------------

inline uint32_t BE32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
         | ((uint32_t)p[2] << 8) | p[3];
}

// The Paeth predictor, from the PNG specification: of the three neighbours,
// the one closest to their linear estimate.
inline int Paeth(int a, int b, int c)
{
    const int p = a + b - c;
    const int pa = p > a ? p - a : a - p;
    const int pb = p > b ? p - b : b - p;
    const int pc = p > c ? p - c : c - p;
    if (pa <= pb && pa <= pc) return a;
    if (pb <= pc) return b;
    return c;
}

// Undoes the per-row filter, in place, over the whole decompressed image.
// Each row is one filter byte followed by `stride` bytes of samples.
bool Unfilter(uint8_t *raw, int height, size_t stride, int bpp)
{
    uint8_t *prev = nullptr;
    uint8_t *row = raw;

    for (int y = 0; y < height; y++)
    {
        const int filter = *row++;
        for (size_t i = 0; i < stride; i++)
        {
            const int a = (i >= (size_t)bpp) ? row[i - bpp] : 0;
            const int b = prev ? prev[i] : 0;
            const int c = (prev && i >= (size_t)bpp) ? prev[i - bpp] : 0;
            switch (filter)
            {
            case 0: break;
            case 1: row[i] = (uint8_t)(row[i] + a); break;
            case 2: row[i] = (uint8_t)(row[i] + b); break;
            case 3: row[i] = (uint8_t)(row[i] + ((a + b) >> 1)); break;
            case 4: row[i] = (uint8_t)(row[i] + Paeth(a, b, c)); break;
            default: return false;
            }
        }
        prev = row;
        row += stride;
    }
    return true;
}

// Reads a whole file into memory. The shim's SDL_RWops is what reaches the
// card from whichever core the game is running on.
uint8_t *ReadWholeFile(const char *file, size_t *size)
{
    SDL_RWops *rw = SDL_RWFromFile(file, "rb");
    if (rw == nullptr)
        return nullptr;

    const Sint64 len = SDL_RWsize(rw);
    if (len <= 0)
    {
        SDL_RWclose(rw);
        return nullptr;
    }

    uint8_t *data = (uint8_t *)malloc((size_t)len);
    if (data == nullptr)
    {
        SDL_RWclose(rw);
        return nullptr;
    }

    const size_t got = SDL_RWread(rw, data, 1, (size_t)len);
    SDL_RWclose(rw);
    if (got != (size_t)len)
    {
        free(data);
        return nullptr;
    }
    *size = (size_t)len;
    return data;
}

} // namespace

extern "C" {

// PNG is the only decoder here, so the answer is PNG or nothing. Reporting
// the truth is what lets the caller stop with a clear message instead of
// failing later on a null surface.
int IMG_Init(int flags)
{
    return flags & IMG_INIT_PNG;
}

void IMG_Quit(void) {}

SDL_Surface *IMG_Load(const char *file)
{
    if (file == nullptr)
    {
        SDL_SetError("no file named");
        return nullptr;
    }

    size_t filesize = 0;
    uint8_t *data = ReadWholeFile(file, &filesize);
    if (data == nullptr)
    {
        SDL_SetError("cannot read %s", file);
        return nullptr;
    }

    static const uint8_t signature[8] = { 137, 'P', 'N', 'G', 13, 10, 26, 10 };
    if (filesize < 8 || memcmp(data, signature, 8) != 0)
    {
        free(data);
        SDL_SetError("%s is not a PNG file", file);
        return nullptr;
    }

    // Pass one: find the header, the palette and the compressed data. The
    // image data may be split over any number of IDAT chunks, which have to
    // be treated as one stream, so they are gathered into a single buffer.
    uint32_t width = 0, height = 0;
    int depth = 0, colour = 0, interlace = 0;
    uint8_t palette[256][3];
    uint8_t palette_alpha[256];
    int palette_size = 0;
    memset(palette, 0, sizeof(palette));
    memset(palette_alpha, 255, sizeof(palette_alpha));

    uint8_t *idat = nullptr;
    size_t idat_len = 0;

    size_t pos = 8;
    bool ok = true;
    while (pos + 8 <= filesize)
    {
        const uint32_t len = BE32(data + pos);
        const uint8_t *type = data + pos + 4;
        const uint8_t *body = data + pos + 8;
        if (pos + 12 + len > filesize)
        {
            ok = false;
            break;
        }

        if (memcmp(type, "IHDR", 4) == 0 && len >= 13)
        {
            width = BE32(body);
            height = BE32(body + 4);
            depth = body[8];
            colour = body[9];
            interlace = body[12];
        }
        else if (memcmp(type, "PLTE", 4) == 0)
        {
            palette_size = (int)(len / 3);
            if (palette_size > 256)
                palette_size = 256;
            memcpy(palette, body, (size_t)palette_size * 3);
        }
        else if (memcmp(type, "tRNS", 4) == 0)
        {
            const size_t n = (len > 256) ? 256 : len;
            memcpy(palette_alpha, body, n);
        }
        else if (memcmp(type, "IDAT", 4) == 0)
        {
            uint8_t *grown = (uint8_t *)realloc(idat, idat_len + len);
            if (grown == nullptr)
            {
                ok = false;
                break;
            }
            idat = grown;
            memcpy(idat + idat_len, body, len);
            idat_len += len;
        }
        else if (memcmp(type, "IEND", 4) == 0)
        {
            break;
        }

        pos += 12 + len;        // length, type, body, CRC
    }

    // How many bytes one pixel takes in the decompressed rows.
    int samples = 0;
    switch (colour)
    {
    case 0: samples = 1; break;     // greyscale
    case 2: samples = 3; break;     // truecolour
    case 3: samples = 1; break;     // palette index
    case 4: samples = 2; break;     // greyscale and alpha
    case 6: samples = 4; break;     // truecolour and alpha
    default: ok = false; break;
    }

    if (!ok || width == 0 || height == 0 || idat == nullptr)
    {
        free(idat);
        free(data);
        SDL_SetError("%s is a malformed PNG file", file);
        return nullptr;
    }
    if (depth != 8 || interlace != 0)
    {
        free(idat);
        free(data);
        SDL_SetError("%s is a %d-bit%s PNG; only 8-bit non-interlaced is read",
                     file, depth, interlace ? " interlaced" : "");
        return nullptr;
    }

    const size_t stride = (size_t)width * samples;
    const size_t rawlen = (stride + 1) * height;
    uint8_t *raw = (uint8_t *)malloc(rawlen);
    if (raw == nullptr || !ZlibInflate(idat, idat_len, raw, rawlen)
        || !Unfilter(raw, (int)height, stride, samples))
    {
        free(raw);
        free(idat);
        free(data);
        SDL_SetError("%s: the compressed image data is malformed", file);
        return nullptr;
    }
    free(idat);
    free(data);

    SDL_Surface *surface = SDL_CreateRGBSurface(0, (int)width, (int)height, 32,
                                                0, 0, 0, 0);
    if (surface == nullptr)
    {
        free(raw);
        return nullptr;
    }

    for (uint32_t y = 0; y < height; y++)
    {
        const uint8_t *src = raw + (stride + 1) * y + 1;
        Uint32 *dst = (Uint32 *)((Uint8 *)surface->pixels
                        + (size_t)y * surface->pitch);
        for (uint32_t x = 0; x < width; x++)
        {
            uint8_t r, g, b, a = 255;
            switch (colour)
            {
            case 0:
                r = g = b = src[x];
                break;
            case 2:
                r = src[x * 3]; g = src[x * 3 + 1]; b = src[x * 3 + 2];
                break;
            case 3:
            {
                const int idx = src[x];
                r = palette[idx][0]; g = palette[idx][1]; b = palette[idx][2];
                a = palette_alpha[idx];
                break;
            }
            case 4:
                r = g = b = src[x * 2];
                a = src[x * 2 + 1];
                break;
            default:
                r = src[x * 4]; g = src[x * 4 + 1]; b = src[x * 4 + 2];
                a = src[x * 4 + 3];
                break;
            }
            dst[x] = ((Uint32)a << 24) | ((Uint32)r << 16) | ((Uint32)g << 8) | b;
        }
    }
    free(raw);

    // The alpha byte is filled in, which a shim surface's pixel format does
    // not describe. This flag is what tells the texture conversion to keep
    // it.
    surface->flags |= NX_SURFACE_HAS_ALPHA;
    return surface;
}

} // extern "C"
