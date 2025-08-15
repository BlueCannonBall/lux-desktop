#define NOMINMAX

#include "ui.hpp"
#include "json.hpp"
#include <FL/Fl_Box.H>
#include <FL/fl_callback_macros.H>
#include <FL/fl_message.H>
#include <filesystem>
#include <fstream>
#include <stdlib.h>
#ifdef _WIN32
    #include "theme.hpp"
    #include <FL/x.H>
#endif

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
    Fl_Double_Window(1000, 650, "Lux Client") {
    xclass("lux-desktop");
    size_range(500, 400);

    auto column = new Fl_Flex(0, 0, w(), h(), Fl_Flex::COLUMN);

    menu_bar = new Fl_Menu_Bar(0, 0, w(), 30);
    menu_bar->add("File/New Connection", FL_CTRL + 'n', [](Fl_Widget*, void* data) {
        auto window = (MainWindow*) data;
        window->handle_new_conn();
    },
        this);
    menu_bar->add("File/Upload", 0, [](Fl_Widget*, void* data) {
        auto window = (MainWindow*) data;
        window->handle_upload();
    },
        this);
    menu_bar->add("File/Download", 0, [](Fl_Widget*, void* data) {
        auto window = (MainWindow*) data;
        window->handle_download();
    },
        this);
    menu_bar->add("File/Quit", 0, [](Fl_Widget*, void*) {
        exit(EXIT_SUCCESS);
    });
    menu_bar->add("Edit/Release All Keys", 0, [](Fl_Widget*, void* data) {
        auto window = (MainWindow*) data;
        if (window->video_window) {
            window->video_window->release_all_keys();
        }
    },
        this);
    menu_bar->add("View/Toggle Fullscreen", FL_F + 11, [](Fl_Widget*, void* data) {
        auto window = (MainWindow*) data;
        window->handle_toggle_fullscreen();
    },
        this);
    menu_bar->add("View/Set Bitrate", 0, [](Fl_Widget*, void* data) {
        auto window = (MainWindow*) data;
        window->handle_set_bitrate();
    },
        this);
    menu_bar->add("View/Request Keyframe", FL_F + 5, [](Fl_Widget*, void* data) {
        auto window = (MainWindow*) data;
        if (window->video_window) {
            window->video_window->request_keyframe();
        }
    },
        this);
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

int MainWindow::handle(int event) {
    if (event == FL_SHORTCUT && is_key_global_shortcut(Fl::event_key())) {
        return menu_bar->handle(event); // Shortcuts should be handled by the menu bar even if it isn't visible
    } else {
        return Fl_Double_Window::handle(event);
    }
}

void MainWindow::hide() {
    // Ensure that file transfers are cleanly cancelled
    conn_list->deselect();
    handle_select_conn();
    Fl_Double_Window::hide();
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
                        conn_list->add(("@b" + entry.path().stem().string()).c_str(), conn_info.get());
                    }
                }
            }
        }
    }
}

void MainWindow::handle_select_conn() {
    Fl::remove_timeout(check_ice_state, this);
    stage->set_centered(nullptr);
    stage->set_fill(false);
    delete conn_editor;
    delete video_window;
    conn_editor = nullptr;
    video_window = nullptr;

    if (conn_list->value()) {
        label((std::string(conn_list->text(conn_list->value())).substr(2) + " - Lux Client").c_str());
        stage->begin();

        conn_editor = new ConnectionEditor(0, 0, 350, 275, std::string(conn_list->text(conn_list->value())).substr(2), *(ConnectionInfo*) conn_list->data(conn_list->value()));
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
                    std::filesystem::remove(conn_path / (std::string(window->conn_list->text(index)).substr(2) + ".json"));
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
                    fl_alert("Error: Failed to save file: Could not open file");
                    return;
                }

                if (window->conn_editor->name() != std::string(window->conn_list->text(index)).substr(2)) {
                    std::filesystem::remove((conn_path / std::string(window->conn_list->text(index)).substr(2)).replace_extension(".json"));
                    window->conn_list->text(index, ("@b" + window->conn_editor->name()).c_str());
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
            Fl::add_timeout(2., check_ice_state, window);
        });
        row->fixed(connect_button, connect_button->w());

        row->end();
        conn_editor->end();
        stage->set_centered(conn_editor);
        stage->end();
    } else {
        label("Lux Client");
    }
}

void MainWindow::handle_new_conn() {
    auto window = new Fl_Double_Window(350, 295, "New Connection");
    window->size_range(350, 295, 0, 400);
    window->set_modal();

    auto conn_editor = new ConnectionEditor(10, 10, window->w() - 20, window->h() - 55);
    window->resizable(conn_editor);

    auto save_button = new Fl_Button(window->w() - 85, window->h() - 40, 75, 30, "Save");
    FL_INLINE_CALLBACK_3(save_button, MainWindow*, main_window, this, Fl_Double_Window*, window, window, ConnectionEditor*, conn_editor, conn_editor, {
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
                fl_alert("Error: Failed to save file: Could not open file");
                return;
            }
        } else {
            fl_alert("Error: Failed to save file: Could not find configuration directory");
            return;
        }

        window->hide();
        Fl::delete_widget(window);
    });

    auto cancel_button = new Fl_Button(save_button->x() - 80, window->h() - 40, 75, 30, "Cancel");
    FL_INLINE_CALLBACK_1(cancel_button, Fl_Double_Window*, window, window, {
        window->hide();
        Fl::delete_widget(window);
    });

    window->callback([](Fl_Widget* window, void*) {
        window->hide();
        Fl::delete_widget(window);
    });

    window->end();
    window->show();
#ifdef _WIN32
    Fl::flush();
    set_window_dark_mode(fl_xid(window));
#endif
}

void MainWindow::handle_upload() {
    if (video_window) {
        if (video_window->file_manager) {
            video_window->file_manager->upload();
        } else {
            fl_alert("Error: This connection is view-only.");
        }
    } else {
        fl_alert("Error: There is no active connection");
    }
}

void MainWindow::handle_download() {
    if (video_window) {
        if (video_window->file_manager) {
            video_window->file_manager->download();
        } else {
            fl_alert("Error: This connection is view-only.");
        }
    } else {
        fl_alert("Error: There is no active connection");
    }
}

void MainWindow::handle_set_bitrate() {
    if (video_window) {
        auto window = new Fl_Double_Window(250, 85, "Set Bitrate");
        window->set_modal();

        auto row = new Fl_Flex(10, 10, window->w() - 20, window->h() - 55, Fl_Flex::ROW);
        auto label = new Label(0, 0, "Bitrate: ");
        auto bitrate_spinner = new Fl_Spinner(0, 0, 0, 0);
        bitrate_spinner->type(FL_INT_INPUT);
        bitrate_spinner->minimum(500);
        bitrate_spinner->maximum(10000);
        bitrate_spinner->value(video_window->get_bitrate());
        row->fixed(label, label->w());
        row->end();

        auto ok_button = new Fl_Button(window->w() - 85, window->h() - 40, 75, 30, "OK");
        FL_INLINE_CALLBACK_3(ok_button, MainWindow*, main_window, this, Fl_Double_Window*, window, window, Fl_Spinner*, bitrate_spinner, bitrate_spinner, {
            main_window->video_window->set_bitrate(bitrate_spinner->value());
            window->hide();
            Fl::delete_widget(window);
        });

        auto cancel_button = new Fl_Button(ok_button->x() - 80, window->h() - 40, 75, 30, "Cancel");
        FL_INLINE_CALLBACK_1(cancel_button, Fl_Double_Window*, window, window, {
            window->hide();
            Fl::delete_widget(window);
        });

        window->callback([](Fl_Widget* window, void*) {
            window->hide();
            Fl::delete_widget(window);
        });

        window->end();
        window->show();
#ifdef _WIN32
        Fl::flush();
        set_window_dark_mode(fl_xid(window));
#endif
    } else {
        fl_alert("Error: There is no active connection");
    }
}

void MainWindow::handle_toggle_fullscreen() {
    if (fullscreen_active()) {
        menu_bar->show();
        conn_list->show();
        fullscreen_off();
        stage->resize(conn_list->x() + conn_list->w(), tile->y(), tile->w() - (conn_list->x() + conn_list->w()), tile->h());
    } else {
        menu_bar->hide();
        conn_list->hide();
        fullscreen();
        stage->resize(tile->x(), tile->y(), tile->w(), tile->h());
    }
}

void MainWindow::check_ice_state(void* data) {
    auto window = (MainWindow*) data;
    if (auto ice_state = window->video_window->ice_state();
        ice_state == rtc::PeerConnection::IceState::Closed ||
        ice_state == rtc::PeerConnection::IceState::Disconnected ||
        ice_state == rtc::PeerConnection::IceState::Failed) {
        fl_message("The connection has closed.");
        window->refresh();
    } else {
        Fl::repeat_timeout(2., check_ice_state, data);
    }
}
