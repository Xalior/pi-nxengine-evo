//
// sdl2_paths.cpp — where the game's files are, on a machine with no shell.
//
// NXEngine-evo asks SDL two questions to build every path it uses:
// SDL_GetBasePath for the directory it was installed in, and SDL_GetPrefPath
// for a directory it may write to. On a desktop those answer with the
// program's own location and the user's home; on a card there is one
// filesystem and no user, so both are stated here.
//
//   /                 the base. The engine looks for its read-only files
//                     under <base>data/, so the card carries a top-level
//                     data/ directory: the engine's own data/ out of the
//                     upstream checkout, with Cave Story's extracted files
//                     alongside it.
//   /nxengine/        the preferences directory: settings, saved games and
//                     the debug log. Created here if it is not already on the
//                     card, because SDL2 guarantees that this directory
//                     exists when it answers.
//
// The engine searches the preferences directory FIRST for read-only files
// too, so a player can override any data file by dropping a copy under
// /nxengine/data/ without touching the shipped one.
//
#include <SDL2/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>

namespace
{

// SDL2 hands back memory the caller frees with SDL_free, which on this
// platform is plain free().
char *Duplicate(const char *s)
{
    const size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p != nullptr)
        memcpy(p, s, n);
    return p;
}

} // namespace

extern "C" {

// Where the game's own files live: RAPI_GAME_DIR, this game's directory on
// the card and nowhere else. A card carries several games, and answering "/"
// here would put this one's resources in the root among all of them.
char *SDL_GetBasePath(void)
{
    return Duplicate(RAPI_GAME_DIR "/");
}

// Saved games and settings, in that same directory. The organisation and
// application names are ignored, as the organisation name is on every
// platform where SDL2 has nowhere to put it: the card directory is already
// this game's alone, so deriving a further directory from a name the engine
// chooses would only bury the saves one level deeper than the data.
char *SDL_GetPrefPath(const char *org, const char *app)
{
    (void)org;
    (void)app;

    // Made if it is not there, because saving is the first thing that needs
    // it and the card may carry only the read-only tree. The trailing
    // separator is left off the call that makes it: FatFs rejects a path
    // ending in one.
    struct stat st;
    if (stat(RAPI_GAME_DIR, &st) != 0)
        mkdir(RAPI_GAME_DIR, 0777);

    return Duplicate(RAPI_GAME_DIR "/");
}

} // extern "C"
