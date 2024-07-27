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

class Lux {
protected:
    GtkWidget* window;
    GtkWidget* address_entry;
    GtkWidget* password_entry;
    GtkWidget* client_side_mouse_check_button;

    std::shared_ptr<rtc::PeerConnection> conn;
    std::shared_ptr<rtc::Track> track;
    std::shared_ptr<rtc::RtcpReceivingSession> session;
    std::shared_ptr<rtc::DataChannel> ordered_channel;
    std::shared_ptr<rtc::DataChannel> unordered_channel;

    glib::Object<GstElement> pipeline;
    pn::UniqueSocket<pn::udp::Client> gst_client;
    gint video_width;
    gint video_height;

    bool client_side_mouse;
    RawMouseManager raw_mouse_manager;

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
        static constexpr const char* buttons[] = {"Close", nullptr};

        std::cerr << primary_text << ": " << secondary_text << std::endl;
        glib::Object<GtkAlertDialog> dialog = gtk_alert_dialog_new("%s", primary_text.c_str());
        gtk_alert_dialog_set_detail(dialog.get(), secondary_text.c_str());
        gtk_alert_dialog_set_buttons(dialog.get(), buttons);
        gtk_alert_dialog_show(dialog.get(), GTK_WINDOW(window));
    }

    void position_in_video(int x, int y, int& x_ret, int& y_ret) {
        float video_aspect_ratio = (float) video_width / video_height;

        gint window_width = gtk_widget_get_width(window);
        gint window_height = gtk_widget_get_height(window);
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

        GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        g_object_set(vbox, "margin-top", 15, "margin-bottom", 15, "margin-start", 15, "margin-end", 15, nullptr);
        gtk_widget_set_valign(vbox, GTK_ALIGN_CENTER);

        GtkWidget* title_label = gtk_label_new("<span size=\"25pt\" weight=\"ultrabold\">Lux Client</span>");
        gtk_label_set_use_markup(GTK_LABEL(title_label), TRUE);
        gtk_widget_set_halign(title_label, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(vbox), title_label);

        address_entry = gtk_entry_new();
        gtk_entry_set_placeholder_text(GTK_ENTRY(address_entry), "IP Address");
        g_signal_connect(address_entry, "activate", G_CALLBACK(connect_cb), this);
        gtk_box_append(GTK_BOX(vbox), address_entry);

        password_entry = gtk_entry_new();
        gtk_entry_set_placeholder_text(GTK_ENTRY(password_entry), "Password");
        gtk_entry_set_visibility(GTK_ENTRY(password_entry), FALSE);
        g_signal_connect(password_entry, "activate", G_CALLBACK(connect_cb), this);
        gtk_box_append(GTK_BOX(vbox), password_entry);

        client_side_mouse_check_button = gtk_check_button_new_with_label("Client-side mouse");
        gtk_box_append(GTK_BOX(vbox), client_side_mouse_check_button);

        GtkWidget* login_button = gtk_button_new_with_label("Login");
        g_signal_connect(login_button, "clicked", G_CALLBACK(connect_cb), this);
        gtk_box_append(GTK_BOX(vbox), login_button);

        gtk_window_set_child(GTK_WINDOW(window), vbox);
        gtk_window_present(GTK_WINDOW(window));
    }

    ~Lux() {
        if (pipeline) {
            gst_element_set_state(pipeline.get(), GST_STATE_PAUSED);
            gst_element_set_state(pipeline.get(), GST_STATE_READY);
            gst_element_set_state(pipeline.get(), GST_STATE_NULL);
        }
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

        session = std::make_shared<rtc::RtcpReceivingSession>();
        track = conn->addTrack(media);
        track->setRtcpHandler(session);
        track->onMessage(std::bind(&Lux::handle_message, this, std::placeholders::_1), nullptr);

        client_side_mouse = gtk_check_button_get_active(GTK_CHECK_BUTTON(client_side_mouse_check_button));

        ordered_channel = conn->createDataChannel("ordered-input");
        if (!client_side_mouse) {
            unordered_channel = conn->createDataChannel("unordered-input",
                {
                    .reliability = {
                        .unordered = true,
                    },
                });
        }

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
            {"password", gtk_editable_get_text(GTK_EDITABLE(password_entry))},
            {"show_mouse", !client_side_mouse},
            {"offer", pw::base64_encode(offer.data(), offer.size())},
        };
        std::cout << "Sending offer: " << req_body_json << std::endl;

        pw::HTTPResponse resp;
        if (pw::fetch("POST", "http://" + std::string(gtk_editable_get_text(GTK_EDITABLE(address_entry))) + "/offer", resp, req_body_json.dump(), {{"Content-Type", "application/json"}}) == PN_ERROR) {
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

        GstElement* udpsrc = gst_element_factory_make("udpsrc", nullptr);
        {
            GstCaps* caps = gst_caps_new_simple("application/x-rtp", "media", G_TYPE_STRING, "video", "encoding-name", G_TYPE_STRING, "H264", "clock-rate", G_TYPE_INT, 90000, nullptr);
            g_object_set(udpsrc, "caps", caps, "address", "127.0.0.1", nullptr);
            gst_caps_unref(caps);

            gint port;
            g_object_get(udpsrc, "port", &port, nullptr);
            std::cout << "Using port: " << port << std::endl;
            if (gst_client->connect("127.0.0.1", port) == PN_ERROR) {
                error("Failed to start streaming", "Error: Failed to connect to GStreamer: " + pn::universal_strerror());
                gst_object_unref(udpsrc);
                return;
            }
        }

        GstElement* rtph264depay = gst_element_factory_make("rtph264depay", nullptr);

        GstElement* queue = gst_element_factory_make("queue", nullptr);

        GstElement* avdec_h264 = gst_element_factory_make("avdec_h264", nullptr);
        {
            glib::Object<GstPad> pad = gst_element_get_static_pad(avdec_h264, "src");
            gst_pad_add_probe(pad.get(), GST_PAD_PROBE_TYPE_EVENT_BOTH, handle_pad_cb, this, nullptr);
        }

        GstElement* videoconvert = gst_element_factory_make("videoconvert", nullptr);

        GstElement* gtk4paintablesink = gst_element_factory_make("gtk4paintablesink", nullptr);
        GdkPaintable* paintable;
        g_object_get(gtk4paintablesink, "paintable", &paintable, nullptr);

        gst_bin_add_many(GST_BIN(pipeline.get()),
            udpsrc,
            rtph264depay,
            queue,
            avdec_h264,
            videoconvert,
            gtk4paintablesink,
            nullptr);
        if (!gst_element_link_many(udpsrc,
                rtph264depay,
                queue,
                avdec_h264,
                videoconvert,
                gtk4paintablesink,
                nullptr)) {
            error("Failed to start streaming", "Error: Failed to link GStreamer elements");
            return;
        }

        GtkWidget* video = gtk_picture_new_for_paintable(paintable);
        gtk_window_set_child(GTK_WINDOW(window), video);

        gst_element_set_state(pipeline.get(), GST_STATE_PLAYING);
        gtk_window_fullscreen(GTK_WINDOW(window));
        conn->setRemoteDescription(*answer);

        GtkEventController* key_event_controller = gtk_event_controller_key_new();
        g_signal_connect(key_event_controller, "key-pressed", G_CALLBACK(handle_key_pressed_cb), this);
        g_signal_connect(key_event_controller, "key-released", G_CALLBACK(handle_key_released_cb), this);
        gtk_widget_add_controller(window, key_event_controller);

        GtkEventController* scroll_event_controller = gtk_event_controller_scroll_new((GtkEventControllerScrollFlags) (GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES | GTK_EVENT_CONTROLLER_SCROLL_DISCRETE));
        g_signal_connect(scroll_event_controller, "scroll", G_CALLBACK(handle_scroll_cb), this);
        gtk_widget_add_controller(window, scroll_event_controller);

        GtkGesture* click_gesture = gtk_gesture_click_new();
        gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click_gesture), 0);
        g_signal_connect(click_gesture, "pressed", G_CALLBACK(handle_button_pressed_cb), this);
        g_signal_connect(click_gesture, "released", G_CALLBACK(handle_button_released_cb), this);
        gtk_widget_add_controller(window, GTK_EVENT_CONTROLLER(click_gesture));

        if (!client_side_mouse) {
            raw_mouse_manager.lock_mouse(gtk_native_get_surface(gtk_widget_get_native(window)));
            g_signal_connect(window, "notify::is-active", G_CALLBACK(handle_focus_change_cb), this);
            g_timeout_add(5, handle_raw_motion_cb, this);
        } else {
            GtkEventController* motion_event_controller = gtk_event_controller_motion_new();
            g_signal_connect(motion_event_controller, "motion", G_CALLBACK(handle_motion_cb), this);
            gtk_widget_add_controller(window, motion_event_controller);
        }
    }

    static void handle_recovery_cb(GObject* rtpulpfecdec, GParamSpec*, gpointer data) {
        ((Lux*) data)->handle_recovery(rtpulpfecdec);
    }

    void handle_recovery(GObject* rtpulpfecdec) {
        guint recovered;
        g_object_get(rtpulpfecdec, "recovered", &recovered, nullptr);
        std::cout << "Packets recovered: " << recovered << std::endl;
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
            gst_structure_get_int(caps_struct, "width", &video_width);
            gst_structure_get_int(caps_struct, "height", &video_height);

            gst_caps_unref(caps);
        }
    }

    static gboolean handle_key_pressed_cb(GtkEventControllerKey*, guint keyval, guint keycode, GdkModifierType state, gpointer data) {
        ((Lux*) data)->handle_key_pressed(keyval, keycode, state);
        return TRUE;
    }

    void handle_key_pressed(guint keyval, guint keycode, GdkModifierType state) {
        if (ordered_channel->isOpen()) {
            json message = {
                {"type", "keydown"},
                {"key", gdk_to_browser_key(keyval)},
            };
            ordered_channel->send(message.dump());
        }
    }

    static gboolean handle_key_released_cb(GtkEventControllerKey*, guint keyval, guint keycode, GdkModifierType state, gpointer data) {
        ((Lux*) data)->handle_key_released(keyval, keycode, state);
        return TRUE;
    }

    void handle_key_released(guint keyval, guint keycode, GdkModifierType state) {
        if (ordered_channel->isOpen()) {
            json message = {
                {"type", "keyup"},
                {"key", gdk_to_browser_key(keyval)},
            };
            ordered_channel->send(message.dump());
        }
    }

    static gboolean handle_scroll_cb(GtkEventControllerScroll*, gdouble x, gdouble y, gpointer data) {
        ((Lux*) data)->handle_scroll(x, y);
        return TRUE;
    }

    void handle_scroll(int x, int y) {
        if (ordered_channel->isOpen()) {
            json message = {
                {"type", "wheel"},
                {"x", x * 120},
                {"y", y * 120},
            };
            ordered_channel->send(message.dump());
        }
    }

    static void handle_button_pressed_cb(GtkGestureClick* click_gesture, gint, gdouble, gdouble, gpointer data) {
        ((Lux*) data)->handle_button_pressed(click_gesture);
    }

    void handle_button_pressed(GtkGestureClick* click_gesture) {
        if (ordered_channel->isOpen()) {
            json message = {
                {"type", "mousedown"},
                {"button", gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(click_gesture)) - 1},
            };
            ordered_channel->send(message.dump());
        }
    }

    static void handle_button_released_cb(GtkGestureClick* click_gesture, gint, gdouble, gdouble, gpointer data) {
        ((Lux*) data)->handle_button_released(click_gesture);
    }

    void handle_button_released(GtkGestureClick* click_gesture) {
        if (ordered_channel->isOpen()) {
            json message = {
                {"type", "mouseup"},
                {"button", gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(click_gesture)) - 1},
            };
            ordered_channel->send(message.dump());
        }
    }

    static gboolean handle_raw_motion_cb(gpointer data) {
        ((Lux*) data)->handle_raw_motion();
        return TRUE;
    }

    void handle_raw_motion() {
        if (unordered_channel->isOpen() && raw_mouse_manager.mouse_locked && raw_mouse_manager.pending()) {
            RawMouseEvent raw_event = raw_mouse_manager.next_event();
            json message = {
                {"type", "mousemove"},
                {"x", (int) (raw_event.x / 2.5f)},
                {"y", (int) (raw_event.y / 2.5f)},
            };
            unordered_channel->send(message.dump());
        }
    }

    static void handle_focus_change_cb(GObject*, GParamSpec*, gpointer data) {
        ((Lux*) data)->handle_focus_change();
    }

    void handle_focus_change() {
        if (gtk_window_is_active(GTK_WINDOW(window))) {
            raw_mouse_manager.lock_mouse(gtk_native_get_surface(gtk_widget_get_native(window)));
        } else {
            raw_mouse_manager.unlock_mouse(gtk_native_get_surface(gtk_widget_get_native(window)));
        }
    }

    static gboolean handle_motion_cb(GtkEventControllerMotion*, gdouble x, gdouble y, gpointer data) {
        ((Lux*) data)->handle_motion(x, y);
        return TRUE;
    }

    void handle_motion(int x, int y) {
        if (ordered_channel->isOpen()) {
            int translated_x;
            int translated_y;
            position_in_video(x, y, translated_x, translated_y);

            json message = {
                {"type", "mousemoveabs"},
                {"x", translated_x},
                {"y", translated_y},
            };
            ordered_channel->send(message.dump());
        }
    }
};

int main(int argc, char* argv[]) {
    pn::init();
    rtc::InitLogger(rtc::LogLevel::Debug);
    gst_init(&argc, &argv);

    Lux lux;
    glib::Object<GtkApplication> app = gtk_application_new("org.telewindow.lux", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app.get(), "activate", G_CALLBACK(Lux::init_cb), &lux);

    int result = g_application_run(G_APPLICATION(app.get()), argc, argv);
    return result;
}
