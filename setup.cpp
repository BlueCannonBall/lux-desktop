#include "setup.hpp"
#include <FL/Fl_Box.H>

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

SetupWindow::SetupWindow():
    Fl_Double_Window(400, 190, "Lux Desktop") {
    xclass("lux-desktop");
    size_range(300, 190, 0, 400);
    auto column = new Fl_Flex(10, 10, w() - 20, h() - 20, Fl_Flex::COLUMN);

    {
        auto row = new Fl_Flex(Fl_Flex::ROW);
        auto label = new Label(0, 0, "IP Address:");
        address_input = new Fl_Input(0, 0, 0, 0, "");
        row->setSize(label, label->w());
        row->end();
    }

    {
        auto row = new Fl_Flex(Fl_Flex::ROW);
        auto label = new Label(0, 0, "Password:");
        password_input = new Fl_Secret_Input(0, 0, 0, 0, "");
        row->setSize(label, label->w());
        row->end();
    }

    {
        auto row = new Fl_Flex(Fl_Flex::ROW);
        auto label = new Label(0, 0, "Bitrate:");
        bitrate_spinner = new Fl_Spinner(0, 0, 0, 0, "");
        bitrate_spinner->type(FL_INT_INPUT);
        bitrate_spinner->minimum(250);
        bitrate_spinner->maximum(10000);
        bitrate_spinner->value(4000);
        row->setSize(label, label->w());
        row->setSize(bitrate_spinner, 80);
        row->end();
    }

    client_side_mouse_check_button = new Fl_Check_Button(0, 0, 0, 0, "Client-side mouse");

    login_button = new Fl_Button(0, 0, 0, 0, "Login@>");
    login_button->callback([](Fl_Widget* widget, void* data) {
        auto setup_window = (SetupWindow*) data;
        setup_window->complete();
    },
        this);

    resizable(column);
    column->end();
    end();
}

void SetupWindow::complete() {
    address = address_input->value();
    password = password_input->value();
    bitrate = bitrate_spinner->value();
    client_side_mouse = client_side_mouse_check_button->value();
    setup_complete = true;
    hide();
}
