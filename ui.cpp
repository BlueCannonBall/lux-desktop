#include "ui.hpp"
#include "icons/icon.h"
#include "json.hpp"
#include <FL/Fl_Box.H>
#include <FL/Fl_Return_Button.H>
#include <FL/Fl_SVG_Image.H>
#include <FL/fl_callback_macros.H>
#include <FL/fl_message.H>
#include <filesystem>
#include <fstream>
#include <stdlib.h>

using nlohmann::json;

std::filesystem::path get_config_path() {
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

// A label widget designed for use with Fl_Flex
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

void Stage::set_centered(Fl_Widget* widget) {
    centered = widget;
    if (centered) {
        centered->position(x() + w() / 2 - centered->w() / 2, y() + h() / 2 - centered->h() / 2);
    }
    redraw();
}

void Stage::set_fill(bool value) {
    if ((fill = value) && centered) {
        centered->resize(x(), y(), w(), h());
    }
    redraw();
}

void Stage::resize(int x, int y, int width, int height) {
    if (centered && centered->parent() == this) {
        if (fill) {
            centered->resize(x, y, width, height);
        } else {
            centered->position(x + width / 2 - centered->w() / 2, y + height / 2 - centered->h() / 2);
        }
    }
    Fl_Widget::resize(x, y, width, height);
}

void Stage::draw() {
    if (damage() & ~FL_DAMAGE_CHILD) {
        draw_box();
        if (!centered) draw_label();
    }
    draw_children();
}

ConnectionEditor::ConnectionEditor(int x, int y, int width, int height, const std::string& name, const ConnectionInfo& conn_info, bool show_connect_button):
    Fl_Flex(x, y, width, height, Fl_Flex::COLUMN) {
    gap(5);

    {
        auto row = new Fl_Flex(Fl_Flex::ROW);
        auto label = new Label(0, 0, "Name: ");
        name_input = new Fl_Input(0, 0, 0, 0);
        name_input->value(name.c_str());
        row->fixed(label, label->w());
        row->end();
    }

    {
        auto row = new Fl_Flex(Fl_Flex::ROW);
        auto label = new Label(0, 0, "Address: ");
        address_input = new Fl_Input(0, 0, 0, 0);
        address_input->value(conn_info.address.c_str());
        row->fixed(label, label->w());
        row->end();
    }

    {
        auto row = new Fl_Flex(Fl_Flex::ROW);
        auto label = new Label(0, 0, "Password: ");
        password_input = new Fl_Secret_Input(0, 0, 0, 0);
        password_input->value(conn_info.password.c_str());
        row->fixed(label, label->w());
        row->end();
    }

    {
        auto row = new Fl_Flex(Fl_Flex::ROW);
        auto label = new Label(0, 0, "Bitrate: ");
        bitrate_spinner = new Fl_Spinner(0, 0, 0, 0);
        bitrate_spinner->type(FL_INT_INPUT);
        bitrate_spinner->minimum(500);
        bitrate_spinner->maximum(10000);
        bitrate_spinner->value(conn_info.bitrate);
        row->fixed(label, label->w());
        row->fixed(bitrate_spinner, 80);
        row->end();
    }

    client_side_mouse_check_button = new Fl_Check_Button(0, 0, 0, 0, "Client-side mouse");
    client_side_mouse_check_button->value(conn_info.client_side_mouse);

    view_only_check_button = new Fl_Check_Button(0, 0, 0, 0, "View only");
    view_only_check_button->value(conn_info.view_only);

    verify_certs_check_button = new Fl_Check_Button(0, 0, 0, 0, "Verify certificates");
    verify_certs_check_button->value(conn_info.verify_certs);

    end();
}

MainWindow::MainWindow():
    Fl_Double_Window(1000, 650, "Lux Client"),
    window_icon(nullptr, icons_icon_png, icons_icon_png_len) {
    xclass("lux-desktop");
    icon(&window_icon);
    size_range(500, 400);

    auto column = new Fl_Flex(0, 0, w(), h(), Fl_Flex::COLUMN);

    menu_bar = new Fl_Menu_Bar(0, 0, w(), 30);
    menu_bar->add("File/New Connection", 0, [](Fl_Widget*, void* data) {
        auto window = (MainWindow*) data;
        window->handle_new_conn();
    },
        this);
    menu_bar->add("File/Quit", 0, [](Fl_Widget*, void*) {
        exit(0);
    });
    menu_bar->add("View/Refresh", 0, [](Fl_Widget*, void* data) {
        auto window = (MainWindow*) data;
        window->refresh();
    },
        this);
    menu_bar->add("Help/About", 0, [](Fl_Widget*, void*) {
        fl_message("lux-desktop 1.0.0\nCreated by BlueCannonBall\nGPLv3");
    });
    column->fixed(menu_bar, menu_bar->h());

    tile = new Fl_Tile(0, menu_bar->h(), w(), h() - menu_bar->h());

    // auto tux = new Fl_SVG_Image(nullptr, icons_tux_svg, icons_tux_svg_len);
    // auto apple = new Fl_SVG_Image(nullptr, icons_apple_svg, icons_apple_svg_len);
    // auto windows = new Fl_SVG_Image(nullptr, icons_windows_svg, icons_windows_svg_len);
    // tux->resize(30, 30);
    // apple->resize(30, 30);
    // windows->resize(30, 30);

    conn_list = new Fl_Hold_Browser(0, menu_bar->h(), 200, h() - menu_bar->h());
    tile->size_range(conn_list, 100, 200);

    stage = new Stage(200, menu_bar->h(), 800, h() - menu_bar->h(), "Select a connection to begin.");
    stage->box(FL_DOWN_BOX);
    stage->end();
    tile->size_range(stage, 370, 295);
    tile->resizable(stage);

    tile->end();
    column->end();
    resizable(column);
    end();

    FL_METHOD_CALLBACK_0(conn_list, MainWindow, this, handle_select_conn);

    refresh();
}

void MainWindow::refresh() {
    conn_list->deselect();
    handle_select_conn();
    conn_list->clear();
    connections.clear();
    if (auto config_path = get_config_path(); !config_path.empty()) {
        if (auto conn_path = config_path / "connections"; !std::filesystem::exists(conn_path)) {
            std::filesystem::create_directory(conn_path);
        } else {
            for (const auto& entry : std::filesystem::directory_iterator(conn_path)) {
                if (entry.path().extension() == ".json") {
                    if (std::ifstream file(entry.path()); file.is_open()) {
                        auto& conn_info = connections.emplace_back(std::make_unique<ConnectionInfo>(json::parse(file)));
                        conn_list->add(entry.path().stem().c_str(), conn_info.get());
                    }
                }
            }
        }
    }
}

void MainWindow::handle_select_conn() {
    stage->set_centered(nullptr);
    stage->set_fill(false);
    delete conn_editor;
    delete video_window;
    conn_editor = nullptr;
    video_window = nullptr;

    if (conn_list->value()) {
        stage->begin();

        conn_editor = new ConnectionEditor(0, 0, 350, 275, conn_list->text(conn_list->value()), *(ConnectionInfo*) conn_list->data(conn_list->value()));
        conn_editor->begin();

        auto row = new Fl_Flex(Fl_Flex::ROW);
        row->gap(5);

        new Fl_Box(0, 0, 0, 0); // Empty space

        auto delete_button = new Fl_Button(0, 0, 75, 0, "Delete");
        FL_INLINE_CALLBACK_2(delete_button, MainWindow*, window, this, int, index, conn_list->value(), {
            auto conn_info = window->conn_editor->to_conn_info();
            if (auto config_path = get_config_path(); !config_path.empty()) {
                auto conn_path = config_path / "connections";
                if (!std::filesystem::exists(conn_path)) {
                    std::filesystem::create_directory(conn_path);
                } else {
                    std::filesystem::remove(conn_path / (std::string(window->conn_list->text(index)) + ".json"));
                    window->refresh();
                }
            }
        });
        row->fixed(delete_button, delete_button->w());

        auto save_button = new Fl_Button(0, 0, 75, 0, "Save");
        FL_INLINE_CALLBACK_2(save_button, MainWindow*, window, this, int, index, conn_list->value(), {
            if (auto config_path = get_config_path(); !config_path.empty()) {
                auto conn_path = config_path / "connections";
                if (!std::filesystem::exists(conn_path)) {
                    std::filesystem::create_directory(conn_path);
                }

                if (std::ofstream file(conn_path / (window->conn_editor->name() + ".json")); file.is_open()) {
                    ConnectionInfo conn_info;
                    file << (conn_info = window->conn_editor->to_conn_info()).to_json();
                    file.close();

                    *((ConnectionInfo*) window->conn_list->data(index)) = std::move(conn_info);
                } else {
                    fl_message("Error: Failed to save file: Could not open file");
                    return;
                }

                if (window->conn_editor->name() != window->conn_list->text(index)) {
                    std::filesystem::remove((conn_path / window->conn_list->text(index)).replace_extension(".json"));
                    window->conn_list->text(index, window->conn_editor->name().c_str());
                }
            }
        });
        row->fixed(save_button, save_button->w());

        auto connect_button = new Fl_Button(0, 0, 75, 0, "Connect");
        FL_INLINE_CALLBACK_2(connect_button, MainWindow*, window, this, int, index, conn_list->value(), {
            window->video_window = new VideoWindow(0, 0, 400, 400, window->conn_editor->to_conn_info());
            if (!window->video_window->is_connected()) {
                delete window->video_window;
                window->video_window = nullptr;
                return;
            }

            window->stage->set_centered(nullptr);
            delete window->conn_editor;
            window->conn_editor = nullptr;

            window->stage->add(window->video_window);
            window->stage->set_centered(window->video_window);
            window->stage->set_fill(true);
            window->video_window->show();
            if (!window->video_window->is_playing()) {
                window->refresh();
            }
        });
        row->fixed(connect_button, connect_button->w());

        row->end();
        conn_editor->end();
        stage->set_centered(conn_editor);
        stage->end();
    }
}

void MainWindow::handle_new_conn() {
    auto window = new Fl_Double_Window(350, 295, "New Connection");
    window->xclass("lux-desktop");
    window->icon(&window_icon);
    window->size_range(350, 295, 0, 400);
    window->set_modal();

    auto conn_editor = new ConnectionEditor(10, 10, window->w() - 20, window->h() - 55);
    window->resizable(conn_editor);

    auto save_button = new Fl_Return_Button(window->w() - 85, window->h() - 40, 75, 30, "Save");
    FL_INLINE_CALLBACK_3(save_button, MainWindow*, main_window, this, Fl_Window*, window, window, ConnectionEditor*, conn_editor, conn_editor, {
        if (auto config_path = get_config_path(); !config_path.empty()) {
            auto conn_path = config_path / "connections";
            if (!std::filesystem::exists(conn_path)) {
                std::filesystem::create_directory(conn_path);
            }

            if (std::ofstream file((conn_path / conn_editor->name()).replace_extension(".json")); file.is_open()) {
                file << conn_editor->to_conn_info().to_json();
                file.close();
                main_window->refresh();
            } else {
                fl_message("Error: Failed to save file: Could not open file");
                return;
            }
        } else {
            fl_message("Error: Failed to save file: Could not find configuration directory");
            return;
        }

        window->hide();
    });

    auto cancel_button = new Fl_Button(save_button->x() - 80, window->h() - 40, 75, 30, "Cancel");
    FL_INLINE_CALLBACK_1(cancel_button, Fl_Window*, window, window, {
        window->hide();
    });

    window->end();
    window->show();
}
