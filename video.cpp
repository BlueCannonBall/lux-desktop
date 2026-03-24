// clang-format off
#include "Polyweb/polyweb.hpp"
// clang-format on
#include "video.hpp"
#include "json.hpp"
#include "keys.hpp"
#include "media_receiver.hpp"
#include "ui.hpp"
#include "util.hpp"
#include <FL/fl_ask.H>
#include <FL/fl_draw.H>
#include <FL/x.H>
#include <cmath>
#include <gst/video/videooverlay.h>
#include <inttypes.h>

using nlohmann::json;

int VideoWindow::system_event_handler(void* event, void* data) {
    auto window = (VideoWindow*) data;
    auto parsed_event = window->mouse_manager->parse_event(event);
    if (parsed_event.has_value() && window->unordered_channel->isOpen()) {
        json message = {
            {"type", "mousemove"},
            {"x", (int) std::round(parsed_event->x)},
            {"y", (int) std::round(parsed_event->y)},
        };
        window->unordered_channel->send(message.dump());
        return 1;
    }
    return 0;
}

void VideoWindow::loading_timer_callback(void* data) {
    auto window = (VideoWindow*) data;
    if (!window->connected) {
        ++window->loading_timer_ticks;
        window->redraw();
        Fl::repeat_timeout(1.0 / 60.0, loading_timer_callback, data);
    }
}

VideoWindow::VideoWindow(int x, int y, int width, int height, ConnectionInfo conn_info):
    Fl_Double_Window(x, y, width, height),
    conn_info(std::move(conn_info)) {
    resizable(this);
    end(); // No child widgets!

    rtc::Configuration config;
    config.iceServers.emplace_back("stun.l.google.com:19302");
    config.enableIceTcp = true;
    conn = std::make_shared<rtc::PeerConnection>(config);

    {
        rtc::Description::Video video("video", rtc::Description::Direction::RecvOnly);
        video.addH264Codec(96);
        video_track = conn->addTrack(video);
    }

    {
        rtc::Description::Audio audio("audio", rtc::Description::Direction::RecvOnly);
        audio.addOpusCodec(97);
        audio_track = conn->addTrack(audio);
    }

    video_track->setMediaHandler(std::make_shared<MediaReceiver>());
    audio_track->setMediaHandler(std::make_shared<MediaReceiver>());

    video_track->onOpen([this]() {
        video_track->requestBitrate(this->conn_info.bitrate * 1000);
    });

    file_manager = std::make_unique<FileManager>(ordered_channel = conn->createDataChannel("ordered-input"));
    if (!conn_info.view_only) {
        unordered_channel = conn->createDataChannel("unordered-input",
            {
                .reliability = {
                    .unordered = true,
                },
            });
    }

    cancel_token = std::make_shared<std::atomic<bool>>(false);
    gathering_waiter = std::make_shared<Waiter>();

    conn->onGatheringStateChange([gathering_waiter_ptr = gathering_waiter](rtc::PeerConnection::GatheringState state) {
        if (state == rtc::PeerConnection::GatheringState::Complete) {
            gathering_waiter_ptr->notify_one();
        }
    });
    conn->setLocalDescription();

    auto cancel_token_copy = cancel_token;
    auto gathering_waiter_copy = gathering_waiter;
    auto conn_info_copy = this->conn_info;
    auto conn_copy = conn;

    std::thread([this, cancel_token_copy, gathering_waiter_copy, conn_info_copy, conn_copy]() {
        if (!gathering_waiter_copy->wait_for(std::chrono::seconds(5))) {
            if (*cancel_token_copy) return;
            awake([cancel_token_copy, this]() {
                if (*cancel_token_copy) return;
                connection_error = true;
                fl_alert("Failed to connect: Timed out waiting for ICE gathering to complete");
            });
            return;
        }

        if (*cancel_token_copy) return;

        std::string offer;
        {
            auto description = conn_copy->localDescription();
            if (!description.has_value()) return;
            json offer_json = {
                {"type", description->typeString()},
                {"sdp", std::string(description.value())},
            };
            offer = offer_json.dump();
        }

        json req_json = {
            {"password", conn_info_copy.password},
            {"show_mouse", conn_info_copy.view_only || !conn_info_copy.client_side_mouse},
            {"offer", pw::base64_encode(offer.data(), offer.size())},
        };

        pw::HTTPResponse resp;
        if (pw::fetch("POST",
                "https://" + conn_info_copy.address + "/offer",
                resp,
                req_json.dump(),
                {{"Content-Type", "application/json"}},
                {
                    .send_timeout = std::chrono::seconds(5),
                    .recv_timeout = std::chrono::seconds(5),
                    .verify_mode = conn_info_copy.verify_certs ? SSL_VERIFY_PEER : SSL_VERIFY_NONE,
                }) == PN_ERROR) {
            if (*cancel_token_copy) return;
            awake([cancel_token_copy, this, err = pw::universal_strerror()]() {
                if (*cancel_token_copy) return;
                connection_error = true;
                fl_alert("Failed to connect: %s", err.c_str());
            });
            return;
        } else if (resp.status_code != 200) {
            if (*cancel_token_copy) return;
            awake([cancel_token_copy, this, status_code = resp.status_code]() {
                if (*cancel_token_copy) return;
                connection_error = true;
                fl_alert("Failed to login: Response has status code %" PRIu16, status_code);
            });
            return;
        }

        if (*cancel_token_copy) return;

        std::unique_ptr<rtc::Description> answer;
        try {
            json resp_json = json::parse(resp.body_string());
            json answer_json = json::parse(pw::base64_decode(resp_json["Offer"].get<std::string>()));
            answer = std::make_unique<rtc::Description>(answer_json["sdp"].get<std::string>(), answer_json["type"].get<std::string>());
        } catch (const std::exception& e) {
            if (*cancel_token_copy) return;
            awake([cancel_token_copy, this, err = std::string(e.what())]() {
                if (*cancel_token_copy) return;
                connection_error = true;
                fl_alert("Failed to start streaming: Failed to parse server answer: %s", err.c_str());
            });
            return;
        }

        if (*cancel_token_copy) return;

        std::shared_ptr<rtc::Description> answer_shared = std::move(answer);
        awake([cancel_token_copy, this, answer_shared, conn_copy]() {
            if (*cancel_token_copy) return;
            conn_copy->setRemoteDescription(*answer_shared);
            connected = true;
            redraw();
            if (!this->conn_info.view_only && Fl::belowmouse() == this && Fl::focus()) {
                keyboard_grab_manager->grab_keyboard();
            }
        });
    }).detach();
}

bool VideoWindow::is_connected() const {
    return connected;
}

bool VideoWindow::has_connection_error() const {
    return connection_error;
}

bool VideoWindow::is_playing() const {
    return playing;
}

rtc::PeerConnection::IceState VideoWindow::ice_state() const {
    return conn->iceState();
}

void VideoWindow::show() {
    Fl_Double_Window::show();
    Fl::flush(); // Force the underlying OS window to be realized so that GStreamer can find it

    Fl::add_timeout(1.0 / 60.0, loading_timer_callback, this);

    if (!conn_info.view_only) {
        if (!conn_info.client_side_mouse) {
            mouse_manager = std::make_unique<RawMouseManager>(this);
            Fl::add_system_handler(&VideoWindow::system_event_handler, this);
        }
        keyboard_grab_manager = std::make_unique<KeyboardGrabManager>(top_window());
    }

    video_pipeline = gst_pipeline_new(nullptr);
    {
        GstElement* appsrc = gst_element_factory_make("appsrc", nullptr);
        {
            GstCaps* caps = gst_caps_new_simple("application/x-rtp", "media", G_TYPE_STRING, "video", "encoding-name", G_TYPE_STRING, "H264", "clock-rate", G_TYPE_INT, 90000, nullptr);
            g_object_set(appsrc, "caps", caps, "emit-signals", FALSE, "format", GST_FORMAT_TIME, "is-live", TRUE, "do-timestamp", TRUE, nullptr);
            gst_caps_unref(caps);
        }
        video_track->onMessage([appsrc](rtc::binary message) {
            GstBuffer* buf = gst_buffer_new_and_alloc(message.size());

            GstMapInfo map;
            gst_buffer_map(buf, &map, GST_MAP_WRITE);
            memcpy(map.data, message.data(), message.size());
            gst_buffer_unmap(buf, &map);

            GstFlowReturn flow;
            g_signal_emit_by_name(appsrc, "push-buffer", buf, &flow);
            gst_buffer_unref(buf);
        },
            nullptr);

        GstElement* rtpjitterbuffer = gst_element_factory_make("rtpjitterbuffer", nullptr);
        g_object_set(rtpjitterbuffer, "latency", 0, nullptr);

        GstElement* rtph264depay = gst_element_factory_make("rtph264depay", nullptr);

#ifdef _WIN32
        GstElement* h264parse = gst_element_factory_make("h264parse", nullptr);

        GstElement* h264dec = gst_element_factory_make("d3d11h264dec", nullptr);
#else
        GstElement* h264dec = gst_element_factory_make("avdec_h264", nullptr);
        g_object_set(h264dec, "direct-rendering", FALSE, nullptr);
#endif
        {
            glib::Object<GstPad> pad = gst_element_get_static_pad(h264dec, "src");
            gst_pad_add_probe(pad.get(), GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, [](GstPad* pad, GstPadProbeInfo* info, void* data) {
                auto video_info = (VideoInfo*) data;
                GstEvent* event = GST_PAD_PROBE_INFO_EVENT(info);
                if (GST_EVENT_TYPE(event) == GST_EVENT_CAPS) {
                    GstCaps* caps;
                    gst_event_parse_caps(event, &caps);

                    GstStructure* structure = gst_caps_get_structure(caps, 0);
                    video_info->mutex.lock();
                    gst_structure_get_int(structure, "width", &video_info->width);
                    gst_structure_get_int(structure, "height", &video_info->height);
                    video_info->mutex.unlock();
                }
                return GST_PAD_PROBE_OK;
            },
                &video_info,
                nullptr);
        }

#ifdef _WIN32
        GstElement* videosink = gst_element_factory_make("d3d11videosink", nullptr);
        g_object_set(videosink, "enable-navigation-events", FALSE, nullptr);
#elif defined(__APPLE__)
        GstElement* videosink = gst_element_factory_make("osxvideosink", nullptr);
#else
        GstElement* videosink = gst_element_factory_make("xvimagesink", nullptr);
#endif
        g_object_set(videosink, "max-lateness", 0, nullptr);

        gst_bin_add_many(GST_BIN(video_pipeline.get()),
            appsrc,
            rtpjitterbuffer,
            rtph264depay,
#ifdef _WIN32
            h264parse,
#endif
            h264dec,
            videosink,
            nullptr);
        if (!gst_element_link_many(
                appsrc,
                rtpjitterbuffer,
                rtph264depay,
#ifdef _WIN32
                h264parse,
#endif
                h264dec,
                videosink,
                nullptr)) {
            fl_alert("Failed to link GStreamer elements (video pipeline)");
            return;
        }

        gst_video_overlay_handle_events(GST_VIDEO_OVERLAY(videosink), FALSE);
#ifdef _WIN32
        gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(videosink), (uintptr_t) fl_xid(this));
#elif defined(__APPLE__)
        gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(videosink), (uintptr_t) fl_xid(this));
#else
        gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(videosink), fl_xid(this));
#endif
        overlay = GST_VIDEO_OVERLAY(videosink);
    }

    audio_pipeline = gst_pipeline_new(nullptr);
    {
        GstElement* appsrc = gst_element_factory_make("appsrc", nullptr);
        {
            GstCaps* caps = gst_caps_new_simple("application/x-rtp", "media", G_TYPE_STRING, "audio", "encoding-name", G_TYPE_STRING, "OPUS", "clock-rate", G_TYPE_INT, 48000, "payload", G_TYPE_INT, 97, nullptr);
            g_object_set(appsrc, "caps", caps, "format", GST_FORMAT_TIME, "is-live", TRUE, "do-timestamp", TRUE, nullptr);
            gst_caps_unref(caps);
        }
        audio_track->onMessage([appsrc](rtc::binary message) {
            GstBuffer* buf = gst_buffer_new_and_alloc(message.size());

            GstMapInfo map;
            gst_buffer_map(buf, &map, GST_MAP_WRITE);
            memcpy(map.data, message.data(), message.size());
            gst_buffer_unmap(buf, &map);

            GstFlowReturn flow;
            g_signal_emit_by_name(appsrc, "push-buffer", buf, &flow);
            gst_buffer_unref(buf);
        },
            nullptr);

        GstElement* rtpopusdepay = gst_element_factory_make("rtpopusdepay", nullptr);

        GstElement* capsfilter = gst_element_factory_make("capsfilter", nullptr);
        {
            GstCaps* caps = gst_caps_new_simple("audio/x-opus", "channels", G_TYPE_INT, 2, nullptr);
            g_object_set(capsfilter, "caps", caps, nullptr);
            gst_caps_unref(caps);
        }

        GstElement* opusdec = gst_element_factory_make("opusdec", nullptr);

        GstElement* audioconvert = gst_element_factory_make("audioconvert", nullptr);

        GstElement* autoaudiosink = gst_element_factory_make("autoaudiosink", nullptr);

        gst_bin_add_many(GST_BIN(audio_pipeline.get()),
            appsrc,
            rtpopusdepay,
            capsfilter,
            opusdec,
            audioconvert,
            autoaudiosink,
            nullptr);
        if (!gst_element_link_many(appsrc,
                rtpopusdepay,
                capsfilter,
                opusdec,
                audioconvert,
                autoaudiosink,
                nullptr)) {
            fl_alert("Failed to link GStreamer elements (audio pipeline)");
            return;
        }
    }

    gst_element_set_state(video_pipeline.get(), GST_STATE_PLAYING);
    gst_element_set_state(audio_pipeline.get(), GST_STATE_PLAYING);
    playing = true;

    take_focus();
}

void VideoWindow::hide() {
    Fl::remove_timeout(loading_timer_callback, this);

    if (cancel_token) {
        *cancel_token = true;
        cancel_token.reset();
    }
    if (gathering_waiter) {
        gathering_waiter->notify_all();
        gathering_waiter.reset();
    }

    if (!conn_info.view_only) {
        if (!conn_info.client_side_mouse) {
            Fl::remove_system_handler(&VideoWindow::system_event_handler);
            mouse_manager.reset();
        }
        keyboard_grab_manager.reset();
    }
    file_manager.reset();
    if (ordered_channel->isOpen()) {
        json message = {
            {"type", "disconnect"},
        };
        ordered_channel->send(message.dump());
    }
    conn->close();
    connected = false;

    overlay = nullptr;
    if (video_pipeline) {
        gst_element_set_state(video_pipeline.get(), GST_STATE_NULL);
        video_pipeline.reset();
    }
    if (audio_pipeline) {
        gst_element_set_state(audio_pipeline.get(), GST_STATE_NULL);
        audio_pipeline.reset();
    }
    playing = false;

    Fl_Double_Window::hide();
}

void VideoWindow::draw() {
    if (!connected) {
        Fl_Double_Window::draw();
        fl_font(labelfont(), 18);
        fl_color(fl_color_average(labelcolor(), color(), 0.65f + 0.35f * std::sin(loading_timer_ticks * (3.14159265f / 60.0f))));
        fl_draw("Connecting...", 0, 0, w(), h(), FL_ALIGN_CENTER);
    } else if (overlay) {
        gst_video_overlay_expose(GST_VIDEO_OVERLAY(overlay));
    }
}

void VideoWindow::flush() {
    if (connected) {
        Fl_Window::flush();
    } else {
        Fl_Double_Window::flush();
    }
}

int VideoWindow::handle(int event) {
    if (connected) {
        switch (event) {
        case FL_PUSH:
            take_focus();
            if (!conn_info.view_only) {
                if (!conn_info.client_side_mouse && !mouse_manager->mouse_locked) {
                    mouse_manager->lock_mouse();
                    return 1;
                } else if (ordered_channel->isOpen()) {
                    json message = {
                        {"type", "mousedown"},
                        {"button", Fl::event_button() - 1},
                    };
                    ordered_channel->send(message.dump());
                    return 1;
                }
            }
            break;

        case FL_RELEASE:
            if (!conn_info.view_only && ordered_channel->isOpen()) {
                json message = {
                    {"type", "mouseup"},
                    {"button", Fl::event_button() - 1},
                };
                ordered_channel->send(message.dump());
                return 1;
            }
            break;

        case FL_MOVE:
        case FL_DRAG:
            if (!conn_info.view_only) {
                if (conn_info.client_side_mouse) {
                    if (ordered_channel->isOpen()) {
                        int x;
                        int y;
                        position_in_video(Fl::event_x(), Fl::event_y(), x, y);

                        json message = {
                            {"type", "mousemoveabs"},
                            {"x", x},
                            {"y", y},
                        };
                        ordered_channel->send(message.dump());
                        return 1;
                    }
                } else {
                    return 1; // This is handled by the RawMouseManager
                }
            }
            break;

        case FL_MOUSEWHEEL:
            if (!conn_info.view_only && unordered_channel->isOpen()) {
                json message = {
                    {"type", "wheel"},
                    {"x", (int) std::round(Fl::event_dx() * 120.0)},
                    {"y", (int) std::round(Fl::event_dy() * 120.0)},
                };
                unordered_channel->send(message.dump());
                return 1;
            }
            break;

        case FL_KEYUP:
            if (!conn_info.view_only) {
                if (!conn_info.client_side_mouse && Fl::event_key() == FL_F + 9) {
                    if (mouse_manager->mouse_locked) {
                        mouse_manager->unlock_mouse();
                    } else {
                        mouse_manager->lock_mouse();
                    }
                    return 1;
                } else if (!is_key_global_shortcut(Fl::event_key()) && ordered_channel->isOpen()) {
                    json message = {
                        {"type", "keyup"},
                        {"key", fltk_to_browser_key(Fl::event_key())},
                    };
                    ordered_channel->send(message.dump());
                    return 1;
                }
            }
            break;

        case FL_KEYDOWN:
            if (!conn_info.view_only && !is_key_global_shortcut(Fl::event_key()) && ordered_channel->isOpen()) {
                json message = {
                    {"type", "keydown"},
                    {"key", fltk_to_browser_key(Fl::event_key())},
                };
                ordered_channel->send(message.dump());
                return 1;
            }
            break;

        case FL_FOCUS:
        case FL_UNFOCUS:
            if (!conn_info.view_only) {
                return 1;
            }
            break;

        case FL_ENTER:
            if (!conn_info.view_only) {
                if (Fl::focus()) {
                    keyboard_grab_manager->grab_keyboard();
                }
                return 1;
            }
            break;

        case FL_LEAVE:
            if (!conn_info.view_only) {
                keyboard_grab_manager->ungrab_keyboard();
                return 1;
            }
            break;
        }
    }
    return Fl_Double_Window::handle(event);
}

void VideoWindow::position_in_video(int x, int y, int& x_ret, int& y_ret) {
    double cw = w();
    double ch = h();

    std::lock_guard<std::mutex> lock(video_info.mutex);
    double vw = video_info.width;
    double vh = video_info.height;

    if (vw * ch > cw * vh) {
        x_ret = x * vw / cw;
        y_ret = (y - ch / 2.0) * vw / cw + vh / 2.0;
    } else {
        x_ret = (x - cw / 2.0) * vh / ch + vw / 2.0;
        y_ret = y * vh / ch;
    }
}

unsigned int VideoWindow::get_bitrate() const {
    return conn_info.bitrate;
}

void VideoWindow::set_bitrate(unsigned int bitrate) {
    video_track->requestBitrate((conn_info.bitrate = bitrate) * 1000);
}

void VideoWindow::request_keyframe() {
    video_track->requestKeyframe();
}

void VideoWindow::release_all_keys() {
    if (!conn_info.view_only) {
        json message = {
            {"type", "releaseall"},
        };
        ordered_channel->send(message.dump());
    }
}
