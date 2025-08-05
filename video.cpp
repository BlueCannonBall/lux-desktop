#include "video.hpp"
#include "Polyweb/polyweb.hpp"
#include "json.hpp"
#include "keys.hpp"
#include "media_receiver.hpp"
#include "waiter.hpp"
#include <FL/fl_ask.H>
#include <FL/x.H>
#include <gst/video/videooverlay.h>

using nlohmann::json;

VideoWindow::VideoWindow(int x, int y, int width, int height, ConnectionInfo conn_info):
    Fl_Window(x, y, width, height),
    conn_info(std::move(conn_info)) {
    rtc::Configuration config;
    config.iceServers.emplace_back("stun.l.google.com:19302");
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

    if (!this->conn_info.view_only) {
        ordered_channel = conn->createDataChannel("ordered-input");
        unordered_channel = conn->createDataChannel("unordered-input",
            {
                .reliability = {
                    .unordered = true,
                },
            });
    }

    {
        Waiter waiter;
        conn->onGatheringStateChange([&waiter](rtc::PeerConnection::GatheringState state) {
            if (state == rtc::PeerConnection::GatheringState::Complete) {
                waiter.notify_one();
            }
        });
        conn->setLocalDescription();
        waiter.wait();
    }

    std::string offer;
    {
        auto description = conn->localDescription();
        json offer_json = {
            {"type", description->typeString()},
            {"sdp", std::string(description.value())},
        };
        offer = offer_json.dump();
    }

    json req_json = {
        {"password", this->conn_info.password},
        {"show_mouse", this->conn_info.view_only || !this->conn_info.client_side_mouse},
        {"offer", pw::base64_encode(offer.data(), offer.size())},
    };

    pw::HTTPResponse resp;
    if (pw::fetch("POST", "https://" + this->conn_info.address + "/offer", resp, req_json.dump(), {{"Content-Type", "application/json"}}, {.verify_mode = this->conn_info.verify_certs ? SSL_VERIFY_PEER : SSL_VERIFY_NONE}) == PN_ERROR) {
        fl_alert("Failed to connect: %s", pw::universal_strerror().c_str());
        return;
    } else if (resp.status_code != 200) {
        fl_alert("Failed to login: Response has status code %" PRIu16, resp.status_code);
        return;
    }

    std::unique_ptr<rtc::Description> answer;
    try {
        json resp_json = json::parse(resp.body_string());
        json answer_json = json::parse(pw::base64_decode(resp_json["Offer"].get<std::string>()));
        answer = std::make_unique<rtc::Description>(answer_json["sdp"].get<std::string>(), answer_json["type"].get<std::string>());
    } catch (const std::exception& e) {
        fl_alert("Failed to start streaming: Failed to parse server answer: %s", e.what());
        return;
    }
    conn->setRemoteDescription(*answer);
    connected = true;
}

bool VideoWindow::is_connected() const {
    return connected;
}

bool VideoWindow::is_playing() const {
    return playing;
}

void VideoWindow::show() {
    Fl_Window::show();
    Fl::flush(); // Force the OS window to be realized

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

        GstElement* rtph264depay = gst_element_factory_make("rtph264depay", nullptr);

        GstElement* h264dec = gst_element_factory_make("avdec_h264", nullptr);
        g_object_set(h264dec, "direct-rendering", FALSE, nullptr);
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
        GstElement* videosink;
        if (!(video_sink = gst_element_factory_make("d3d12videosink", nullptr))) {
            videosink = gst_element_factory_make("d3d11videosink", nullptr);
        }
#elif defined(__APPLE__)
        GstElement* videosink = gst_element_factory_make("osxvideosink", nullptr);
#else
        GstElement* videosink = gst_element_factory_make("xvimagesink", nullptr);
#endif
        g_object_set(videosink, "sync", FALSE, nullptr);

        gst_bin_add_many(GST_BIN(video_pipeline.get()),
            appsrc,
            rtph264depay,
            h264dec,
            videosink,
            nullptr);
        if (!gst_element_link_many(
                appsrc,
                rtph264depay,
                h264dec,
                videosink,
                nullptr)) {
            fl_alert("Failed to link GStreamer elements (video pipeline)");
            return;
        }

        gst_video_overlay_handle_events(GST_VIDEO_OVERLAY(videosink), FALSE);
#ifdef _WIN32
        set_window_dark_mode(info.info.win.window);
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
    conn->close();
    gst_element_set_state(video_pipeline.get(), GST_STATE_NULL);
    gst_element_set_state(audio_pipeline.get(), GST_STATE_NULL);
    Fl_Window::hide();
}

void VideoWindow::draw() {
    if (overlay) {
        gst_video_overlay_expose(GST_VIDEO_OVERLAY(overlay));
    }
}

int VideoWindow::handle(int event) {
    switch (event) {
    case FL_PUSH:
        take_focus();
        if (!conn_info.view_only && ordered_channel->isOpen()) {
            json message = {
                {"type", "mousedown"},
                {"button", Fl::event_button() - 1},
            };
            ordered_channel->send(message.dump());
            return 1;
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
                if (unordered_channel->isOpen()) {
                    // json message = {
                    //     {"type", "mousemove"},
                    //     {"x", event.motion.xrel},
                    //     {"y", event.motion.yrel},
                    // };
                    // unordered_channel->send(message.dump());
                }
            }
        }
        break;

    case FL_MOUSEWHEEL:
        if (!conn_info.view_only && unordered_channel->isOpen()) {
            json message = {
                {"type", "wheel"},
                {"x", (int) roundf(Fl::event_dx() * 120.f)},
                {"y", (int) roundf(Fl::event_dy() * 120.f)},
            };
            unordered_channel->send(message.dump());
            return 1;
        }
        break;

    case FL_KEYUP:
        if (!conn_info.view_only && ordered_channel->isOpen()) {
            json message = {
                {"type", "keyup"},
                {"key", fltk_to_browser_key(Fl::event_key())},
            };
            ordered_channel->send(message.dump());
            return 1;
        }
        break;

    case FL_KEYDOWN:
        if (!conn_info.view_only && ordered_channel->isOpen()) {
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
    }
    return Fl_Window::handle(event);
}

void VideoWindow::position_in_video(int x, int y, int& x_ret, int& y_ret) {
    int window_width = w();
    int window_height = h();

    std::lock_guard<std::mutex> lock(video_info.mutex);
    float video_aspect_ratio = (float) video_info.width / video_info.height;
    float window_aspect_ratio = (float) window_width / window_height;
    if (video_aspect_ratio > window_aspect_ratio) {
        x_ret = x / ((float) window_width / video_info.width);
        y_ret = (y - ((1.f - window_aspect_ratio / video_aspect_ratio) * window_height / 2.f)) / ((float) window_width / video_info.width);
    } else if (video_aspect_ratio < window_aspect_ratio) {
        x_ret = (x - ((1.f - video_aspect_ratio / window_aspect_ratio) * window_width / 2.f)) / ((float) window_height / video_info.height);
        y_ret = y / ((float) window_height / video_info.height);
    } else {
        x_ret = x / ((float) window_width / video_info.width);
        y_ret = y / ((float) window_height / video_info.height);
    }
}

// void VideoWindow::set_keyboard_grab(bool grabbed) {
//     if (grabbed) {
// #if !defined(_WIN32) && !defined(__APPLE__)
//         if (is_kde() && system("qdbus org.kde.kglobalaccel /kglobalaccel blockGlobalShortcuts true") != 0) {
//             std::cerr << "Warning: Qt D-Bus call failed (ignore unless on KDE)" << std::endl;
//         }
// #endif
//         SDL_SetWindowKeyboardGrab(window, SDL_TRUE);
//     } else {
// #if !defined(_WIN32) && !defined(__APPLE__)
//         if (is_kde() && system("qdbus org.kde.kglobalaccel /kglobalaccel blockGlobalShortcuts false") != 0) {
//             std::cerr << "Warning: Qt D-Bus call failed (ignore unless on KDE)" << std::endl;
//         }
// #endif
//         SDL_SetWindowKeyboardGrab(window, SDL_FALSE);
//     }
// }

// void VideoWindow::run(std::shared_ptr<rtc::PeerConnection> conn, std::shared_ptr<rtc::Track> track, std::shared_ptr<rtc::DataChannel> ordered_channel, std::shared_ptr<rtc::DataChannel> unordered_channel) {
//     for (;;) {
//         if (conn->iceState() == rtc::PeerConnection::IceState::Closed ||
//             conn->iceState() == rtc::PeerConnection::IceState::Disconnected ||
//             conn->iceState() == rtc::PeerConnection::IceState::Failed) {
//             SDL_SetWindowFullscreen(window, 0);
//             if (!view_only) {
//                 SDL_SetRelativeMouseMode(SDL_FALSE);
//                 set_keyboard_grab(false);
//             }
//             fl_alert("The connection has closed.");
//             break;
//         }

//         SDL_Event event;
//         if (SDL_WaitEvent(&event)) {
//             do {
//                 switch (event.type) {
//                 case SDL_QUIT:
//                     gst_element_set_state(overlay, GST_STATE_NULL);
//                     return;

//                 case SDL_WINDOWEVENT:
//                     switch (event.window.event) {
//                     case SDL_WINDOWEVENT_EXPOSED:
//                         gst_video_overlay_expose(GST_VIDEO_OVERLAY(overlay));
//                         break;

//                     case SDL_WINDOWEVENT_FOCUS_LOST:
//                         if (!view_only) {
//                             set_keyboard_grab(false);
//                         }
//                         break;

//                     case SDL_WINDOWEVENT_FOCUS_GAINED:
//                         if (!view_only) {
//                             set_keyboard_grab(true);
//                         }
//                         break;
//                     }
//                     break;

//                 case SDL_KEYDOWN:
//                     if (!view_only &&
//                         event.key.keysym.sym != SDLK_F5 &&
//                         event.key.keysym.sym != SDLK_F9 &&
//                         event.key.keysym.sym != SDLK_F11 &&
//                         ordered_channel->isOpen()) {
//                         json message = {
//                             {"type", "keydown"},
//                             {"key", sdl_to_browser_key(event.key.keysym.sym)},
//                         };
//                         ordered_channel->send(message.dump());
//                     }
//                     break;

//                 case SDL_KEYUP:
//                     if (event.key.keysym.sym == SDLK_F5) {
//                         track->requestKeyframe();
//                     } else if (event.key.keysym.sym == SDLK_F11) {
//                         Uint32 flags = SDL_GetWindowFlags(window);
//                         if (flags & SDL_WINDOW_FULLSCREEN) {
//                             SDL_SetWindowFullscreen(window, 0);
//                         } else {
//                             SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
//                         }
//                     } else if (!view_only) {
//                         if (event.key.keysym.sym == SDLK_F9) {
//                             if (!client_side_mouse) {
//                                 SDL_SetRelativeMouseMode(SDL_GetRelativeMouseMode() ? SDL_FALSE : SDL_TRUE);
//                             }
//                         } else if (ordered_channel->isOpen()) {
//                             json message = {
//                                 {"type", "keyup"},
//                                 {"key", sdl_to_browser_key(event.key.keysym.sym)},
//                             };
//                             ordered_channel->send(message.dump());
//                         }
//                     }
//                     break;

//                 case SDL_MOUSEMOTION:
//                     if (!view_only) {
//                         if (client_side_mouse) {
//                             if (ordered_channel->isOpen()) {
//                                 int x;
//                                 int y;
//                                 position_in_video(event.motion.x, event.motion.y, x, y);

//                                 json message = {
//                                     {"type", "mousemoveabs"},
//                                     {"x", x},
//                                     {"y", y},
//                                 };
//                                 ordered_channel->send(message.dump());
//                             }
//                         } else {
//                             if (unordered_channel->isOpen()) {
//                                 json message = {
//                                     {"type", "mousemove"},
//                                     {"x", event.motion.xrel},
//                                     {"y", event.motion.yrel},
//                                 };
//                                 unordered_channel->send(message.dump());
//                             }
//                         }
//                     }
//                     break;

//                 case SDL_MOUSEBUTTONDOWN:
//                     if (!view_only) {
//                         if (!client_side_mouse && !SDL_GetRelativeMouseMode()) {
//                             SDL_SetRelativeMouseMode(SDL_TRUE);
//                         } else if (ordered_channel->isOpen()) {
//                             json message = {
//                                 {"type", "mousedown"},
//                                 {"button", event.button.button - 1},
//                             };
//                             ordered_channel->send(message.dump());
//                         }
//                     }
//                     break;

//                 case SDL_MOUSEBUTTONUP:
//                     if (!view_only && ordered_channel->isOpen()) {
//                         json message = {
//                             {"type", "mouseup"},
//                             {"button", event.button.button - 1},
//                         };
//                         ordered_channel->send(message.dump());
//                     }
//                     break;

//                 case SDL_MOUSEWHEEL:
//                     if (!view_only && unordered_channel->isOpen()) {
//                         json message = {
//                             {"type", "wheel"},
//                             {"x", (int) roundf(event.wheel.preciseX * 120.f)},
//                             {"y", (int) roundf(event.wheel.preciseY * -120.f)},
//                         };
//                         unordered_channel->send(message.dump());
//                     }
//                     break;
//                 }
//             } while (SDL_PollEvent(&event));
//         }
//     }
// }
