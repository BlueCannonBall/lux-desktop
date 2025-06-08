#include "theme.hpp"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#ifndef _WIN32
    #include "glib.hpp"
    #include <gio/gio.h>
    #include <string.h>
#endif

bool is_dark_mode() {
#ifndef _WIN32
    glib::Object<GSettings> settings = g_settings_new("org.gnome.desktop.interface");
    char* theme = g_settings_get_string(settings.get(), "color-scheme");
    bool ret = strcmp(theme, "default") && strcmp(theme, "prefer-light");
    g_free(theme);
    return ret;
#else
    return false;
#endif
}

static void draw_dark_fltk_up_frame(int x, int y, int w, int h, Fl_Color c) {
    Fl::set_box_color(fl_color_average(fl_rgb_color(79), c, 0.5f));
    fl_xyline(x + 2, y + 1, x + w - 3);
    fl_yxline(x + 1, y + 2, y + h - 3);

    Fl::set_box_color(fl_color_average(FL_BLACK, c, 0.65f));
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

static void draw_dark_fltk_up_box(int x, int y, int w, int h, Fl_Color c) {
    draw_dark_fltk_up_frame(x, y, w, h, c);

    Fl::set_box_color(fl_color_average(fl_rgb_color(79), c, 0.4f));
    fl_xyline(x + 2, y + 2, x + w - 3);
    Fl::set_box_color(fl_color_average(fl_rgb_color(79), c, 0.2f));
    fl_xyline(x + 2, y + 3, x + w - 3);
    Fl::set_box_color(fl_color_average(fl_rgb_color(79), c, 0.1f));
    fl_xyline(x + 2, y + 4, x + w - 3);
    Fl::set_box_color(c);
    fl_rectf(x + 2, y + 5, w - 4, h - 7);
    Fl::set_box_color(fl_color_average(FL_BLACK, c, 0.025f));
    fl_xyline(x + 2, y + h - 4, x + w - 3);
    Fl::set_box_color(fl_color_average(FL_BLACK, c, 0.05f));
    fl_xyline(x + 2, y + h - 3, x + w - 3);
    Fl::set_box_color(fl_color_average(FL_BLACK, c, 0.1f));
    fl_xyline(x + 2, y + h - 2, x + w - 3);
    fl_yxline(x + w - 2, y + 2, y + h - 3);
}

static void draw_dark_fltk_down_frame(int x, int y, int w, int h, Fl_Color c) {
    Fl::set_box_color(fl_color_average(FL_WHITE, c, 0.175f));
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

static void draw_dark_fltk_down_box(int x, int y, int w, int h, Fl_Color c) {
    draw_dark_fltk_down_frame(x, y, w, h, c);

    Fl::set_box_color(c);
    fl_xyline(x + 2, y + 1, x + w - 3);
    fl_xyline(x + 2, y + h - 2, x + w - 3);
    fl_yxline(x + 1, y + 2, y + h - 3);
    fl_yxline(x + w - 2, y + 2, y + h - 3);
    fl_rectf(x + 2, y + 2, w - 4, h - 4);
}

static void draw_light_fltk_up_frame(int x, int y, int w, int h, Fl_Color c) {
    Fl::set_box_color(fl_color_average(FL_WHITE, c, 0.5f));
    fl_xyline(x + 2, y + 1, x + w - 3);
    fl_yxline(x + 1, y + 2, y + h - 3);

    Fl::set_box_color(fl_color_average(FL_BLACK, c, 0.4f));
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

static void draw_light_fltk_up_box(int x, int y, int w, int h, Fl_Color c) {
    draw_light_fltk_up_frame(x, y, w, h, c);

    Fl::set_box_color(fl_color_average(FL_WHITE, c, 0.4f));
    fl_xyline(x + 2, y + 2, x + w - 3);
    Fl::set_box_color(fl_color_average(FL_WHITE, c, 0.2f));
    fl_xyline(x + 2, y + 3, x + w - 3);
    Fl::set_box_color(fl_color_average(FL_WHITE, c, 0.1f));
    fl_xyline(x + 2, y + 4, x + w - 3);
    Fl::set_box_color(c);
    fl_rectf(x + 2, y + 5, w - 4, h - 7);
    Fl::set_box_color(fl_color_average(FL_BLACK, c, 0.025f));
    fl_xyline(x + 2, y + h - 4, x + w - 3);
    Fl::set_box_color(fl_color_average(FL_BLACK, c, 0.05f));
    fl_xyline(x + 2, y + h - 3, x + w - 3);
    Fl::set_box_color(fl_color_average(FL_BLACK, c, 0.1f));
    fl_xyline(x + 2, y + h - 2, x + w - 3);
    fl_yxline(x + w - 2, y + 2, y + h - 3);
}

static void draw_light_fltk_down_frame(int x, int y, int w, int h, Fl_Color c) {
    Fl::set_box_color(fl_color_average(FL_BLACK, c, 0.35f));
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

    Fl::set_box_color(fl_color_average(FL_BLACK, c, 0.075f));
    fl_xyline(x + 2, y + 1, x + w - 3);
    fl_yxline(x + 1, y + 2, y + h - 3);

    Fl::set_box_color(fl_color_average(FL_BLACK, c, 0.04f));
    fl_yxline(x + 2, y + h - 2, y + 2, x + w - 2);
}

static void draw_light_fltk_down_box(int x, int y, int w, int h, Fl_Color c) {
    draw_light_fltk_down_frame(x, y, w, h, c);

    Fl::set_box_color(c);
    fl_rectf(x + 3, y + 3, w - 5, h - 4);
    fl_yxline(x + w - 2, y + 3, y + h - 3);
}

void configure_fltk_colors() {
    Fl::scheme("gtk+");
    if (is_dark_mode()) {
        Fl::set_boxtype(FL_UP_BOX, draw_dark_fltk_up_box, 2, 2, 4, 4);
        Fl::set_boxtype(FL_UP_FRAME, draw_dark_fltk_up_frame, 2, 2, 4, 4);
        Fl::set_boxtype(FL_DOWN_BOX, draw_dark_fltk_down_box, 2, 2, 4, 4);
        Fl::set_boxtype(FL_DOWN_FRAME, draw_dark_fltk_down_frame, 2, 2, 4, 4);
        Fl::background(44, 44, 44);
        Fl::background2(22, 22, 22);
    } else {
        Fl::set_boxtype(FL_UP_BOX, draw_light_fltk_up_box, 2, 2, 4, 4);
        Fl::set_boxtype(FL_UP_FRAME, draw_light_fltk_up_frame, 2, 2, 4, 4);
        Fl::set_boxtype(FL_DOWN_BOX, draw_light_fltk_down_box, 2, 2, 4, 4);
        Fl::set_boxtype(FL_DOWN_FRAME, draw_light_fltk_down_frame, 2, 2, 4, 4);
        Fl::background(224, 224, 224);
        Fl::background2(255, 255, 255);
    }
    Fl::set_color(FL_SELECTION_COLOR, 7, 59, 165);
}
