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
// Reading pixels back from the frame now works — circle-libsdl2 implements
// SDL_RenderReadPixels for real — so the screenshot key's remaining problem
// is narrower than it once was: there is a picture to save, and nowhere to
// encode it. Writing a PNG needs an encoder this machine does not have.
//
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include <string>

int png_save_surface(const std::string &filename, SDL_Surface *surf)
{
    (void)filename;
    (void)surf;
    SDL_SetError("writing PNG files is not available: there is no PNG "
                 "encoder on this machine");
    return -1;
}

// Reading is available — circle-libsdl2 has a PNG reader — so this answers
// with it rather than refusing. Nothing in the game calls this, but the
// header declares it and a build should not carry a second, worse answer to
// a question already answered.
SDL_Surface *png_load_surface(const std::string &filename)
{
    return IMG_Load(filename.c_str());
}
