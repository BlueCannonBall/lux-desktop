#include "Polyweb/polyweb.hpp"
#include "glib.hpp"
#include "json.hpp"
#include "keys.hpp"
#include "mouse.hpp"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <gst/gst.h>
#include <gst/video/gstvideosink.h>
#include <gtk/gtk.h>
#include <iostream>
#include <memory>
#include <rtc/rtc.hpp>
#include <string>

using nlohmann::json;

void position_in_video(int x, int y, int video_width, int video_height, GtkWidget* window, int& x_ret, int& y_ret) {
    float video_aspect_ratio = (float) video_width / video_height;

    gint window_width;
    gint window_height;
    gtk_window_get_size(GTK_WINDOW(window), &window_width, &window_height);
    float window_aspect_ratio = (float) window_width / window_height;

    if (video_aspect_ratio > window_aspect_ratio) {
        x_ret = x / ((float) window_width / video_width);
        y_ret = (y - ((1.f - window_aspect_ratio / video_aspect_ratio) * window_height / 2.f)) / ((float) window_width / video_width);
    } else if (video_aspect_ratio < window_aspect_ratio) {
        x_ret = (x - ((1.f - video_aspect_ratio / window_aspect_ratio) * window_width / 2.f)) / ((float) window_height / video_height);
        y_ret = y / ((float) window_height / video_height);
    } else {
        x_ret = x / ((float) window_width / video_width);
        y_ret = y / ((float) window_height / video_height);
    }
}

class Lux {
protected:
    GtkWidget* window;
    GtkWidget* vbox;
    GtkWidget* address_entry;
    GtkWidget* password_entry;
    GtkWidget* client_side_mouse_check_button;

    bool client_side_mouse;

    std::shared_ptr<rtc::PeerConnection> conn;
    std::shared_ptr<rtc::Track> track;
    std::shared_ptr<rtc::RtcpReceivingSession> session;
    std::shared_ptr<rtc::DataChannel> ordered_channel;
    std::shared_ptr<rtc::DataChannel> unordered_channel;

    glib::Object<GstElement> pipeline;
    pn::UniqueSocket<pn::udp::Client> gst_client;
    gint width;
    gint height;

    glib::Object<GdkDisplay> display;
    glib::Object<GdkSeat> seat;
    glib::Object<GdkCursor> blank_cursor;
    bool seat_grabbed = false;
    std::unique_ptr<RawMouseManager> raw_mouse_manager;
    gdouble scroll_delta_x = 0.;
    gdouble scroll_delta_y = 0.;

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
            return FALSE;
        }
    }

    void error(const std::string& primary_text, const std::string& secondary_text) {
        std::cerr << primary_text << ": " << secondary_text << std::endl;
        GtkWidget* dialog = gtk_message_dialog_new(GTK_WINDOW(window),
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
    Lux() = default;
    Lux(GtkApplication* app) {
        init(app);
    }

    static void init_cb(GtkApplication* app, gpointer data) {
        ((Lux*) data)->init(app);
    }

    void init(GtkApplication* app) {
        window = gtk_application_window_new(app);
        gtk_window_set_title(GTK_WINDOW(window), "Lux Desktop");
        gtk_window_set_default_size(GTK_WINDOW(window), 450, 0);

        vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        g_object_set(vbox, "margin", 15, nullptr);
        gtk_widget_set_valign(vbox, GTK_ALIGN_CENTER);

        GtkWidget* title_label = gtk_label_new("<span size=\"25pt\" weight=\"ultrabold\">Lux Client</span>");
        gtk_label_set_use_markup(GTK_LABEL(title_label), TRUE);
        gtk_widget_set_halign(title_label, GTK_ALIGN_START);
        gtk_container_add(GTK_CONTAINER(vbox), title_label);

        address_entry = gtk_entry_new();
        gtk_entry_set_placeholder_text(GTK_ENTRY(address_entry), "IP Address");
        g_signal_connect(address_entry, "activate", G_CALLBACK(connect_cb), this);
        gtk_container_add(GTK_CONTAINER(vbox), address_entry);

        password_entry = gtk_entry_new();
        gtk_entry_set_placeholder_text(GTK_ENTRY(password_entry), "Password");
        gtk_entry_set_visibility(GTK_ENTRY(password_entry), FALSE);
        g_signal_connect(password_entry, "activate", G_CALLBACK(connect_cb), this);
        gtk_container_add(GTK_CONTAINER(vbox), password_entry);

        client_side_mouse_check_button = gtk_check_button_new_with_label("Client-side mouse");
        gtk_container_add(GTK_CONTAINER(vbox), client_side_mouse_check_button);

        GtkWidget* login_button = gtk_button_new_with_label("Login");
        g_signal_connect(login_button, "clicked", G_CALLBACK(connect_cb), this);
        gtk_container_add(GTK_CONTAINER(vbox), login_button);

        gtk_container_add(GTK_CONTAINER(window), vbox);
        gtk_widget_show_all(window);
    }

    static void connect_cb(GtkWidget*, gpointer data) {
        ((Lux*) data)->connect();
    }

    void connect() {
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

        ordered_channel = conn->createDataChannel("ordered-input");
        unordered_channel = conn->createDataChannel("unordered-input",
            {
                .reliability = {
                    .unordered = true,
                },
            });
        client_side_mouse = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(client_side_mouse_check_button));

        conn->setLocalDescription();
    }

    void handle_state_change(rtc::PeerConnection::State state) {
        std::cout << "State: " << state << std::endl;
    }

    void handle_gathering_state_change(rtc::PeerConnection::GatheringState state) {
        std::cout << "Gathering state: " << state << std::endl;
        if (state == rtc::PeerConnection::GatheringState::Complete) {
            g_idle_add(login_cb, this);
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

    static gboolean login_cb(gpointer data) {
        ((Lux*) data)->login();
        return FALSE;
    }

    void login() {
        std::string offer;
        {
            auto description = conn->localDescription();
            json offer_json = {
                {"type", description->typeString()},
                {"sdp", std::string(description.value())},
            };
            offer = offer_json.dump();
        }

        json req_body_json = {
            {"password", gtk_entry_get_text(GTK_ENTRY(password_entry))},
            {"show_mouse", !client_side_mouse},
            {"offer", pw::base64_encode(offer.data(), offer.size())},
        };
        std::cout << "Sending offer: " << req_body_json << std::endl;

        pw::HTTPResponse resp;
        if (pw::fetch("POST", "http://" + std::string(gtk_entry_get_text(GTK_ENTRY(address_entry))) + "/offer", resp, req_body_json.dump(), {{"Content-Type", "application/json"}}) == PN_ERROR) {
            error("Failed to connect", "Error: " + pw::universal_strerror());
            return;
        } else if (resp.status_code != 200) {
            error("Failed to login", "Error: Response has status code " + std::to_string(resp.status_code));
            return;
        }

        std::unique_ptr<rtc::Description> answer;
        try {
            json answer_json = json::parse(resp.body_string());
            auto sdp_data = pw::base64_decode(answer_json["Offer"]);
            answer_json = json::parse(sdp_data);
            answer = std::make_unique<rtc::Description>(answer_json["sdp"].get<std::string>(), answer_json["type"].get<std::string>());
        } catch (const std::exception& e) {
            error("Failed to start streaming", "Error: Failed to parse server answer: " + std::string(e.what()));
            return;
        }

        pipeline = gst_pipeline_new(nullptr);
        glib::Object<GstElement> udpsrc = gst_element_factory_make("udpsrc", nullptr);
        glib::Object<GstElement> rtph264depay = gst_element_factory_make("rtph264depay", nullptr);
        glib::Object<GstElement> queue = gst_element_factory_make("queue", nullptr);
        glib::Object<GstElement> avdec_h264 = gst_element_factory_make("avdec_h264", nullptr);
        glib::Object<GstElement> glsinkbin = gst_element_factory_make("glsinkbin", nullptr);
        glib::Object<GstElement> gtkglsink = gst_element_factory_make("gtkglsink", nullptr);

        GstCaps* caps = gst_caps_new_simple("application/x-rtp", "media", G_TYPE_STRING, "video", "clock-rate", G_TYPE_INT, 90000, "encoding-name", G_TYPE_STRING, "H264", nullptr);
        g_object_set(udpsrc.get(), "caps", caps, "address", "127.0.0.1", nullptr);
        gst_caps_unref(caps);

        gint port;
        g_object_get(udpsrc.get(), "port", &port, nullptr);
        std::cout << "Using port: " << port << std::endl;
        if (gst_client->connect("127.0.0.1", port) == PN_ERROR) {
            error("Failed to start streaming", "Error: Failed to connect to GStreamer: " + pn::universal_strerror());
            return;
        }

        glib::Object<GstPad> pad = gst_element_get_static_pad(avdec_h264.get(), "src");
        gst_pad_add_probe(pad.get(), GST_PAD_PROBE_TYPE_EVENT_BOTH, handle_pad_cb, this, nullptr);

        gst_bin_add_many(GST_BIN(pipeline.get()), udpsrc.get(), rtph264depay.get(), queue.get(), avdec_h264.get(), glsinkbin.get(), nullptr);
        if (!gst_element_link_many(udpsrc.get(), rtph264depay.get(), queue.get(), avdec_h264.get(), glsinkbin.get(), NULL)) {
            error("Failed to start streaming", "Error: Failed to link GStreamer elements");
            return;
        }

        GtkWidget* video;
        g_object_set(glsinkbin.get(), "sink", gtkglsink.get(), nullptr);
        g_object_get(gtkglsink.get(), "widget", &video, nullptr);
        gtk_container_remove(GTK_CONTAINER(window), vbox);
        gtk_container_add(GTK_CONTAINER(window), video);
        gtk_widget_show(video);

        gst_element_set_state(pipeline.get(), GST_STATE_PLAYING);
        conn->setRemoteDescription(*answer);

        gtk_widget_add_events(window, GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_SCROLL_MASK | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);
        g_signal_connect(window, "button-press-event", G_CALLBACK(handle_button_cb), this);
        g_signal_connect(window, "button-release-event", G_CALLBACK(handle_button_cb), this);
        g_signal_connect(window, "scroll-event", G_CALLBACK(handle_scroll_cb), this);
        g_signal_connect(window, "key-press-event", G_CALLBACK(handle_key_cb), this);
        g_signal_connect(window, "key-release-event", G_CALLBACK(handle_key_cb), this);

        if (!client_side_mouse) {
            display = gdk_display_get_default();
            seat = gdk_display_get_default_seat(display.get());
            blank_cursor = gdk_cursor_new_for_display(display.get(), GDK_BLANK_CURSOR);
            gdk_seat_grab(seat.get(), gtk_widget_get_window(window), GDK_SEAT_CAPABILITY_ALL_POINTING, FALSE, blank_cursor.get(), nullptr, nullptr, nullptr);
            seat_grabbed = true;

            gtk_widget_add_events(window, GDK_FOCUS_CHANGE_MASK);
            g_signal_connect(window, "focus-in-event", G_CALLBACK(handle_focus_cb), this);
            g_signal_connect(window, "focus-out-event", G_CALLBACK(handle_focus_cb), this);

            raw_mouse_manager = std::make_unique<RawMouseManager>();
            g_timeout_add(17, handle_raw_motion_cb, this);
        } else {
            g_signal_connect(window, "motion-notify-event", G_CALLBACK(handle_motion_cb), this);
        }
    }

    static GstPadProbeReturn handle_pad_cb(GstPad* pad, GstPadProbeInfo* info, gpointer data) {
        ((Lux*) data)->handle_pad(pad, info);
        return GST_PAD_PROBE_OK;
    }

    void handle_pad(GstPad* pad, GstPadProbeInfo* info) {
        GstEvent* event = GST_PAD_PROBE_INFO_EVENT(info);
        if (GST_EVENT_TYPE(event) == GST_EVENT_CAPS) {
            GstCaps* caps = gst_caps_new_any();
            gst_event_parse_caps(event, &caps);

            GstStructure* caps_struct = gst_caps_get_structure(caps, 0);
            gst_structure_get_int(caps_struct, "width", &width);
            gst_structure_get_int(caps_struct, "height", &height);

            gst_caps_unref(caps);
        }
    }

    static gboolean handle_focus_cb(GtkWidget*, GdkEventFocus* event, gpointer data) {
        ((Lux*) data)->handle_focus(*event);
        return FALSE;
    }

    void handle_focus(const GdkEventFocus& event) {
        if (event.in) {
            gdk_seat_grab(seat.get(), gtk_widget_get_window(window), GDK_SEAT_CAPABILITY_ALL_POINTING, FALSE, blank_cursor.get(), nullptr, nullptr, nullptr);
        } else {
            gdk_seat_ungrab(seat.get());
        }
        seat_grabbed = event.in;
    }

    static gboolean handle_motion_cb(GtkWidget*, GdkEventMotion* event, gpointer data) {
        ((Lux*) data)->handle_motion(*event);
        return FALSE;
    }

    void handle_motion(const GdkEventMotion& event) {
        if (ordered_channel->isOpen()) {
            int x;
            int y;
            position_in_video(event.x, event.y, width, height, window, x, y);

            json message = {
                {"type", "mousemoveabs"},
                {"x", x},
                {"y", y},
            };
            ordered_channel->send(message.dump());
        }
    }

    static gboolean handle_raw_motion_cb(gpointer data) {
        ((Lux*) data)->handle_raw_motion();
        return TRUE;
    }

    void handle_raw_motion() {
        if (unordered_channel->isOpen() && seat_grabbed && raw_mouse_manager->pending()) {
            RawMouseEvent raw_event = raw_mouse_manager->next_event();
            json message = {
                {"type", "mousemove"},
                {"x", (int) raw_event.x / 2},
                {"y", (int) raw_event.y / 2},
            };
            unordered_channel->send(message.dump());
        }
    }

    static gboolean handle_button_cb(GtkWidget*, GdkEventButton* event, gpointer data) {
        ((Lux*) data)->handle_button(*event);
        return FALSE;
    }

    void handle_button(const GdkEventButton& event) {
        if (ordered_channel->isOpen() &&
            event.type != GDK_2BUTTON_PRESS &&
            event.type != GDK_3BUTTON_PRESS) {
            json message = {
                {"type", event.type == GDK_BUTTON_RELEASE ? "mouseup" : "mousedown"},
                {"button", event.button - 1},
            };
            ordered_channel->send(message.dump());
        }
    }

    static gboolean handle_scroll_cb(GtkWidget*, GdkEventScroll* event, gpointer data) {
        ((Lux*) data)->handle_scroll(*event);
        return FALSE;
    }

    void handle_scroll(const GdkEventScroll& event) {
        if (ordered_channel->isOpen()) {
            scroll_delta_x += event.delta_x;
            scroll_delta_y += event.delta_y;
            std::cout << scroll_delta_x << ' ' << scroll_delta_y << std::endl;

            if (std::abs(scroll_delta_x) >= 1 || std::abs(scroll_delta_y) >= 1) {
                json message = {
                    {"type", "wheel"},
                    {"x", scroll_delta_x * 120.},
                    {"y", scroll_delta_y * 120.},
                };
                ordered_channel->send(message.dump());

                scroll_delta_x = 0.;
                scroll_delta_y = 0.;
            }
        }
    }

    static gboolean handle_key_cb(GtkWidget*, GdkEventKey* event, gpointer data) {
        ((Lux*) data)->handle_key(*event);
        return FALSE;
    }

    void handle_key(const GdkEventKey& event) {
        if (ordered_channel->isOpen()) {
            json message = {
                {"type", event.type == GDK_KEY_RELEASE ? "keyup" : "keydown"},
                {"key", gdk_to_browser_key(event.keyval)},
            };
            ordered_channel->send(message.dump());
        }
    }
};

int main(int argc, char* argv[]) {
    pn::init();
    rtc::InitLogger(rtc::LogLevel::Info);
    gst_init(&argc, &argv);

    Lux lux;
    glib::Object<GtkApplication> app = gtk_application_new("org.telewindow.lux", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app.get(), "activate", G_CALLBACK(Lux::init_cb), &lux);

    int result = g_application_run(G_APPLICATION(app.get()), argc, argv);
    return result;
}
