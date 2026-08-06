//
// sdl2_stdinc.cpp — SDL2's own names for three C library calls.
//
// SDL2 publishes the C library it uses under its own names, so that an
// application can call SDL_memcpy on a platform where there is no C library
// at all. circle-libsdl2 provides the allocator half of that family —
// SDL_malloc, SDL_calloc, SDL_realloc, SDL_free — but not the memory and
// string half, and NXEngine-evo uses three of them: SDL_memcpy and
// SDL_memset while it is building sound buffers, and SDL_strdup once.
//
// There is a C library here, and it is the one SDL would be forwarding to
// anyway, so these are one line each.
//
#include <SDL2/SDL.h>

#include <cstdlib>
#include <cstring>

extern "C" {

void *SDL_memcpy(void *dst, const void *src, size_t len)
{
    return memcpy(dst, src, len);
}

void *SDL_memset(void *dst, int c, size_t len)
{
    return memset(dst, c, len);
}

// The caller frees this with SDL_free, which on this platform is free().
char *SDL_strdup(const char *str)
{
    if (str == nullptr)
        str = "";
    const size_t n = strlen(str) + 1;
    char *p = (char *)malloc(n);
    if (p != nullptr)
        memcpy(p, str, n);
    return p;
}

} // extern "C"
