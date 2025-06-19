#pragma once

#include <string>
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

bool is_dark_mode();
std::string get_gtk_theme();
void configure_fltk_colors();
#ifdef _WIN32
void set_window_dark_mode(HWND window, bool value = is_dark_mode());
#endif
