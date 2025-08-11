#pragma once

#include "connection.hpp"
#include "video.hpp"
#include <FL/Fl.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Flex.H>
#include <FL/Fl_Hold_Browser.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_PNG_Image.H>
#include <FL/Fl_Secret_Input.H>
#include <FL/Fl_Spinner.H>
#include <FL/Fl_Tile.H>
#include <memory>
#include <string>
#include <vector>

inline bool is_key_global_shortcut(int key) {
    switch (key) {
    case FL_F + 5:
    case FL_F + 9:
    case FL_F + 11:
        return true;

    default:
        return false;
    }
}

class Stage : public Fl_Group {
protected:
    Fl_Widget* centered = nullptr;
    bool fill = false;

public:
    Stage(int x, int y, int width, int height, const char* label = nullptr):
        Fl_Group(x, y, width, height, label) {
        align(FL_ALIGN_INSIDE | FL_ALIGN_CENTER);
        labelsize(18);
    }

    Fl_Widget* get_centered() const {
        return centered;
    }

    bool get_fill() const {
        return fill;
    }

    void set_centered(Fl_Widget* widget);
    void set_fill(bool fill);
    void resize(int x, int y, int width, int height) override;
    void draw() override;
};

class ConnectionEditor : public Fl_Flex {
protected:
    Fl_Input* name_input;
    Fl_Input* address_input;
    Fl_Secret_Input* password_input;
    Fl_Spinner* bitrate_spinner;
    Fl_Check_Button* client_side_mouse_check_button;
    Fl_Check_Button* view_only_check_button;
    Fl_Check_Button* verify_certs_check_button;

public:
    ConnectionEditor(int x, int y, int width, int height, const std::string& name = {}, const ConnectionInfo& connection = {}, bool show_connect_button = false);

    std::string name() const {
        return name_input->value();
    }

    ConnectionInfo to_conn_info() const {
        return {
            address_input->value(),
            password_input->value(),
            (unsigned int) bitrate_spinner->value(),
            (bool) client_side_mouse_check_button->value(),
            (bool) view_only_check_button->value(),
            (bool) verify_certs_check_button->value(),
        };
    }
};

class MainWindow : public Fl_Double_Window {
public:
    MainWindow();

    void show() override;
    int handle(int event) override;

protected:
    Fl_PNG_Image window_icon;
    Fl_Menu_Bar* menu_bar;
    Fl_Tile* tile;
    Fl_Hold_Browser* conn_list;
    Stage* stage;
    ConnectionEditor* conn_editor = nullptr;
    VideoWindow* video_window = nullptr;

    std::vector<std::unique_ptr<ConnectionInfo>> connections;

    void refresh();
    void handle_select_conn();
    void handle_new_conn();
    void handle_toggle_fullscreen();
};
