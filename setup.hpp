#pragma once

#include "FL_Flex/FL_Flex.H"
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Secret_Input.H>
#include <FL/Fl_Spinner.H>
#include <string>

class SetupWindow : public Fl_Window {
public:
    std::string address;
    std::string password;
    unsigned int bitrate;
    bool client_side_mouse;
    bool config_complete = false;

    SetupWindow();
    SetupWindow(const SetupWindow&) = delete;
    SetupWindow(SetupWindow&&) = delete;

    bool run() {
        show();
        while (shown()) {
            Fl::wait();
        }
        return config_complete;
    }

protected:
    Fl_Input* address_input;
    Fl_Secret_Input* password_input;
    Fl_Spinner* bitrate_spinner;
    Fl_Check_Button* client_side_mouse_check_button;
    Fl_Button* login_button;

    void complete();
};