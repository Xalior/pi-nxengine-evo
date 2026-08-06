//
// circle_stubs.cpp — the SDL2 entry points NXEngine-evo references that
// circle-libsdl2 does not implement, and that this port does not implement
// either.
//
// Every function here either does the only sensible thing on a bare-metal
// board with one fixed display, or fails honestly rather than pretending to
// work. Nothing here guesses.
//
// These are seams, not permanent furniture: when the shim implements one of
// these for real, the way to adopt it is to DELETE the stub here. An object
// file linked directly into the kernel beats an archive member, so a stub
// left behind would silently win over the working implementation — which is
// why the shim archive is pulled in whole and a leftover stub becomes a
// link error instead of a silent regression.
//
#include <SDL2/SDL.h>

#include <cstdio>
#include <cstring>

extern "C" {

// ---- the window -------------------------------------------------------------
//
// There is one display and the game is always on all of it. The size the
// application is given is the virtual display the kernel declared before
// SDL_Init, and it cannot change while the machine runs — so a request to
// resize the window, or to enter or leave fullscreen, has nothing to do and
// reports success. The engine calls both from its options menu.

int SDL_SetWindowFullscreen(SDL_Window *, Uint32) { return 0; }
void SDL_SetWindowSize(SDL_Window *, int, int) {}

// No desktop, no window decoration, nowhere to put an icon.
void SDL_SetWindowIcon(SDL_Window *, SDL_Surface *) {}

// ---- messages ---------------------------------------------------------------
//
// The engine puts its fatal errors in a message box. There is no one at a
// mouse to dismiss one, so it goes to the serial console, which is where
// everything else the machine has to say already is.

int SDL_ShowSimpleMessageBox(Uint32, const char *title, const char *message,
                             SDL_Window *)
{
    printf("%s: %s\n", title != nullptr ? title : "message",
           message != nullptr ? message : "");
    return 0;
}

// ---- reading pixels back ----------------------------------------------------
//
// Used only by the screenshot key. The shim's renderer composes each frame
// on the presentation core and hands it to the framebuffer; there is no
// finished picture to read back from the application core, so this fails and
// the screenshot does not happen.

int SDL_RenderReadPixels(SDL_Renderer *, const SDL_Rect *, Uint32, void *, int)
{
    SDL_SetError("reading rendered pixels back is not available");
    return -1;
}

} // extern "C"
