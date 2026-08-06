//
// nx_pngfuncs.cpp — the two functions upstream's src/graphics/pngfuncs.cpp
// provides, without libpng.
//
// Upstream writes screenshots with libpng, which is not on this machine and
// is not worth carrying: nothing else in the game needs it, and the only
// caller is the screenshot key. So that one source file is left out of this
// build and this one is compiled in its place, keeping the upstream checkout
// untouched. The declarations both files answer to are in upstream's
// pngfuncs.h, which is what the game includes.
//
// Saving a screenshot cannot work here for a reason further up anyway: the
// finished picture is assembled on the presentation core and handed to the
// framebuffer, so SDL_RenderReadPixels has nothing to hand back and fails
// first. This function is never reached with a real surface.
//
#include <SDL2/SDL.h>

#include <string>

extern "C" SDL_Surface *IMG_Load(const char *file);

int png_save_surface(const std::string &filename, SDL_Surface *surf)
{
    (void)filename;
    (void)surf;
    SDL_SetError("writing PNG files is not available");
    return -1;
}

// Reading is available — sdl2_image.cpp has a PNG reader — so this answers
// with it rather than refusing. Nothing in the game calls this, but the
// header declares it and a build should not carry a second, worse answer to
// a question already answered.
SDL_Surface *png_load_surface(const std::string &filename)
{
    return IMG_Load(filename.c_str());
}
