//
// SDL_mixer.h — the SDL_mixer API surface this port implements.
//
// This is not upstream SDL_mixer's header. It is a declaration of exactly
// the entry points NXEngine-evo calls, so the game's own `#include
// <SDL_mixer.h>` resolves without the real library being present. The
// implementations are in sdl2_mixer.cpp beside it: a channel mixer over the
// shim's SDL_OpenAudioDevice callback.
//
#ifndef _SDL_mixer_h
#define _SDL_mixer_h

#include <SDL2/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

// The version this API surface follows. NXEngine-evo tests the patch level
// to decide between Mix_OpenAudio and Mix_OpenAudioDevice; both are here,
// and the newer call is the one worth taking.
#define SDL_MIXER_MAJOR_VERSION 2
#define SDL_MIXER_MINOR_VERSION 0
#define SDL_MIXER_PATCHLEVEL    4

// Decoder flags. None of these decoders exists here — see Mix_Init — but the
// names are declared so that code naming a format compiles.
#define MIX_INIT_FLAC   0x00000001
#define MIX_INIT_MOD    0x00000002
#define MIX_INIT_MP3    0x00000008
#define MIX_INIT_OGG    0x00000010
#define MIX_INIT_MID    0x00000020
#define MIX_INIT_OPUS   0x00000040

#define MIX_MAX_VOLUME  SDL_MIX_MAXVOLUME
#define MIX_CHANNELS    8

// A block of already-decoded audio in the device's own format.
typedef struct Mix_Chunk
{
    int allocated;      // non-zero when abuf belongs to the mixer
    Uint8 *abuf;
    Uint32 alen;
    Uint8 volume;       // 0 to MIX_MAX_VOLUME
} Mix_Chunk;

// A music stream. Streamed music needs a decoder this port does not have, so
// the type is opaque and Mix_LoadMUS always fails; the music the engine
// synthesises itself arrives through Mix_HookMusic instead.
typedef struct _Mix_Music Mix_Music;

typedef void (*Mix_MusicHook)(void *udata, Uint8 *stream, int len);
typedef void (*Mix_MusicFinishedHook)(void);
typedef void (*Mix_ChannelFinishedHook)(int channel);

// Reports which of the requested decoders are available, which is none of
// them: this returns 0 rather than -1, so a caller checking only for failure
// carries on and a caller checking the flags learns the truth.
int Mix_Init(int flags);
void Mix_Quit(void);

int Mix_OpenAudio(int frequency, Uint16 format, int channels, int chunksize);
int Mix_OpenAudioDevice(int frequency, Uint16 format, int channels,
                        int chunksize, const char *device, int allowed_changes);
void Mix_CloseAudio(void);

int Mix_AllocateChannels(int numchans);

Mix_Chunk *Mix_QuickLoad_RAW(Uint8 *mem, Uint32 len);
void Mix_FreeChunk(Mix_Chunk *chunk);

int Mix_PlayChannelTimed(int channel, Mix_Chunk *chunk, int loops, int ticks);
#define Mix_PlayChannel(channel, chunk, loops) \
    Mix_PlayChannelTimed(channel, chunk, loops, -1)

int Mix_HaltChannel(int channel);
void Mix_Pause(int channel);
void Mix_Resume(int channel);
int Mix_Volume(int channel, int volume);
void Mix_ChannelFinished(Mix_ChannelFinishedHook channel_finished);

void Mix_HookMusic(Mix_MusicHook mix_func, void *arg);
void Mix_HookMusicFinished(Mix_MusicFinishedHook music_finished);

Mix_Music *Mix_LoadMUS(const char *file);
void Mix_FreeMusic(Mix_Music *music);
int Mix_PlayMusic(Mix_Music *music, int loops);
int Mix_VolumeMusic(int volume);
int Mix_SetMusicPosition(double position);
int Mix_HaltMusic(void);
void Mix_PauseMusic(void);
void Mix_ResumeMusic(void);

#define Mix_GetError    SDL_GetError
#define Mix_SetError    SDL_SetError

#ifdef __cplusplus
}
#endif

#endif
