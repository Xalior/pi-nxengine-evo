//
// sdl2_render.cpp — two renderer calls NXEngine-evo makes that
// circle-libsdl2 does not have.
//
// Both are built out of calls the shim does have, so there is no drawing
// here that the library is not already doing.
//
#include <SDL2/SDL.h>

extern "C" {

// The shim draws one rectangle at a time; the game asks for four at once
// when it outlines a box.
int SDL_RenderFillRects(SDL_Renderer *renderer, const SDL_Rect *rects,
                        int count)
{
    for (int i = 0; i < count; i++)
        if (SDL_RenderFillRect(renderer, &rects[i]) != 0)
            return -1;
    return 0;
}

// The shim has no viewport of its own to report: drawing always covers the
// whole of the virtual display the kernel declared. So the viewport is that
// display, which is what the renderer's output size already is.
void SDL_RenderGetViewport(SDL_Renderer *renderer, SDL_Rect *rect)
{
    if (rect == nullptr)
        return;
    int w = 0, h = 0;
    SDL_GetRendererOutputSize(renderer, &w, &h);
    rect->x = 0;
    rect->y = 0;
    rect->w = w;
    rect->h = h;
}

} // extern "C"
