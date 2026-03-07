#include "theme.hpp"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#ifdef _WIN32
    #include <dwmapi.h>
#else
    #include "glib.hpp"
    #include <gio/gio.h>
    #include <string.h>
#endif

bool is_dark_mode() {
#ifdef _WIN32
    if (HKEY key; RegOpenKeyEx(
                      HKEY_CURRENT_USER,
                      "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                      0,
                      KEY_READ,
                      &key) == ERROR_SUCCESS) {
        if (DWORD value, size = sizeof value; RegQueryValueExA(key, "AppsUseLightTheme", nullptr, nullptr, (LPBYTE) &value, &size) == ERROR_SUCCESS) {
            RegCloseKey(key);
            return !value;
        }
        RegCloseKey(key);
    }
    return false;
#else
    glib::Object<GSettings> settings = g_settings_new("org.gnome.desktop.interface");
    char* theme = g_settings_get_string(settings.get(), "color-scheme");
    bool ret = strcmp(theme, "default") && strcmp(theme, "prefer-light");
    g_free(theme);
    return ret;
#endif
}

static void draw_modern_frame(int x, int y, int w, int h, Fl_Color c) {
    fl_begin_loop();
    fl_vertex(x, y + 2);
    fl_vertex(x + 2, y);
    fl_vertex(x + w - 3, y);
    fl_vertex(x + w - 1, y + 2);
    fl_vertex(x + w - 1, y + h - 3);
    fl_vertex(x + w - 3, y + h - 1);
    fl_vertex(x + 2, y + h - 1);
    fl_vertex(x, y + h - 3);
    fl_end_loop();
}

static void draw_modern_box(int x, int y, int w, int h, Fl_Color bg, Fl_Color border) {
    Fl::set_box_color(bg);
    fl_begin_polygon();
    fl_vertex(x, y + 2);
    fl_vertex(x + 2, y);
    fl_vertex(x + w - 3, y);
    fl_vertex(x + w - 1, y + 2);
    fl_vertex(x + w - 1, y + h - 3);
    fl_vertex(x + w - 3, y + h - 1);
    fl_vertex(x + 2, y + h - 1);
    fl_vertex(x, y + h - 3);
    fl_end_polygon();

    Fl::set_box_color(border);
    draw_modern_frame(x, y, w, h, border);
}

static void draw_dark_fltk_up_frame(int x, int y, int w, int h, Fl_Color c) {
    Fl::set_box_color(fl_color_average(FL_WHITE, c, 0.15f));
    draw_modern_frame(x, y, w, h, c);
}

static void draw_dark_fltk_up_box(int x, int y, int w, int h, Fl_Color c) {
    Fl_Color bg = (c <= 255) ? fl_color_average(FL_WHITE, c, 0.05f) : c;
    draw_modern_box(x, y, w, h, bg, fl_color_average(FL_WHITE, c, 0.12f));
}

static void draw_dark_fltk_down_frame(int x, int y, int w, int h, Fl_Color c) {
    Fl::set_box_color(fl_color_average(FL_BLACK, c, 0.2f));
    draw_modern_frame(x, y, w, h, c);
}

static void draw_dark_fltk_down_box(int x, int y, int w, int h, Fl_Color c) {
    draw_modern_box(x, y, w, h, c, fl_color_average(FL_WHITE, Fl::get_color(FL_BACKGROUND_COLOR), 0.08f));
}

static void draw_light_fltk_up_frame(int x, int y, int w, int h, Fl_Color c) {
    Fl::set_box_color(fl_color_average(FL_BLACK, c, 0.2f));
    draw_modern_frame(x, y, w, h, c);
}

static void draw_light_fltk_up_box(int x, int y, int w, int h, Fl_Color c) {
    Fl_Color bg = (c <= 255) ? fl_color_average(FL_WHITE, c, 0.4f) : c;
    draw_modern_box(x, y, w, h, bg, fl_color_average(FL_BLACK, c, 0.2f));
}

static void draw_light_fltk_down_frame(int x, int y, int w, int h, Fl_Color c) {
    Fl::set_box_color(fl_color_average(FL_BLACK, c, 0.3f));
    draw_modern_frame(x, y, w, h, c);
}

static void draw_light_fltk_down_box(int x, int y, int w, int h, Fl_Color c) {
    draw_modern_box(x, y, w, h, c, fl_color_average(FL_BLACK, c, 0.25f));
}

void configure_fltk_colors() {
    Fl::scheme("gtk+");
    if (is_dark_mode()) {
        Fl::set_boxtype(FL_UP_BOX, draw_dark_fltk_up_box, 2, 2, 4, 4);
        Fl::set_boxtype(FL_UP_FRAME, draw_dark_fltk_up_frame, 2, 2, 4, 4);
        Fl::set_boxtype(FL_DOWN_BOX, draw_dark_fltk_down_box, 2, 2, 4, 4);
        Fl::set_boxtype(FL_DOWN_FRAME, draw_dark_fltk_down_frame, 2, 2, 4, 4);
        Fl::foreground(245, 245, 245);
        Fl::background(40, 40, 40);
        Fl::background2(24, 24, 24);
        Fl::set_color(FL_SELECTION_COLOR, 0, 120, 215);
    } else {
        Fl::set_boxtype(FL_UP_BOX, draw_light_fltk_up_box, 2, 2, 4, 4);
        Fl::set_boxtype(FL_UP_FRAME, draw_light_fltk_up_frame, 2, 2, 4, 4);
        Fl::set_boxtype(FL_DOWN_BOX, draw_light_fltk_down_box, 2, 2, 4, 4);
        Fl::set_boxtype(FL_DOWN_FRAME, draw_light_fltk_down_frame, 2, 2, 4, 4);
        Fl::foreground(32, 32, 32);
        Fl::background(235, 235, 235);  // Visibly gray so the white input fields starkly contrast against it
        Fl::background2(255, 255, 255); // Pure stark white for entry fields
        Fl::set_color(FL_SELECTION_COLOR, 7, 59, 165);
    }
}

#ifdef _WIN32
void set_window_dark_mode(HWND window, bool value) {
    BOOL use_immersive_dark_mode = value;
    DwmSetWindowAttribute(window, DWMWA_USE_IMMERSIVE_DARK_MODE, &use_immersive_dark_mode, sizeof use_immersive_dark_mode);
}
#endif
