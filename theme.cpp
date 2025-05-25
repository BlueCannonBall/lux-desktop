#include "theme.hpp"
#include "glib.hpp"
#include <gio/gio.h>
#include <string.h>

bool is_dark_mode() {
    glib::Object<GSettings> settings = g_settings_new("org.gnome.desktop.interface");
    char* theme = g_settings_get_string(settings.get(), "color-scheme");
    bool ret = strcmp(theme, "default") && strcmp(theme, "prefer-light");
    g_free(theme);
    return ret;
}

std::string get_gtk_theme() {
    glib::Object<GSettings> settings = g_settings_new("org.gnome.desktop.interface");
    char* theme = g_settings_get_string(settings.get(), "gtk-theme");
    std::string ret = theme;
    g_free(theme);
    return ret;
}
