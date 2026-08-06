//
// sdl2_mixer.cpp — the SDL_mixer API NXEngine-evo uses, over the shim's
// audio device.
//
// The game's sound comes from two places and they arrive here by different
// routes. Its sound effects are synthesised by Pixtone into blocks of PCM
// and played on numbered channels. Its music is synthesised by Organya, one
// buffer at a time, through the music hook — the callback SDL_mixer offers a
// program that would rather generate music than play a file. This file mixes
// the two together and hands the result to circle-libsdl2's audio callback.
//
// STREAMED MUSIC IS NOT AVAILABLE. Cave Story's optional replacement
// soundtracks are Ogg Vorbis files, and there is no Vorbis decoder on this
// machine, so Mix_LoadMUS fails and says so. The engine treats that as "this
// music directory does not work" and plays the Organya soundtrack, which is
// the original one and needs no decoder at all.
//
// NO LOCKING. In circle-libsdl2 the audio callback runs on the application's
// own core, called from SDL_PumpEvents — the same core, in the same call
// stack, as the game code that starts and stops sounds. There is no second
// thread to race with, so there is nothing here to lock. That is a property
// of this SDL implementation, not of SDL_mixer, and it is why this file has
// no critical sections in it.
//
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

#include <cstdlib>
#include <cstring>
#include <vector>

namespace
{

struct Channel
{
    Mix_Chunk *chunk = nullptr;
    Uint32 pos = 0;          // bytes played from the chunk
    int loops = 0;           // remaining repeats; -1 forever
    bool paused = false;
    int volume = MIX_MAX_VOLUME;
};

std::vector<Channel> s_channels;
bool s_open = false;
SDL_AudioDeviceID s_device = 0;
SDL_AudioSpec s_spec;

Mix_MusicHook s_music_hook = nullptr;
void *s_music_arg = nullptr;
Mix_MusicFinishedHook s_music_finished = nullptr;
Mix_ChannelFinishedHook s_channel_finished = nullptr;
int s_music_volume = MIX_MAX_VOLUME;

// Saturating 16-bit add, which is what mixing two signals into one is.
inline Sint16 MixSample(Sint32 a, Sint32 b)
{
    const Sint32 v = a + b;
    if (v > 32767) return 32767;
    if (v < -32768) return -32768;
    return (Sint16)v;
}

// One channel's contribution to the output block. Returns false when the
// chunk has finished and the channel should be freed.
bool MixChannel(Channel &c, Sint16 *out, int frames)
{
    const Uint32 total = c.chunk->alen;
    Sint16 *dst = out;
    int left = frames * 2;      // the device is stereo

    while (left > 0)
    {
        if (c.pos >= total)
        {
            if (c.loops == 0)
                return false;
            if (c.loops > 0)
                c.loops--;
            c.pos = 0;
        }

        const Uint32 avail_bytes = total - c.pos;
        int avail = (int)(avail_bytes / 2);
        if (avail > left)
            avail = left;
        if (avail <= 0)
            return false;

        const Sint16 *src = (const Sint16 *)(c.chunk->abuf + c.pos);
        const Sint32 vol = (Sint32)c.volume * c.chunk->volume;
        for (int i = 0; i < avail; i++)
        {
            // Two volume settings apply to every sample: the channel's and
            // the chunk's own, each 0 to MIX_MAX_VOLUME.
            const Sint32 s = ((Sint32)src[i] * vol) / (MIX_MAX_VOLUME * MIX_MAX_VOLUME);
            dst[i] = MixSample(dst[i], s);
        }
        dst += avail;
        left -= avail;
        c.pos += (Uint32)avail * 2;
    }
    return true;
}

void AudioCallback(void *, Uint8 *stream, int len)
{
    const int frames = len / 4;         // 16-bit stereo

    // The music hook fills the block; without one the block starts silent.
    // This is SDL_mixer's own order: music first, effects mixed on top.
    if (s_music_hook != nullptr)
        s_music_hook(s_music_arg, stream, len);
    else
        memset(stream, 0, (size_t)len);

    Sint16 *out = (Sint16 *)stream;
    for (size_t i = 0; i < s_channels.size(); i++)
    {
        Channel &c = s_channels[i];
        if (c.chunk == nullptr || c.paused)
            continue;
        if (!MixChannel(c, out, frames))
        {
            c.chunk = nullptr;
            if (s_channel_finished != nullptr)
                s_channel_finished((int)i);
        }
    }
}

} // namespace

extern "C" {

// No file decoders exist here, so none of the requested formats is
// available. Reporting that as zero rather than -1 is deliberate: -1 means
// the mixer itself could not start, and it can.
int Mix_Init(int) { return 0; }

void Mix_Quit(void) {}

int Mix_OpenAudioDevice(int frequency, Uint16 format, int channels,
                        int chunksize, const char *, int)
{
    if (s_open)
    {
        Mix_SetError("the mixer is already open");
        return -1;
    }
    if (format != AUDIO_S16SYS || channels != 2)
    {
        Mix_SetError("only 16-bit signed stereo output is implemented");
        return -1;
    }

    SDL_AudioSpec want;
    memset(&want, 0, sizeof(want));
    want.freq = frequency;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = (Uint16)chunksize;
    want.callback = AudioCallback;

    s_device = SDL_OpenAudioDevice(nullptr, 0, &want, &s_spec, 0);
    if (s_device == 0)
        return -1;

    s_channels.assign(MIX_CHANNELS, Channel());
    s_open = true;
    SDL_PauseAudioDevice(s_device, 0);
    return 0;
}

int Mix_OpenAudio(int frequency, Uint16 format, int channels, int chunksize)
{
    return Mix_OpenAudioDevice(frequency, format, channels, chunksize,
                               nullptr, 0);
}

void Mix_CloseAudio(void)
{
    if (!s_open)
        return;
    SDL_CloseAudioDevice(s_device);
    s_device = 0;
    s_open = false;
    s_channels.clear();
}

int Mix_AllocateChannels(int numchans)
{
    if (numchans < 0)
        return (int)s_channels.size();
    s_channels.resize((size_t)numchans);
    return numchans;
}

// The block belongs to the mixer from here on, which is what "quick load"
// means: no decoding, no copy, and Mix_FreeChunk frees it.
Mix_Chunk *Mix_QuickLoad_RAW(Uint8 *mem, Uint32 len)
{
    Mix_Chunk *chunk = (Mix_Chunk *)malloc(sizeof(Mix_Chunk));
    if (chunk == nullptr)
    {
        Mix_SetError("out of memory");
        return nullptr;
    }
    chunk->allocated = 0;
    chunk->abuf = mem;
    chunk->alen = len;
    chunk->volume = MIX_MAX_VOLUME;
    return chunk;
}

void Mix_FreeChunk(Mix_Chunk *chunk)
{
    if (chunk == nullptr)
        return;
    for (auto &c : s_channels)
        if (c.chunk == chunk)
            c.chunk = nullptr;
    if (chunk->allocated)
        free(chunk->abuf);
    free(chunk);
}

int Mix_PlayChannelTimed(int channel, Mix_Chunk *chunk, int loops, int ticks)
{
    if (!s_open || chunk == nullptr)
        return -1;
    if (ticks >= 0)
    {
        Mix_SetError("time-limited playback is not implemented");
        return -1;
    }

    if (channel < 0)
    {
        channel = -1;
        for (size_t i = 0; i < s_channels.size(); i++)
            if (s_channels[i].chunk == nullptr)
            {
                channel = (int)i;
                break;
            }
        if (channel < 0)
        {
            Mix_SetError("every channel is in use");
            return -1;
        }
    }
    if ((size_t)channel >= s_channels.size())
        return -1;

    Channel &c = s_channels[(size_t)channel];
    c.chunk = chunk;
    c.pos = 0;
    c.loops = loops;
    c.paused = false;
    return channel;
}

int Mix_HaltChannel(int channel)
{
    if (channel < 0)
    {
        for (auto &c : s_channels)
            c.chunk = nullptr;
        return 0;
    }
    if ((size_t)channel < s_channels.size())
        s_channels[(size_t)channel].chunk = nullptr;
    return 0;
}

void Mix_Pause(int channel)
{
    if (channel < 0)
        for (auto &c : s_channels) c.paused = true;
    else if ((size_t)channel < s_channels.size())
        s_channels[(size_t)channel].paused = true;
}

void Mix_Resume(int channel)
{
    if (channel < 0)
        for (auto &c : s_channels) c.paused = false;
    else if ((size_t)channel < s_channels.size())
        s_channels[(size_t)channel].paused = false;
}

int Mix_Volume(int channel, int volume)
{
    if (volume > MIX_MAX_VOLUME)
        volume = MIX_MAX_VOLUME;

    if (channel < 0)
    {
        int previous = s_channels.empty() ? 0 : s_channels[0].volume;
        if (volume >= 0)
            for (auto &c : s_channels) c.volume = volume;
        return previous;
    }
    if ((size_t)channel >= s_channels.size())
        return 0;

    int previous = s_channels[(size_t)channel].volume;
    if (volume >= 0)
        s_channels[(size_t)channel].volume = volume;
    return previous;
}

void Mix_ChannelFinished(Mix_ChannelFinishedHook channel_finished)
{
    s_channel_finished = channel_finished;
}

void Mix_HookMusic(Mix_MusicHook mix_func, void *arg)
{
    s_music_hook = mix_func;
    s_music_arg = arg;
}

void Mix_HookMusicFinished(Mix_MusicFinishedHook music_finished)
{
    s_music_finished = music_finished;
}

// ---- streamed music ---------------------------------------------------------
//
// Nothing here can decode a music file, so loading one fails and everything
// downstream of a loaded song is unreachable. The calls still exist because
// the game makes them unconditionally when it shuts a song down.

Mix_Music *Mix_LoadMUS(const char *)
{
    Mix_SetError("no music file decoder is available");
    return nullptr;
}

void Mix_FreeMusic(Mix_Music *) {}

int Mix_PlayMusic(Mix_Music *, int)
{
    Mix_SetError("no music file decoder is available");
    return -1;
}

int Mix_VolumeMusic(int volume)
{
    const int previous = s_music_volume;
    if (volume >= 0)
        s_music_volume = (volume > MIX_MAX_VOLUME) ? MIX_MAX_VOLUME : volume;
    return previous;
}

int Mix_SetMusicPosition(double)
{
    Mix_SetError("no music file decoder is available");
    return -1;
}

int Mix_HaltMusic(void)
{
    if (s_music_finished != nullptr)
        s_music_finished();
    return 0;
}

void Mix_PauseMusic(void) {}
void Mix_ResumeMusic(void) {}

} // extern "C"
