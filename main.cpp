#include "Polyweb/polyweb.hpp"
#include "glib.hpp"
#include "json.hpp"
#include <cstdlib>
#include <cstring>
#include <gst/gst.h>
#include <gtk/gtk.h>
#include <iostream>
#include <memory>
#include <rtc/rtc.hpp>

using nlohmann::json;

class Lux {
protected:
    glib::Object<GtkWidget> window;
    GtkWidget* vbox;
    GtkWidget* address_entry;
    GtkWidget* password_entry;
    GtkWidget* client_side_mouse_check_button;

    std::shared_ptr<rtc::PeerConnection> conn;
    std::shared_ptr<rtc::Track> track;
    std::shared_ptr<rtc::RtcpReceivingSession> session;

    glib::Object<GstElement> pipeline;
    pn::UniqueSocket<pn::udp::Client> gst_client;

    struct ErrorInfo {
        Lux* lux;
        std::string primary_text;
        std::string secondary_text;
        bool fatal = false;
    };

    // data should be an ErrorInfo object allocated with new, which will automatically be deleted
    static gboolean error_cb(gpointer data) {
        auto info = (ErrorInfo*) data;
        info->lux->error(info->primary_text, info->secondary_text);
        if (info->fatal) {
            exit(1);
        } else {
            delete info;
            return false;
        }
    }

    void error(const std::string& primary_text, const std::string& secondary_text) {
        std::cerr << primary_text << ": " << secondary_text << std::endl;
        GtkWidget* dialog = gtk_message_dialog_new(GTK_WINDOW(window.get()),
            GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_ERROR,
            GTK_BUTTONS_CLOSE,
            "%s",
            primary_text.c_str());
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s", secondary_text.c_str());
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
    }

public:
    static void activate_cb(GtkApplication* app, gpointer data) {
        ((Lux*) data)->activate(app);
    }

    void activate(GtkApplication* app) {
        window = gtk_application_window_new(app);
        gtk_window_set_title(GTK_WINDOW(window.get()), "Lux Desktop");
        gtk_window_set_default_size(GTK_WINDOW(window.get()), 450, 0);

        vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        g_object_set(vbox, "margin", 15, nullptr);
        gtk_widget_set_valign(vbox, GTK_ALIGN_CENTER);

        GtkWidget* title_label = gtk_label_new("<span size=\"26pt\" weight=\"ultrabold\">Lux Client</span>");
        gtk_label_set_use_markup(GTK_LABEL(title_label), true);
        gtk_widget_set_halign(title_label, GTK_ALIGN_START);
        gtk_container_add(GTK_CONTAINER(vbox), title_label);

        address_entry = gtk_entry_new();
        gtk_entry_set_placeholder_text(GTK_ENTRY(address_entry), "IP Address");
        gtk_container_add(GTK_CONTAINER(vbox), address_entry);

        password_entry = gtk_entry_new();
        gtk_entry_set_placeholder_text(GTK_ENTRY(password_entry), "Password");
        gtk_entry_set_visibility(GTK_ENTRY(password_entry), false);
        gtk_container_add(GTK_CONTAINER(vbox), password_entry);

        client_side_mouse_check_button = gtk_check_button_new_with_label("Client-side mouse");
        gtk_container_add(GTK_CONTAINER(vbox), client_side_mouse_check_button);

        GtkWidget* login_button = gtk_button_new_with_label("Login");
        g_signal_connect(login_button, "clicked", G_CALLBACK(Lux::login_cb), this);
        gtk_container_add(GTK_CONTAINER(vbox), login_button);

        gtk_container_add(GTK_CONTAINER(window.get()), vbox);
        gtk_widget_show_all(window.get());
    }

    ~Lux() {
        gst_element_set_state(pipeline.get(), GST_STATE_PAUSED);
    }

    static void login_cb(GtkWidget* button, gpointer data) {
        ((Lux*) data)->login(button);
    }

    void login(GtkWidget* button) {
        rtc::Configuration config;
        config.iceServers.emplace_back("stun.l.google.com:19302");
        conn = std::make_shared<rtc::PeerConnection>(config);

        conn->onStateChange(std::bind(&Lux::handle_state_change, this, std::placeholders::_1));
        conn->onGatheringStateChange(std::bind(&Lux::handle_gathering_state_change, this, std::placeholders::_1));

        rtc::Description::Video media("video", rtc::Description::Direction::RecvOnly);
        media.addH264Codec(96);
        media.setBitrate(4000);

        session = std::make_shared<rtc::RtcpReceivingSession>();
        track = conn->addTrack(media);
        track->setRtcpHandler(session);
        track->onMessage(std::bind(&Lux::handle_message, this, std::placeholders::_1), nullptr);

        conn->setLocalDescription();
    }

    void handle_state_change(rtc::PeerConnection::State state) {
        std::cout << "State: " << state << std::endl;
    }

    void handle_gathering_state_change(rtc::PeerConnection::GatheringState state) {
        std::cout << "Gathering state: " << state << std::endl;
        if (state == rtc::PeerConnection::GatheringState::Complete) {
            g_idle_add([](gpointer data) -> gboolean {
                auto lux = (Lux*) data;

                std::string offer;
                {
                    auto description = lux->conn->localDescription();
                    json offer_json = {
                        {"type", description->typeString()},
                        {"sdp", std::string(description.value())},
                    };
                    offer = offer_json.dump();
                }

                json req_body_json = {
                    {"password", gtk_entry_get_text(GTK_ENTRY(lux->password_entry))},
                    {"show_mouse", !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lux->client_side_mouse_check_button))},
                    {"offer", pw::base64_encode(offer.data(), offer.size())},
                };

                pw::HTTPResponse resp;
                if (pw::fetch("POST", "http://" + std::string(gtk_entry_get_text(GTK_ENTRY(lux->address_entry))) + "/offer", resp, req_body_json.dump(), {{"Content-Type", "application/json"}}) == PN_ERROR) {
                    lux->error("Failed to connect", "Error: " + pw::universal_strerror());
                    return false;
                } else if (resp.status_code != 200) {
                    lux->error("Failed to login", "Error: Response has status code " + std::to_string(resp.status_code));
                    return false;
                }

                lux->pipeline = gst_pipeline_new(nullptr);
                glib::Object<GstElement> udpsrc = gst_element_factory_make("udpsrc", nullptr);
                glib::Object<GstElement> rtph264depay = gst_element_factory_make("rtph264depay", nullptr);
                glib::Object<GstElement> queue = gst_element_factory_make("queue", nullptr);
                glib::Object<GstElement> avdec_h264 = gst_element_factory_make("avdec_h264", nullptr);
                glib::Object<GstElement> glsinkbin = gst_element_factory_make("glsinkbin", nullptr);
                glib::Object<GstElement> gtkglsink = gst_element_factory_make("gtkglsink", nullptr);

                glib::Object<GstCaps> caps = gst_caps_new_simple("application/x-rtp", "media", G_TYPE_STRING, "video", "clock-rate", G_TYPE_INT, 90000, "encoding-name", G_TYPE_STRING, "H264", nullptr);
                g_object_set(udpsrc.get(), "caps", caps.get(), nullptr);
                g_object_set(udpsrc.get(), "address", "127.0.0.1", nullptr);

                gint port;
                g_object_get(udpsrc.get(), "port", &port, nullptr);
                std::cout << "Using port: " << port << std::endl;
                if (lux->gst_client->connect("127.0.0.1", port) == PN_ERROR) {
                    lux->error("Failed to start streaming", "Error: Failed to connect to GStreamer: " + pn::universal_strerror());
                    return false;
                }

                gst_bin_add_many(GST_BIN(lux->pipeline.get()), udpsrc.get(), rtph264depay.get(), queue.get(), avdec_h264.get(), glsinkbin.get(), nullptr);
                if (!gst_element_link_many(udpsrc.get(), rtph264depay.get(), queue.get(), avdec_h264.get(), glsinkbin.get(), NULL)) {
                    lux->error("Failed to start streaming", "Error: Failed to link GStreamer elements");
                    return false;
                }

                GtkWidget* video;
                g_object_set(glsinkbin.get(), "sink", gtkglsink.get(), nullptr);
                g_object_get(gtkglsink.get(), "widget", &video, nullptr);
                gtk_container_remove(GTK_CONTAINER(lux->window.get()), lux->vbox);
                gtk_container_add(GTK_CONTAINER(lux->window.get()), video);
                gtk_widget_show(video);
                gst_element_set_state(lux->pipeline.get(), GST_STATE_PLAYING);

                json answer_json = json::parse(resp.body_string());
                auto sdp_data = pw::base64_decode(answer_json["Offer"]);
                answer_json = json::parse(sdp_data);

                rtc::Description answer(answer_json["sdp"].get<std::string>(), answer_json["type"].get<std::string>());
                lux->conn->setRemoteDescription(answer);

                return false;
            },
                this);
        }
    }

    void handle_message(rtc::binary message) {
        if (gst_client->sendto(message.data(), message.size(), nullptr, 0) == PN_ERROR) {
            g_idle_add(error_cb,
                new ErrorInfo {
                    .lux = this,
                    .primary_text = "Failed to send RTP packet to GStreamer",
                    .secondary_text = "Error: " + pn::universal_strerror(),
                    .fatal = true,
                });
        }
    }
};

int main(int argc, char* argv[]) {
    pn::init();
    rtc::InitLogger(rtc::LogLevel::Debug);
    gst_init(&argc, &argv);

    Lux lux;
    glib::Object<GtkApplication> app = gtk_application_new("org.telewindow.lux", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app.get(), "activate", G_CALLBACK(Lux::activate_cb), &lux);

    int result = g_application_run(G_APPLICATION(app.get()), argc, argv);
    return result;
}
