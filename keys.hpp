#pragma once

#include <SDL2/SDL_keycode.h>
#include <string_view>

std::string_view sdl_to_browser_key(SDL_Keycode key);
