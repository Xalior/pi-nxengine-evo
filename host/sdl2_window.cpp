//
// sdl2_window.cpp — the window, on a board with one screen and no desktop.
//
// There is one display and the game is always on all of it. The size the
// application is given is the virtual display the kernel declared before
// SDL_Init, and it cannot change while the machine runs — so a request to
// resize the window, or to enter or leave fullscreen, has nothing to do and
// reports success. The engine makes both from its options menu.
//
// THE FOCUS FLAG IS WHY THIS FILE HAS A WRAPPER IN IT. NXEngine-evo decides
// whether to run at all by asking the window for its flags: a window that is
// shown, not minimised, and holds the input focus is a window worth drawing
// into, and anything else means the player has alt-tabbed away, so the game
// pauses in a loop until the flags say otherwise. circle-libsdl2 marks its
// window shown and fullscreen but never sets SDL_WINDOW_INPUT_FOCUS, so the
// game would pause on its first frame and never resume — a black screen with
// nothing wrong in the log.
//
// On this machine the window always has the focus: there is no other window
// and no way to move away from it. So the flag is added here, by wrapping the
// library's own call at link time (see WRAPPED_SDL in the Makefile) rather
// than by replacing it.
//
#include <SDL2/SDL.h>

extern "C" {

Uint32 __real_SDL_GetWindowFlags(SDL_Window *window);

Uint32 __wrap_SDL_GetWindowFlags(SDL_Window *window)
{
    const Uint32 flags = __real_SDL_GetWindowFlags(window);
    return window != nullptr ? (flags | SDL_WINDOW_INPUT_FOCUS) : flags;
}

int SDL_SetWindowFullscreen(SDL_Window *, Uint32) { return 0; }
void SDL_SetWindowSize(SDL_Window *, int, int) {}

// No desktop, no window decoration, nowhere to put an icon.
void SDL_SetWindowIcon(SDL_Window *, SDL_Surface *) {}

} // extern "C"
