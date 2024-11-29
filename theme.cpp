// This theme is from Tilemap Studio project, available at: https://github.com/Rangi42/tilemap-studio
// It is licensed under the terms of the LGPL license, available at: https://github.com/Rangi42/tilemap-studio/blob/master/LICENSE.md
// This code has been lightly modified and reformatted for use in Lux Desktop

#include <FL/Fl.H>
#include <FL/Fl_PNG_Image.H>
#include <FL/Fl_Tooltip.H>
#include <FL/fl_draw.H>

#define OS_FONT FL_FREE_FONT

#ifdef _WIN32
    #define OS_FONT_SIZE 12
#else
    #define OS_FONT_SIZE 13
#endif

#define OS_BUTTON_UP_BOX              FL_GTK_UP_BOX
#define OS_CHECK_DOWN_BOX             FL_GTK_DOWN_BOX
#define OS_BUTTON_UP_FRAME            FL_GTK_UP_FRAME
#define OS_CHECK_DOWN_FRAME           FL_GTK_DOWN_FRAME
#define OS_PANEL_THIN_UP_BOX          FL_GTK_THIN_UP_BOX
#define OS_SPACER_THIN_DOWN_BOX       FL_GTK_THIN_DOWN_BOX
#define OS_PANEL_THIN_UP_FRAME        FL_GTK_THIN_UP_FRAME
#define OS_SPACER_THIN_DOWN_FRAME     FL_GTK_THIN_DOWN_FRAME
#define OS_RADIO_ROUND_DOWN_BOX       FL_GTK_ROUND_DOWN_BOX
#define OS_HOVERED_UP_BOX             FL_PLASTIC_UP_BOX
#define OS_DEPRESSED_DOWN_BOX         FL_PLASTIC_DOWN_BOX
#define OS_HOVERED_UP_FRAME           FL_PLASTIC_UP_FRAME
#define OS_DEPRESSED_DOWN_FRAME       FL_PLASTIC_DOWN_FRAME
#define OS_INPUT_THIN_DOWN_BOX        FL_PLASTIC_THIN_DOWN_BOX
#define OS_INPUT_THIN_DOWN_FRAME      FL_PLASTIC_ROUND_DOWN_BOX
#define OS_MINI_BUTTON_UP_BOX         FL_GLEAM_UP_BOX
#define OS_MINI_DEPRESSED_DOWN_BOX    FL_GLEAM_DOWN_BOX
#define OS_MINI_BUTTON_UP_FRAME       FL_GLEAM_UP_FRAME
#define OS_MINI_DEPRESSED_DOWN_FRAME  FL_GLEAM_DOWN_FRAME
#define OS_DEFAULT_BUTTON_UP_BOX      FL_DIAMOND_UP_BOX
#define OS_DEFAULT_HOVERED_UP_BOX     FL_PLASTIC_THIN_UP_BOX
#define OS_DEFAULT_DEPRESSED_DOWN_BOX FL_DIAMOND_DOWN_BOX
#define OS_TOOLBAR_BUTTON_HOVER_BOX   FL_GLEAM_ROUND_UP_BOX
#define OS_TABS_BOX                   FL_EMBOSSED_BOX
#define OS_SWATCH_BOX                 FL_ENGRAVED_BOX
#define OS_SWATCH_FRAME               FL_ENGRAVED_FRAME
#define OS_BG_BOX                     FL_FREE_BOXTYPE
#define OS_BG_DOWN_BOX                (Fl_Boxtype)(FL_FREE_BOXTYPE + 1)
#define OS_TOOLBAR_FRAME              (Fl_Boxtype)(FL_FREE_BOXTYPE + 2)

#define OS_TAB_COLOR FL_FREE_COLOR

Fl_Color activated_color(Fl_Color c) {
    return Fl::draw_box_active() ? c : fl_inactive(c);
}

void vertical_gradient(int x1, int y1, int x2, int y2, Fl_Color c1, Fl_Color c2) {
    int imax = y2 - y1;
    int d = imax ? imax : 1;
    if (Fl::draw_box_active()) {
        for (int i = 0; i <= imax; i++) {
            float w = 1.0f - (float) i / d;
            fl_color(fl_color_average(c1, c2, w));
            fl_xyline(x1, y1 + i, x2);
        }
    } else {
        for (int i = 0; i <= imax; i++) {
            float w = 1.0f - (float) i / d;
            fl_color(fl_inactive(fl_color_average(c1, c2, w)));
            fl_xyline(x1, y1 + i, x2);
        }
    }
}

void horizontal_gradient(int x1, int y1, int x2, int y2, Fl_Color c1, Fl_Color c2) {
    int imax = x2 - x1;
    int d = imax ? imax : 1;
    if (Fl::draw_box_active()) {
        for (int i = 0; i <= imax; i++) {
            float w = 1.0f - (float) i / d;
            fl_color(fl_color_average(c1, c2, w));
            fl_yxline(x1 + i, y1, y2);
        }
    } else {
        for (int i = 0; i <= imax; i++) {
            float w = 1.0f - (float) i / d;
            fl_color(fl_inactive(fl_color_average(c1, c2, w)));
            fl_yxline(x1 + i, y1, y2);
        }
    }
}

void greybird_button_up_frame(int x, int y, int w, int h, Fl_Color) {
    // top outer border
    fl_color(activated_color(fl_rgb_color(0xA6, 0xA6, 0xA6)));
    fl_xyline(x + 2, y, x + w - 3);
    // side outer borders
    fl_color(activated_color(fl_rgb_color(0x96, 0x96, 0x96)));
    fl_yxline(x, y + 2, y + h - 3);
    fl_yxline(x + w - 1, y + 2, y + h - 3);
    // bottom outer border
    fl_color(activated_color(fl_rgb_color(0x87, 0x87, 0x87)));
    fl_xyline(x + 2, y + h - 1, x + w - 3);
    // top inner border
    fl_color(activated_color(fl_rgb_color(0xEE, 0xEE, 0xEE)));
    fl_xyline(x + 2, y + 1, x + w - 3);
    // side inner borders
    fl_color(activated_color(fl_rgb_color(0xE4, 0xE4, 0xE4)));
    fl_yxline(x + 1, y + 2, y + h - 3);
    fl_yxline(x + w - 2, y + 2, y + h - 3);
    // top corners
    fl_color(activated_color(fl_rgb_color(0xB8, 0xB8, 0xB8)));
    fl_xyline(x, y + 1, x + 1, y);
    fl_yxline(x + w - 2, y, y + 1, x + w - 1);
    // bottom corners
    fl_color(activated_color(fl_rgb_color(0xA0, 0xA0, 0xA0)));
    fl_xyline(x, y + h - 2, x + 1, y + h - 1);
    fl_yxline(x + w - 2, y + h - 1, y + h - 2, x + w - 1);
}

void greybird_button_up_box(int x, int y, int w, int h, Fl_Color c) {
    if (w >= h) {
        vertical_gradient(x + 2, y + 2, x + w - 3, y + h - 2, activated_color(fl_rgb_color(0xDB, 0xDB, 0xDB)), activated_color(fl_rgb_color(0xCC, 0xCC, 0xCC)));
    } else {
        horizontal_gradient(x + 2, y + 2, x + w - 3, y + h - 2, activated_color(fl_rgb_color(0xDB, 0xDB, 0xDB)), activated_color(fl_rgb_color(0xCC, 0xCC, 0xCC)));
    }
    greybird_button_up_frame(x, y, w, h, c);
}

void greybird_check_down_frame(int x, int y, int w, int h, Fl_Color) {
    // top border
    fl_color(activated_color(fl_rgb_color(0x80, 0x80, 0x80)));
    fl_xyline(x + 2, y, x + w - 3);
    // side borders
    fl_color(activated_color(fl_rgb_color(0x89, 0x89, 0x89)));
    fl_yxline(x, y + 2, y + h - 3);
    fl_yxline(x + w - 1, y + 2, y + h - 3);
    // bottom border
    fl_color(activated_color(fl_rgb_color(0x90, 0x90, 0x90)));
    fl_xyline(x + 2, y + h - 1, x + w - 3);
    // top corners
    fl_color(activated_color(fl_rgb_color(0xA6, 0xA6, 0xA6)));
    fl_xyline(x, y + 1, x + 1, y);
    fl_yxline(x + w - 2, y, y + 1, x + w - 1);
    // bottom corners
    fl_color(activated_color(fl_rgb_color(0xB0, 0xB0, 0xB0)));
    fl_xyline(x, y + h - 2, x + 1, y + h - 1);
    fl_yxline(x + w - 2, y + h - 1, y + h - 2, x + w - 1);
}

void greybird_panel_thin_up_frame(int x, int y, int w, int h, Fl_Color) {
    // top and left borders
    fl_color(activated_color(fl_rgb_color(0xDA, 0xDA, 0xDA)));
    fl_yxline(x, y + h - 2, y, x + w - 2);
    // bottom and right borders
    fl_color(activated_color(fl_rgb_color(0xC1, 0xC1, 0xC1)));
    fl_xyline(x, y + h - 1, x + w - 1, y);
}

void greybird_check_down_box(int x, int y, int w, int h, Fl_Color c) {
    fl_color(activated_color(c));
    fl_rectf(x + 1, y + 1, w - 2, h - 2);
    greybird_check_down_frame(x, y, w, h, c);
}

void greybird_panel_thin_up_box(int x, int y, int w, int h, Fl_Color c) {
    fl_color(activated_color(c));
    fl_rectf(x + 1, y + 1, w - 2, h - 2);
    greybird_panel_thin_up_frame(x, y, w, h, c);
}

void greybird_spacer_thin_down_frame(int x, int y, int w, int h, Fl_Color) {
    // top and left borders
    fl_color(activated_color(fl_rgb_color(0xBA, 0xBA, 0xBA)));
    fl_yxline(x, y + h - 2, y, x + w - 2);
    // bottom and right borders
    fl_color(activated_color(fl_rgb_color(0xDA, 0xDA, 0xDA)));
    fl_xyline(x, y + h - 1, x + w - 1, y);
}

void greybird_spacer_thin_down_box(int x, int y, int w, int h, Fl_Color c) {
    fl_color(activated_color(c));
    fl_rectf(x + 1, y + 1, w - 2, h - 2);
    greybird_spacer_thin_down_frame(x, y, w, h, c);
}

void greybird_radio_round_down_frame(int x, int y, int w, int h, Fl_Color) {
    fl_color(activated_color(fl_rgb_color(0x80, 0x80, 0x80)));
    fl_arc(x, y, w, h, 0.0, 360.0);
}

void greybird_radio_round_down_box(int x, int y, int w, int h, Fl_Color c) {
    fl_color(c);
    fl_pie(x + 1, y + 1, w - 2, h - 2, 0.0, 360.0);
    greybird_radio_round_down_frame(x, y, w, h, c);
}

void greybird_hovered_up_frame(int x, int y, int w, int h, Fl_Color) {
    // top outer border
    fl_color(activated_color(fl_rgb_color(0xAE, 0xAE, 0xAE)));
    fl_xyline(x + 2, y, x + w - 3);
    // side outer borders
    fl_color(activated_color(fl_rgb_color(0x9E, 0x9E, 0x9E)));
    fl_yxline(x, y + 2, y + h - 3);
    fl_yxline(x + w - 1, y + 2, y + h - 3);
    // bottom outer border
    fl_color(activated_color(fl_rgb_color(0x8E, 0x8E, 0x8E)));
    fl_xyline(x + 2, y + h - 1, x + w - 3);
    // top inner border
    fl_color(activated_color(fl_rgb_color(0xF3, 0xF3, 0xF3)));
    fl_xyline(x + 2, y + 1, x + w - 3);
    // side inner borders
    fl_color(activated_color(fl_rgb_color(0xED, 0xED, 0xED)));
    fl_yxline(x + 1, y + 2, y + h - 3);
    fl_yxline(x + w - 2, y + 2, y + h - 3);
    // top corners
    fl_color(activated_color(fl_rgb_color(0xC0, 0xC0, 0xC0)));
    fl_xyline(x, y + 1, x + 1, y);
    fl_yxline(x + w - 2, y, y + 1, x + w - 1);
    // bottom corners
    fl_color(activated_color(fl_rgb_color(0xA7, 0xA7, 0xA7)));
    fl_xyline(x, y + h - 2, x + 1, y + h - 1);
    fl_yxline(x + w - 2, y + h - 1, y + h - 2, x + w - 1);
}

void greybird_hovered_up_box(int x, int y, int w, int h, Fl_Color c) {
    vertical_gradient(x + 2, y + 2, x + w - 3, y + h - 2, activated_color(fl_rgb_color(0xE6, 0xE6, 0xE6)), activated_color(fl_rgb_color(0xD6, 0xD6, 0xD6)));
    greybird_hovered_up_frame(x, y, w, h, c);
}

void greybird_depressed_down_frame(int x, int y, int w, int h, Fl_Color) {
    // top outer border
    fl_color(activated_color(fl_rgb_color(0x8A, 0x8A, 0x8A)));
    fl_xyline(x + 2, y, x + w - 3);
    // side outer borders
    fl_color(activated_color(fl_rgb_color(0x7D, 0x7D, 0x7D)));
    fl_yxline(x, y + 2, y + h - 3);
    fl_yxline(x + w - 1, y + 2, y + h - 3);
    // bottom outer border
    fl_color(activated_color(fl_rgb_color(0x71, 0x71, 0x71)));
    fl_xyline(x + 2, y + h - 1, x + w - 3);
    // top corners
    fl_color(activated_color(fl_rgb_color(0x98, 0x98, 0x98)));
    fl_xyline(x, y + 1, x + 1, y);
    fl_yxline(x + w - 2, y, y + 1, x + w - 1);
    // bottom corners
    fl_color(activated_color(fl_rgb_color(0x88, 0x88, 0x88)));
    fl_xyline(x, y + h - 2, x + 1, y + h - 1);
    fl_yxline(x + w - 2, y + h - 1, y + h - 2, x + w - 1);
}

void greybird_depressed_down_box(int x, int y, int w, int h, Fl_Color c) {
    // top gradient
    vertical_gradient(x + 1, y + 1, x + w - 2, y + 4, activated_color(fl_rgb_color(0xAF, 0xAF, 0xAF)), activated_color(fl_rgb_color(0xB4, 0xB4, 0xB4)));
    vertical_gradient(x + 1, y + 5, x + w - 2, y + h - 1, activated_color(fl_rgb_color(0xB4, 0xB4, 0xB4)), activated_color(fl_rgb_color(0xAA, 0xAA, 0xAA)));
    greybird_depressed_down_frame(x, y, w, h, c);
}

void greybird_input_thin_down_frame(int x, int y, int w, int h, Fl_Color) {
    // top outer border
    fl_color(activated_color(fl_rgb_color(0x84, 0x84, 0x84)));
    fl_xyline(x + 2, y, x + w - 3);
    // side outer borders
    fl_color(activated_color(fl_rgb_color(0x97, 0x97, 0x97)));
    fl_yxline(x, y + 2, y + h - 3);
    fl_yxline(x + w - 1, y + 2, y + h - 3);
    // bottom outer border
    fl_color(activated_color(fl_rgb_color(0xAA, 0xAA, 0xAA)));
    fl_xyline(x + 2, y + h - 1, x + w - 3);
    // inner border
    fl_color(activated_color(fl_rgb_color(0xEC, 0xEC, 0xEC)));
    fl_xyline(x + 2, y + 1, x + w - 3);
    fl_yxline(x + 1, y + 2, y + h - 3);
    fl_yxline(x + w - 2, y + 2, y + h - 3);
    // top corners
    fl_color(activated_color(fl_rgb_color(0xA4, 0xA4, 0xA4)));
    fl_xyline(x, y + 1, x + 1, y);
    fl_yxline(x + w - 2, y, y + 1, x + w - 1);
    // bottom corners
    fl_color(activated_color(fl_rgb_color(0xBE, 0xBE, 0xBE)));
    fl_xyline(x, y + h - 2, x + 1, y + h - 1);
    fl_yxline(x + w - 2, y + h - 1, y + h - 2, x + w - 1);
}

void greybird_input_thin_down_box(int x, int y, int w, int h, Fl_Color c) {
    fl_color(activated_color(c));
    fl_rectf(x + 2, y + 2, w - 4, h - 3);
    greybird_input_thin_down_frame(x, y, w, h, c);
}

void greybird_default_button_up_frame(int x, int y, int w, int h, Fl_Color) {
    // top outer border
    fl_color(activated_color(fl_rgb_color(0x69, 0x82, 0x9D)));
    fl_xyline(x + 2, y, x + w - 3);
    // side outer borders
    fl_color(activated_color(fl_rgb_color(0x61, 0x77, 0x8E)));
    fl_yxline(x, y + 2, y + h - 3);
    fl_yxline(x + w - 1, y + 2, y + h - 3);
    // bottom outer border
    fl_color(activated_color(fl_rgb_color(0x59, 0x6B, 0x7D)));
    fl_xyline(x + 2, y + h - 1, x + w - 3);
    // top inner border
    fl_color(activated_color(fl_rgb_color(0x88, 0xB7, 0xE9)));
    fl_xyline(x + 2, y + 1, x + w - 3);
    // side inner borders
    fl_color(activated_color(fl_rgb_color(0x79, 0xAC, 0xE1)));
    fl_yxline(x + 1, y + 2, y + h - 3);
    fl_yxline(x + w - 2, y + 2, y + h - 3);
    // top corners
    fl_color(activated_color(fl_rgb_color(0x76, 0x99, 0xBE)));
    fl_xyline(x, y + 1, x + 1, y);
    fl_yxline(x + w - 2, y, y + 1, x + w - 1);
    // bottom corners
    fl_color(activated_color(fl_rgb_color(0x5D, 0x81, 0xA6)));
    fl_xyline(x, y + h - 2, x + 1, y + h - 1);
    fl_yxline(x + w - 2, y + h - 1, y + h - 2, x + w - 1);
}

void greybird_default_button_up_box(int x, int y, int w, int h, Fl_Color c) {
    vertical_gradient(x + 2, y + 2, x + w - 3, y + h - 2, activated_color(fl_rgb_color(0x72, 0xA7, 0xDF)), activated_color(fl_rgb_color(0x63, 0x9C, 0xD7)));
    greybird_default_button_up_frame(x, y, w, h, c);
}

void greybird_default_depressed_down_frame(int x, int y, int w, int h, Fl_Color) {
    // top outer border
    fl_color(activated_color(fl_rgb_color(0x58, 0x71, 0x8C)));
    fl_xyline(x + 2, y, x + w - 3);
    // side outer borders
    fl_color(activated_color(fl_rgb_color(0x50, 0x66, 0x7D)));
    fl_yxline(x, y + 2, y + h - 3);
    fl_yxline(x + w - 1, y + 2, y + h - 3);
    // bottom outer border
    fl_color(activated_color(fl_rgb_color(0x48, 0x5A, 0x6C)));
    fl_xyline(x + 2, y + h - 1, x + w - 3);
    // top corners
    fl_color(activated_color(fl_rgb_color(0x65, 0x88, 0xAD)));
    fl_xyline(x, y + 1, x + 1, y);
    fl_yxline(x + w - 2, y, y + 1, x + w - 1);
    // bottom corners
    fl_color(activated_color(fl_rgb_color(0x4C, 0x70, 0x95)));
    fl_xyline(x, y + h - 2, x + 1, y + h - 1);
    fl_yxline(x + w - 2, y + h - 1, y + h - 2, x + w - 1);
}

void greybird_default_depressed_down_box(int x, int y, int w, int h, Fl_Color c) {
    // top gradient
    vertical_gradient(x + 1, y + 1, x + w - 2, y + 4, activated_color(fl_rgb_color(0x53, 0x83, 0xB2)), activated_color(fl_rgb_color(0x5C, 0x92, 0xC7)));
    vertical_gradient(x + 1, y + 5, x + w - 2, y + h - 1, activated_color(fl_rgb_color(0x5C, 0x92, 0xC7)), activated_color(fl_rgb_color(0x4D, 0x7B, 0xA5)));
    greybird_default_depressed_down_frame(x, y, w, h, c);
}

void greybird_tabs_frame(int x, int y, int w, int h, Fl_Color) {
    // top outer border
    fl_color(activated_color(fl_rgb_color(0xA6, 0xA6, 0xA6)));
    fl_xyline(x + 2, y, x + w - 3);
    // side outer borders
    fl_color(activated_color(fl_rgb_color(0x96, 0x96, 0x96)));
    fl_yxline(x, y + 2, y + h - 3);
    fl_yxline(x + w - 1, y + 2, y + h - 3);
    // bottom outer border
    fl_color(activated_color(fl_rgb_color(0x87, 0x87, 0x87)));
    fl_xyline(x + 2, y + h - 1, x + w - 3);
    // top inner border
    fl_color(activated_color(fl_rgb_color(0xEE, 0xEE, 0xEE)));
    fl_xyline(x + 2, y + 1, x + w - 3);
    // side inner borders
    fl_color(activated_color(fl_rgb_color(0xE4, 0xE4, 0xE4)));
    fl_yxline(x + 1, y + 2, y + h - 3);
    fl_yxline(x + w - 2, y + 2, y + h - 3);
    // top corners
    fl_color(activated_color(fl_rgb_color(0xB8, 0xB8, 0xB8)));
    fl_xyline(x, y + 1, x + 1, y);
    fl_yxline(x + w - 2, y, y + 1, x + w - 1);
    // bottom corners
    fl_color(activated_color(fl_rgb_color(0xA0, 0xA0, 0xA0)));
    fl_xyline(x, y + h - 2, x + 1, y + h - 1);
    fl_yxline(x + w - 2, y + h - 1, y + h - 2, x + w - 1);
}

void greybird_tabs_box(int x, int y, int w, int h, Fl_Color c) {
    fl_color(activated_color(fl_rgb_color(0xD9, 0xD9, 0xD9)));
    fl_rectf(x + 2, y + 2, w - 3, h - 2);
    greybird_tabs_frame(x, y, w, h, c);
}

void use_greybird_scheme() {
    Fl::scheme("gtk+");
    Fl::set_boxtype(OS_BUTTON_UP_BOX, greybird_button_up_box, 2, 2, 4, 4);
    Fl::set_boxtype(OS_CHECK_DOWN_BOX, greybird_check_down_box, 1, 1, 2, 2);
    Fl::set_boxtype(OS_BUTTON_UP_FRAME, greybird_button_up_frame, 2, 2, 4, 4);
    Fl::set_boxtype(OS_CHECK_DOWN_FRAME, greybird_check_down_frame, 1, 1, 2, 2);
    Fl::set_boxtype(OS_PANEL_THIN_UP_BOX, greybird_panel_thin_up_box, 1, 1, 2, 2);
    Fl::set_boxtype(OS_SPACER_THIN_DOWN_BOX, greybird_spacer_thin_down_box, 1, 1, 2, 2);
    Fl::set_boxtype(OS_PANEL_THIN_UP_FRAME, greybird_panel_thin_up_frame, 1, 1, 2, 2);
    Fl::set_boxtype(OS_SPACER_THIN_DOWN_FRAME, greybird_spacer_thin_down_frame, 1, 1, 2, 2);
    Fl::set_boxtype(OS_RADIO_ROUND_DOWN_BOX, greybird_radio_round_down_box, 3, 3, 6, 6);
    Fl::set_boxtype(OS_HOVERED_UP_BOX, greybird_hovered_up_box, 2, 2, 4, 4);
    Fl::set_boxtype(OS_DEPRESSED_DOWN_BOX, greybird_depressed_down_box, 2, 2, 4, 4);
    Fl::set_boxtype(OS_HOVERED_UP_FRAME, greybird_hovered_up_frame, 2, 2, 4, 4);
    Fl::set_boxtype(OS_DEPRESSED_DOWN_FRAME, greybird_depressed_down_frame, 2, 2, 4, 4);
    Fl::set_boxtype(OS_INPUT_THIN_DOWN_BOX, greybird_input_thin_down_box, 2, 3, 4, 6);
    Fl::set_boxtype(OS_INPUT_THIN_DOWN_FRAME, greybird_input_thin_down_frame, 2, 3, 4, 6);
    Fl::set_boxtype(OS_DEFAULT_BUTTON_UP_BOX, greybird_default_button_up_box, 2, 2, 4, 4);
    Fl::set_boxtype(OS_DEFAULT_HOVERED_UP_BOX, OS_HOVERED_UP_BOX);
    Fl::set_boxtype(OS_DEFAULT_DEPRESSED_DOWN_BOX, greybird_default_depressed_down_box, 2, 2, 4, 4);
    Fl::set_boxtype(OS_TOOLBAR_BUTTON_HOVER_BOX, OS_BUTTON_UP_BOX);
    Fl::set_boxtype(OS_TABS_BOX, greybird_tabs_box, 2, 2, 4, 4);
    Fl::set_boxtype(OS_SWATCH_BOX, OS_SPACER_THIN_DOWN_BOX);
    Fl::set_boxtype(OS_MINI_BUTTON_UP_BOX, OS_BUTTON_UP_BOX);
    Fl::set_boxtype(OS_MINI_DEPRESSED_DOWN_BOX, OS_DEPRESSED_DOWN_BOX);
    Fl::set_boxtype(OS_MINI_BUTTON_UP_FRAME, OS_BUTTON_UP_FRAME);
    Fl::set_boxtype(OS_MINI_DEPRESSED_DOWN_FRAME, OS_DEPRESSED_DOWN_FRAME);
    Fl::set_boxtype(FL_UP_BOX, OS_BUTTON_UP_BOX);
    Fl::set_boxtype(FL_DOWN_BOX, OS_CHECK_DOWN_BOX);
    Fl::set_boxtype(FL_ROUND_DOWN_BOX, OS_RADIO_ROUND_DOWN_BOX);
    Fl::set_boxtype(OS_BG_BOX, FL_FLAT_BOX);
    Fl::set_boxtype(OS_BG_DOWN_BOX, OS_BG_BOX);
    Fl::set_boxtype(OS_TOOLBAR_FRAME, OS_PANEL_THIN_UP_FRAME);
}

void use_greybird_colors() {
    Fl::background(0xCE, 0xCE, 0xCE);
    Fl::background2(0xFC, 0xFC, 0xFC);
    Fl::foreground(0x24, 0x24, 0x24);
    Fl::set_color(FL_INACTIVE_COLOR, 0x55, 0x55, 0x55);
    Fl::set_color(FL_SELECTION_COLOR, 0x33, 0x67, 0xD1);
    Fl::set_color(OS_TAB_COLOR, 0xD9, 0xD9, 0xD9);
    Fl_Tooltip::color(fl_rgb_color(0x0A, 0x0A, 0x0A));
    Fl_Tooltip::textcolor(fl_rgb_color(0xFF, 0xFF, 0xFF));
}

void use_native_settings() {
    Fl::visible_focus(0);
    Fl::scrollbar_size(15);
    Fl_Tooltip::font(OS_FONT);
    Fl_Tooltip::size(OS_FONT_SIZE);
    Fl_Tooltip::delay(0.5f);
}
