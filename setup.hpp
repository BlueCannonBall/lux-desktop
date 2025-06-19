#pragma once

#include "theme.hpp"
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Secret_Input.H>
#include <FL/Fl_Spinner.H>
#include <FL/x.H>
#include <filesystem>
#include <string>

class SetupWindow : public Fl_Double_Window {
public:
    std::string address;
    std::string password;
    unsigned int bitrate;
    bool client_side_mouse;
    bool view_only;
    bool verify_certs;
    bool setup_complete = false;

    SetupWindow();
    SetupWindow(const SetupWindow&) = delete;
    SetupWindow(SetupWindow&&) = delete;

    bool run() {
        show();
#ifdef _WIN32
        set_window_dark_mode(fl_xid(this));
#endif
        while (shown()) {
            Fl::wait();
        }
        Fl::check();
        return setup_complete;
    }

    static std::filesystem::path get_config_path();

protected:
    Fl_Input* address_input;
    Fl_Secret_Input* password_input;
    Fl_Spinner* bitrate_spinner;
    Fl_Check_Button* client_side_mouse_check_button;
    Fl_Check_Button* view_only_check_button;
    Fl_Check_Button* verify_certs_check_button;
    Fl_Button* login_button;

    void complete();
};
