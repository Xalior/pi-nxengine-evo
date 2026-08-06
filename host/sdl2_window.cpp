//
// sdl2_window.cpp — the input-focus flag circle-libsdl2 does not set.
//
// NXEngine-evo decides whether to run at all by asking the window for its
// flags: a window that is shown, not minimised, and holds the input focus is
// a window worth drawing into, and anything else means the player has
// alt-tabbed away, so the game pauses in a loop until the flags say
// otherwise. circle-libsdl2 marks its window shown and fullscreen but never
// sets SDL_WINDOW_INPUT_FOCUS, so the game would pause on its first frame and
// never resume — a black screen with nothing wrong in the log.
//
// On this machine the window always has the focus: there is one display and
// no way to move away from it. So the flag is added here, by wrapping the
// library's own call at link time (see WRAPPED_SDL in the Makefile) rather
// than by replacing it. Everything else this file used to stand in for —
// resizing, fullscreen toggling, a window icon — is now the library's own,
// so only the wrapper remains.
//
#include <SDL2/SDL.h>

extern "C" {

Uint32 __real_SDL_GetWindowFlags(SDL_Window *window);

Uint32 __wrap_SDL_GetWindowFlags(SDL_Window *window)
{
    const Uint32 flags = __real_SDL_GetWindowFlags(window);
    return window != nullptr ? (flags | SDL_WINDOW_INPUT_FOCUS) : flags;
}

} // extern "C"
