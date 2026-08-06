//
// sdl2_keyname.cpp — the printable name of a key.
//
// NXEngine-evo shows the current binding beside each action in its controls
// menu, and asks SDL for the text. SDL2 builds that name from the keycode:
// a printable key is named by its own character in upper case, and everything
// else has a fixed English name. This is that, for the keys a keyboard on
// this machine can produce.
//
// SDL2 returns the empty string, not a null pointer, for a key it cannot
// name, and the storage belongs to SDL. Both hold here.
//
#include <SDL2/SDL.h>

#include <cstdio>

extern "C" {

const char *SDL_GetKeyName(SDL_Keycode key)
{
    // One buffer, overwritten by each call, exactly as SDL2's own is. The
    // caller copies the text before asking about another key — which is what
    // the engine does.
    static char s_name[16];

    switch (key)
    {
    case SDLK_RETURN:       return "Return";
    case SDLK_ESCAPE:       return "Escape";
    case SDLK_BACKSPACE:    return "Backspace";
    case SDLK_TAB:          return "Tab";
    case SDLK_SPACE:        return "Space";
    case SDLK_DELETE:       return "Delete";
    case SDLK_CAPSLOCK:     return "CapsLock";
    case SDLK_PRINTSCREEN:  return "PrintScreen";
    case SDLK_SCROLLLOCK:   return "ScrollLock";
    case SDLK_PAUSE:        return "Pause";
    case SDLK_INSERT:       return "Insert";
    case SDLK_HOME:         return "Home";
    case SDLK_PAGEUP:       return "PageUp";
    case SDLK_END:          return "End";
    case SDLK_PAGEDOWN:     return "PageDown";
    case SDLK_RIGHT:        return "Right";
    case SDLK_LEFT:         return "Left";
    case SDLK_DOWN:         return "Down";
    case SDLK_UP:           return "Up";
    case SDLK_LCTRL:        return "Left Ctrl";
    case SDLK_LSHIFT:       return "Left Shift";
    case SDLK_LALT:         return "Left Alt";
    case SDLK_LGUI:         return "Left GUI";
    case SDLK_RCTRL:        return "Right Ctrl";
    case SDLK_RSHIFT:       return "Right Shift";
    case SDLK_RALT:         return "Right Alt";
    case SDLK_RGUI:         return "Right GUI";
    default:                break;
    }

    if (key >= SDLK_F1 && key <= SDLK_F12)
    {
        snprintf(s_name, sizeof(s_name), "F%d", (int)(key - SDLK_F1) + 1);
        return s_name;
    }

    // Printable ASCII names itself, in upper case, as SDL2 does.
    if (key > 0x20 && key < 0x7F)
    {
        s_name[0] = (char)((key >= 'a' && key <= 'z') ? key - 'a' + 'A' : key);
        s_name[1] = '\0';
        return s_name;
    }

    return "";
}

} // extern "C"
