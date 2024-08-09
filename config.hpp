#pragma once

#include "glib.hpp"
#include <gtk/gtk.h>
#include <iostream>
#include <string>

inline void error(const std::string& primary_text, const std::string& secondary_text) {
    static constexpr const char* buttons[] = {"Close", nullptr};

    std::cerr << primary_text << ": " << secondary_text << std::endl;
    glib::Object<GtkAlertDialog> dialog = gtk_alert_dialog_new("%s", primary_text.c_str());
    gtk_alert_dialog_set_detail(dialog.get(), secondary_text.c_str());
    gtk_alert_dialog_set_buttons(dialog.get(), buttons);
    gtk_alert_dialog_show(dialog.get(), nullptr);
}

class ConfigWindow {
public:
    std::string address;
    std::string password;
    unsigned int bitrate;
    bool client_side_mouse;
    bool config_complete = false;

    ConfigWindow();
    ConfigWindow(const ConfigWindow&) = delete;
    ConfigWindow(ConfigWindow&&) = delete;

    bool run() {
        while (g_list_model_get_n_items(gtk_window_get_toplevels())) {
            g_main_context_iteration(nullptr, TRUE);
        }
        return config_complete;
    }

protected:
    glib::Object<GtkWidget> window;
    GtkWidget* address_entry;
    GtkWidget* password_entry;
    GtkWidget* bitrate_spin_button;
    GtkWidget* client_side_mouse_check_button;

    void complete(void*) {
        address = gtk_editable_get_text(GTK_EDITABLE(address_entry));
        password = gtk_editable_get_text(GTK_EDITABLE(password_entry));
        bitrate = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(bitrate_spin_button));
        client_side_mouse = gtk_check_button_get_active(GTK_CHECK_BUTTON(client_side_mouse_check_button));
        config_complete = true;
        gtk_window_destroy(GTK_WINDOW(window.get()));
    }
};