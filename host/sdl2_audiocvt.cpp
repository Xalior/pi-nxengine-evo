//
// sdl2_audiocvt.cpp — SDL2's audio conversion, for the conversions this game
// asks for.
//
// NXEngine-evo synthesises every sound effect itself, with Pixtone, and hands
// the result to SDL to be put into the device's format: 8-bit signed mono at
// 22050 Hz becomes 16-bit signed stereo at 44100 Hz. It does the same again,
// at other rates, to pitch-shift the two sounds the game plays resampled.
// That is the whole of what passes through here.
//
// WHAT IS IMPLEMENTED: 8-bit signed, 8-bit unsigned and 16-bit signed
// (native byte order) sources, to a 16-bit signed native-order destination;
// one or two channels either way; any rate ratio, by linear interpolation.
// Anything else is refused by SDL_BuildAudioCVT rather than converted
// approximately.
//
// WHERE THE CHANNEL COUNTS LIVE. SDL_AudioCVT records the two formats and
// the rate ratio in named fields, but not the two channel counts: SDL2 keeps
// those inside the chain of filter functions it builds, and this
// implementation has no such chain. So the counts are kept in the first two
// slots of that unused `filters` array. The array is part of the structure
// the caller already carries between the two calls, so nothing needs
// allocating and nothing can be left behind.
//
#include <SDL2/SDL.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace
{

// The filter slots, borrowed to carry the channel counts from
// SDL_BuildAudioCVT to SDL_ConvertAudio.
inline void PutChannels(SDL_AudioCVT *cvt, int src, int dst)
{
    cvt->filters[0] = (SDL_AudioFilter)(uintptr_t)src;
    cvt->filters[1] = (SDL_AudioFilter)(uintptr_t)dst;
}

inline int SrcChannels(const SDL_AudioCVT *cvt)
{
    return (int)(uintptr_t)cvt->filters[0];
}

inline int DstChannels(const SDL_AudioCVT *cvt)
{
    return (int)(uintptr_t)cvt->filters[1];
}

bool SupportedSource(SDL_AudioFormat f)
{
    return f == AUDIO_S8 || f == AUDIO_U8 || f == AUDIO_S16SYS;
}

int BytesPerSample(SDL_AudioFormat f)
{
    return SDL_AUDIO_BITSIZE(f) / 8;
}

// One source sample, whatever its format, as a 16-bit signed value.
inline Sint16 ReadSample(const Uint8 *p, SDL_AudioFormat f)
{
    switch (f)
    {
    case AUDIO_S8:  return (Sint16)((Sint8)*p << 8);
    case AUDIO_U8:  return (Sint16)(((int)*p - 128) << 8);
    default:        return *(const Sint16 *)p;
    }
}

} // namespace

extern "C" {

int SDL_BuildAudioCVT(SDL_AudioCVT *cvt, SDL_AudioFormat src_format,
                      Uint8 src_channels, int src_rate,
                      SDL_AudioFormat dst_format, Uint8 dst_channels,
                      int dst_rate)
{
    if (cvt == nullptr)
        return -1;

    memset(cvt, 0, sizeof(*cvt));

    if (!SupportedSource(src_format) || dst_format != AUDIO_S16SYS)
    {
        SDL_SetError("audio conversion to or from this format is not implemented");
        return -1;
    }
    if (src_channels < 1 || src_channels > 2 || dst_channels < 1 || dst_channels > 2)
    {
        SDL_SetError("audio conversion beyond mono and stereo is not implemented");
        return -1;
    }
    if (src_rate <= 0 || dst_rate <= 0)
    {
        SDL_SetError("audio conversion needs both sample rates");
        return -1;
    }

    cvt->src_format = src_format;
    cvt->dst_format = dst_format;
    cvt->rate_incr  = (double)dst_rate / (double)src_rate;
    PutChannels(cvt, src_channels, dst_channels);

    // How the buffer changes size: bytes per frame either side, then the rate.
    const double src_frame = (double)BytesPerSample(src_format) * src_channels;
    const double dst_frame = (double)BytesPerSample(dst_format) * dst_channels;
    cvt->len_ratio = (dst_frame / src_frame) * cvt->rate_incr;

    // len_mult is what the caller multiplies its buffer by, so it must be a
    // whole number and must never be smaller than the ratio.
    cvt->len_mult = (int)ceil(cvt->len_ratio);
    if (cvt->len_mult < 1)
        cvt->len_mult = 1;

    // Zero means "the data is already in the right form"; the caller may then
    // skip SDL_ConvertAudio entirely.
    cvt->needed = (src_format != dst_format || src_channels != dst_channels
                   || src_rate != dst_rate) ? 1 : 0;
    return cvt->needed;
}

int SDL_ConvertAudio(SDL_AudioCVT *cvt)
{
    if (cvt == nullptr || cvt->buf == nullptr || cvt->len <= 0)
    {
        SDL_SetError("nothing to convert");
        return -1;
    }
    if (!cvt->needed)
    {
        cvt->len_cvt = cvt->len;
        return 0;
    }

    const int src_ch  = SrcChannels(cvt);
    const int dst_ch  = DstChannels(cvt);
    const int src_bps = BytesPerSample(cvt->src_format);

    const int src_frames = cvt->len / (src_bps * src_ch);
    if (src_frames <= 0)
    {
        cvt->len_cvt = 0;
        return 0;
    }

    int dst_frames = (int)(src_frames * cvt->rate_incr);
    if (dst_frames < 1)
        dst_frames = 1;

    const size_t out_bytes = (size_t)dst_frames * dst_ch * 2;
    Sint16 *out = (Sint16 *)malloc(out_bytes);
    if (out == nullptr)
    {
        SDL_SetError("out of memory converting audio");
        return -1;
    }

    for (int i = 0; i < dst_frames; i++)
    {
        // Where this output frame falls between two input frames.
        const double at = (double)i / cvt->rate_incr;
        int a = (int)at;
        if (a > src_frames - 1)
            a = src_frames - 1;
        int b = (a + 1 < src_frames) ? a + 1 : a;
        const double t = at - (double)a;

        // The source frame, as up to two channels of 16-bit signed.
        Sint32 chan[2] = { 0, 0 };
        for (int c = 0; c < src_ch; c++)
        {
            const Uint8 *pa = cvt->buf + (size_t)(a * src_ch + c) * src_bps;
            const Uint8 *pb = cvt->buf + (size_t)(b * src_ch + c) * src_bps;
            const double va = ReadSample(pa, cvt->src_format);
            const double vb = ReadSample(pb, cvt->src_format);
            chan[c] = (Sint32)(va + (vb - va) * t);
        }

        // Mono to stereo duplicates; stereo to mono averages.
        for (int c = 0; c < dst_ch; c++)
        {
            Sint32 v;
            if (src_ch == dst_ch)
                v = chan[c];
            else if (src_ch == 1)
                v = chan[0];
            else
                v = (chan[0] + chan[1]) / 2;
            out[(size_t)i * dst_ch + c] = (Sint16)v;
        }
    }

    memcpy(cvt->buf, out, out_bytes);
    free(out);
    cvt->len_cvt = (int)out_bytes;
    return 0;
}

} // extern "C"
