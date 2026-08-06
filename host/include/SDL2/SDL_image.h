//
// SDL_image.h — the SDL_image API surface this port implements.
//
// This is not upstream SDL_image's header. It is a declaration of exactly
// the entry points NXEngine-evo calls, so the game's own `#include
// <SDL_image.h>` resolves without the real library being present. The
// implementations are in sdl2_image.cpp beside it: a PNG reader, which is
// the one format the game asks IMG_Init for.
//
#ifndef _SDL_image_h
#define _SDL_image_h

#include <SDL2/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialisation flags. Only PNG is implemented; the others are declared so
// that code testing for them compiles, and IMG_Init reports them unavailable.
#define IMG_INIT_JPG   0x00000001
#define IMG_INIT_PNG   0x00000002
#define IMG_INIT_TIF   0x00000004
#define IMG_INIT_WEBP  0x00000008

// Returns the subset of `flags` that is available, which is the caller's
// cue that a format it needs is missing.
int IMG_Init(int flags);
void IMG_Quit(void);

// Reads an image file into a newly allocated surface, or returns NULL and
// sets the error string. The surface is 32 bits per pixel whatever the file
// held.
SDL_Surface *IMG_Load(const char *file);

#define IMG_GetError    SDL_GetError
#define IMG_SetError    SDL_SetError

#ifdef __cplusplus
}
#endif

#endif
