#include "config.hpp"
#include <functional>

ConfigWindow::ConfigWindow() {
    window = gtk_window_new();
    window.connect_signal("destroy", [this](GtkWidget*) {
        window.release();
    });
    gtk_window_set_title(GTK_WINDOW(window.get()), "Lux Desktop");
    gtk_window_set_default_size(GTK_WINDOW(window.get()), 450, 0);

    glib::Object<GtkWidget> vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    g_object_set(vbox.get(), "margin-top", 15, "margin-bottom", 15, "margin-start", 15, "margin-end", 15, nullptr);
    gtk_widget_set_valign(vbox.get(), GTK_ALIGN_CENTER);

    glib::Object<GtkWidget> title_label = gtk_label_new("<span size=\"25pt\" weight=\"ultrabold\">Lux Client</span>");
    gtk_label_set_use_markup(GTK_LABEL(title_label.get()), TRUE);
    gtk_widget_set_halign(title_label.get(), GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(vbox.get()), title_label.release());

    address_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(address_entry), "IP Address");
    glib::connect_signal(address_entry, "activate", std::bind(&ConfigWindow::complete, this, std::placeholders::_1));
    gtk_box_append(GTK_BOX(vbox.get()), address_entry);

    password_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(password_entry), "Password");
    gtk_entry_set_visibility(GTK_ENTRY(password_entry), FALSE);
    glib::connect_signal(password_entry, "activate", std::bind(&ConfigWindow::complete, this, std::placeholders::_1));
    gtk_box_append(GTK_BOX(vbox.get()), password_entry);

    {
        glib::Object<GtkWidget> hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        glib::Object<GtkWidget> title_label = gtk_label_new("Bitrate (Kbps) ");
        bitrate_spin_button = gtk_spin_button_new_with_range(250, 10000, 1);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(bitrate_spin_button), 4000);
        glib::connect_signal(bitrate_spin_button, "activate", std::bind(&ConfigWindow::complete, this, std::placeholders::_1));
        gtk_box_append(GTK_BOX(hbox.get()), title_label.release());
        gtk_box_append(GTK_BOX(hbox.get()), bitrate_spin_button);
        gtk_box_append(GTK_BOX(vbox.get()), hbox.release());
    }

    client_side_mouse_check_button = gtk_check_button_new_with_label("Client-side mouse");
    gtk_box_append(GTK_BOX(vbox.get()), client_side_mouse_check_button);

    glib::Object<GtkWidget> login_button = gtk_button_new_with_label("Login");
    login_button.connect_signal("clicked", std::bind(&ConfigWindow::complete, this, std::placeholders::_1));
    gtk_box_append(GTK_BOX(vbox.get()), login_button.release());

    gtk_window_set_child(GTK_WINDOW(window.get()), vbox.release());
    gtk_window_present(GTK_WINDOW(window.get()));
}
