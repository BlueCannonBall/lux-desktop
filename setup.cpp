#include "setup.hpp"
#include "json.hpp"
#include <FL/Fl_Box.H>
#include <FL/Fl_Flex.H>
#include <fstream>
#include <iostream>
#include <stdlib.h>

using nlohmann::json;

class Label : public Fl_Box {
public:
    Label(int x, int y, const char* text):
        Fl_Box(0, 0, 0, 0, text) {
        int label_w = 0;
        int label_h;
        measure_label(label_w, label_h);
        Fl_Box::resize(x, y, label_w, label_h);
    }
    Label(int x, int y, int w, int h, const char* text):
        Fl_Box(0, 0, 0, 0, text) {
        resize(x, y, w, h);
    }

    void resize(int x, int y, int w, int h) override {
        int label_w = 0;
        int label_h;
        measure_label(label_w, label_h);
        Fl_Box::resize(x, y, std::max(w, label_w), std::max(h, label_h));
    }
};

std::filesystem::path SetupWindow::get_config_path() {
    std::filesystem::path ret;
#ifdef _WIN32
    if (char* appdata = getenv("APPDATA")) {
        ret = std::filesystem::path(appdata) / "lux-desktop";
    }
#elif defined(__APPLE__)
    if (char* home = getenv("HOME")) {
        ret = std::filesystem::path(home) / "Library" / "Application Support" / "lux-desktop";
    }
#else
    if (char* xdg_config_home = getenv("XDG_CONFIG_HOME")) {
        ret = std::filesystem::path(xdg_config_home) / "lux-desktop";
    } else if (char* home = getenv("HOME")) {
        ret = std::filesystem::path(home) / ".config" / "lux-desktop";
    }
#endif
    return ret;
}

SetupWindow::SetupWindow():
    Fl_Double_Window(400, 270, "Lux Client") {
    xclass("lux-desktop");
    size_range(350, 250, 0, 400);
    auto column = new Fl_Flex(10, 10, w() - 20, h() - 20, Fl_Flex::COLUMN);
    column->gap(5);

    {
        auto row = new Fl_Flex(Fl_Flex::ROW);
        auto label = new Label(0, 0, "Address: ");
        address_input = new Fl_Input(0, 0, 0, 0, "");
        row->fixed(label, label->w());
        row->end();
    }

    {
        auto row = new Fl_Flex(Fl_Flex::ROW);
        auto label = new Label(0, 0, "Password: ");
        password_input = new Fl_Secret_Input(0, 0, 0, 0, "");
        row->fixed(label, label->w());
        row->end();
    }

    {
        auto row = new Fl_Flex(Fl_Flex::ROW);
        auto label = new Label(0, 0, "Bitrate: ");
        bitrate_spinner = new Fl_Spinner(0, 0, 0, 0, "");
        bitrate_spinner->type(FL_INT_INPUT);
        bitrate_spinner->minimum(500);
        bitrate_spinner->maximum(10000);
        bitrate_spinner->value(4000);
        row->fixed(label, label->w());
        row->fixed(bitrate_spinner, 80);
        row->end();
    }

    client_side_mouse_check_button = new Fl_Check_Button(0, 0, 0, 0, "Client-side mouse");

    view_only_check_button = new Fl_Check_Button(0, 0, 0, 0, "View only");

    verify_certs_check_button = new Fl_Check_Button(0, 0, 0, 0, "Verify certificates");
    verify_certs_check_button->value(true);

    login_button = new Fl_Button(0, 0, 0, 0, "Login@>");
    login_button->callback([](Fl_Widget* widget, void* data) {
        auto setup_window = (SetupWindow*) data;
        setup_window->complete();
    },
        this);

    resizable(column);
    column->end();
    end();

    auto config_path = get_config_path();
    if (!config_path.empty()) {
        std::ifstream config_file(config_path / "config.json");
        if (config_file.is_open()) {
            json config_json = json::parse(config_file);
            if (auto address_it = config_json.find("address"); address_it != config_json.end() && address_it->is_string()) {
                address_input->value(address_it->get<std::string>().c_str());
            }
            if (auto password_it = config_json.find("password"); password_it != config_json.end() && password_it->is_string()) {
                password_input->value(password_it->get<std::string>().c_str());
            }
            if (auto bitrate_it = config_json.find("bitrate"); bitrate_it != config_json.end() && bitrate_it->is_number_unsigned()) {
                bitrate_spinner->value(bitrate_it->get<unsigned int>());
            }
            if (auto client_side_mouse_it = config_json.find("client_side_mouse"); client_side_mouse_it != config_json.end() && client_side_mouse_it->is_boolean()) {
                client_side_mouse_check_button->value(client_side_mouse_it->get<bool>());
            }
            if (auto view_only_it = config_json.find("view_only"); view_only_it != config_json.end() && view_only_it->is_boolean()) {
                view_only_check_button->value(view_only_it->get<bool>());
            }
            if (auto verify_certs_it = config_json.find("verify_certs"); verify_certs_it != config_json.end() && verify_certs_it->is_boolean()) {
                verify_certs_check_button->value(verify_certs_it->get<bool>());
            }
        } else {
            std::cerr << "Error: Could not open config file" << std::endl;
        }
    }
}

void SetupWindow::complete() {
    address = address_input->value();
    password = password_input->value();
    bitrate = bitrate_spinner->value();
    client_side_mouse = client_side_mouse_check_button->value();
    view_only = view_only_check_button->value();
    verify_certs = verify_certs_check_button->value();
    setup_complete = true;
    hide();

    auto config_path = get_config_path();
    if (!config_path.empty()) {
        if (!std::filesystem::exists(config_path)) {
            std::filesystem::create_directory(config_path);
        }

        std::ofstream config_file(config_path / "config.json");
        if (config_file.is_open()) {
            json config_json = {
                {"address", address},
                {"password", password},
                {"bitrate", bitrate},
                {"client_side_mouse", client_side_mouse},
                {"view_only", view_only},
                {"verify_certs", verify_certs},
            };
            config_file << config_json;
        } else {
            std::cerr << "Error: Could not open config file" << std::endl;
        }
    }
}
