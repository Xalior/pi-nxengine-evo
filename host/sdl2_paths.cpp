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

char *SDL_GetBasePath(void)
{
    return Duplicate("/");
}

// The organisation name is ignored, as it is on the platforms where SDL2 has
// nowhere to put it. The application name is not: it is what makes the
// directory, so a second program on the same card gets its own.
char *SDL_GetPrefPath(const char *org, const char *app)
{
    (void)org;

    char path[256];
    if (app == nullptr || *app == '\0')
        app = "app";
    snprintf(path, sizeof(path), "/%s/", app);

    // Trailing separator removed for the call that makes it: FatFs rejects a
    // path ending in one.
    char dir[256];
    snprintf(dir, sizeof(dir), "/%s", app);
    struct stat st;
    if (stat(dir, &st) != 0)
        mkdir(dir, 0777);

    return Duplicate(path);
}

} // extern "C"
