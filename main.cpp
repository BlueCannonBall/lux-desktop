#include "Polyweb/polyweb.hpp"
#include "json.hpp"
#include <gtkmm.h>
#include <iostream>
#include <string>

using nlohmann::json;

class Lux : public Gtk::Window {
public:
    Lux():
        client_side_mouse_checkbutton("Client-side mouse"),
        login_button("Login") {
        set_default_size(480, -1);

        auto hbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 10);
        hbox->set_valign(Gtk::Align::CENTER);
        hbox->set_margin(20);

        auto title_label = Gtk::make_managed<Gtk::Label>("Lux Client");
        title_label->set_halign(Gtk::Align::START);
        {
            auto ctx = title_label->get_pango_context();
            auto font = ctx->get_font_description();
            font.set_weight(Pango::Weight::ULTRABOLD);
            font.set_size(PANGO_SCALE * 25);
            ctx->set_font_description(font);
        }
        hbox->append(*title_label);

        address_entry.set_placeholder_text("IP Address");
        hbox->append(address_entry);

        password_entry.property_placeholder_text() = "Password";
        hbox->append(password_entry);

        hbox->append(client_side_mouse_checkbutton);

        login_button.signal_clicked().connect(sigc::mem_fun(*this, &Lux::login));
        hbox->append(login_button);

        set_child(*hbox);
    }

protected:
    Gtk::Entry address_entry;
    Gtk::PasswordEntry password_entry;
    Gtk::CheckButton client_side_mouse_checkbutton;
    Gtk::Button login_button;

    void login() {
        json req_body = {
            {"password", password_entry.get_text()},
            {"show_mouse", client_side_mouse_checkbutton.get_active()},
            {"offer", "unknown"},
        };

        pw::HTTPResponse resp;
        if (pw::fetch("POST", "http://" + address_entry.get_text() + "/offer", resp, req_body.dump(), {{"Content-Type", "application/json"}}) == PN_ERROR) {
            auto error_dialog = Gtk::AlertDialog::create("Failed to connect");
            error_dialog->set_detail("Error: " + pw::universal_strerror());
            error_dialog->show(*this);
            return;
        } else if (resp.status_code != 200) {
            auto error_dialog = Gtk::AlertDialog::create("Failed to login");
            error_dialog->set_detail("Error: Response has status code " + std::to_string(resp.status_code));
            std::cout << resp.build_string() << std::endl;
            error_dialog->show(*this);
            return;
        }

        std::cout << "Working!" << std::endl;
    }
};

int main(int argc, char* argv[]) {
    pn::init();
    auto app = Gtk::Application::create("org.telewindow.lux");
    return app->make_window_and_run<Lux>(argc, argv);
}
