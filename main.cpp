#include "Polyweb/polyweb.hpp"
#include "glib.hpp"
#include "json.hpp"
#include "media_receiver.hpp"
#include "setup.hpp"
#include "theme.hpp"
#include "video.hpp"
#include "waiter.hpp"
#include <FL/Fl.H>
#include <FL/fl_ask.H>
#include <SDL2/SDL_main.h>
#include <SDL2/SDL_syswm.h>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>
#include <inttypes.h>
#include <memory>
#include <mutex>
#include <rtc/rtc.hpp>
#include <string.h>
#include <string>
#ifdef _WIN32
    #include <ios>
    #include <stdint.h>
    #include <stdio.h>
    #include <windows.h>
#endif

using nlohmann::json;

int main(int argc, char* argv[]) {
#ifdef _WIN32
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        FILE* fp;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
        freopen_s(&fp, "CONIN$", "r", stdin);
        std::ios::sync_with_stdio();
    }
#endif

    Fl::visual(FL_DOUBLE | FL_INDEX);
    configure_fltk_colors();
    rtc::InitLogger(rtc::LogLevel::Debug);
    pn::init();
    pw::threadpool.resize(0); // The threadpool is only used by Polyweb in server applications
    gst_init(&argc, &argv);

    std::shared_ptr<rtc::PeerConnection> conn;
    std::shared_ptr<rtc::Track> video_track;
    std::shared_ptr<rtc::Track> audio_track;
    std::shared_ptr<rtc::DataChannel> ordered_channel;
    std::shared_ptr<rtc::DataChannel> unordered_channel;

    VideoInfo video_info;
    std::unique_ptr<VideoWindow> video_window;
    glib::Object<GstElement> video_pipeline;
    glib::Object<GstElement> audio_pipeline;

    for (;; conn.reset(),
        video_track.reset(),
        audio_track.reset(),
        ordered_channel.reset(),
        unordered_channel.reset(),
        video_window.reset(),
        video_pipeline.reset(),
        audio_pipeline.reset()) {
        SetupWindow setup_window;
        if (!setup_window.run()) {
            return 0;
        }

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

        video_track->onOpen([&video_track, bitrate = setup_window.bitrate]() {
            video_track->requestBitrate(bitrate * 1000);
        });

        if (!setup_window.view_only) {
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
            {"password", setup_window.password},
            {"show_mouse", setup_window.view_only || !setup_window.client_side_mouse},
            {"offer", pw::base64_encode(offer.data(), offer.size())},
        };

        pw::HTTPResponse resp;
        if (pw::fetch("POST", "https://" + setup_window.address + "/offer", resp, req_json.dump(), {{"Content-Type", "application/json"}}, {.verify_mode = setup_window.verify_certs ? SSL_VERIFY_PEER : SSL_VERIFY_NONE}) == PN_ERROR) {
            fl_alert("Failed to connect: %s", pw::universal_strerror().c_str());
            continue;
        } else if (resp.status_code != 200) {
            fl_alert("Failed to login: Response has status code %" PRIu16, resp.status_code);
            continue;
        }

        std::unique_ptr<rtc::Description> answer;
        try {
            json resp_json = json::parse(resp.body_string());
            json answer_json = json::parse(pw::base64_decode(resp_json["Offer"].get<std::string>()));
            answer = std::make_unique<rtc::Description>(answer_json["sdp"].get<std::string>(), answer_json["type"].get<std::string>());
        } catch (const std::exception& e) {
            fl_alert("Failed to start streaming: Failed to parse server answer: %s", e.what());
            continue;
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

            GstElement* rtph264depay = gst_element_factory_make("rtph264depay", nullptr);

#ifdef _WIN32
            GstElement* h264parse = gst_element_factory_make("h264parse", nullptr);

            GstElement* h264dec = gst_element_factory_make("d3d11h264dec", nullptr);

            GstElement* videosink = gst_element_factory_make("d3d11videosink", nullptr);
#else
            GstElement* h264dec = gst_element_factory_make("avdec_h264", nullptr);
            g_object_set(avdec_h264, "direct-rendering", FALSE, nullptr);

    #ifdef __APPLE__
            GstElement* videosink = gst_element_factory_make("osxvideosink", nullptr);
    #else
            GstElement* videosink = gst_element_factory_make("xvimagesink", nullptr);
    #endif
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

            gst_bin_add_many(GST_BIN(video_pipeline.get()),
                appsrc,
                rtph264depay,
#ifdef _WIN32
                h264parse,
#endif
                h264dec,
                videosink,
                nullptr);
            if (!gst_element_link_many(
                    appsrc,
                    rtph264depay,
#ifdef _WIN32
                    h264parse,
#endif
                    h264dec,
                    videosink,
                    nullptr)) {
                fl_alert("Failed to link GStreamer elements (video pipeline)");
                continue;
            }

            video_window = std::make_unique<VideoWindow>(video_info, videosink, setup_window.client_side_mouse, setup_window.view_only);

            SDL_SysWMinfo info;
            SDL_GetWindowWMInfo(video_window->window, &info);

            gst_video_overlay_handle_events(GST_VIDEO_OVERLAY(videosink), FALSE);
#ifdef _WIN32
            gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(videosink), (uintptr_t) info.info.win.window);
#elif defined(__APPLE__)
            gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(videosink), (uintptr_t) info.info.cocoa.window);
#else
            gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(videosink), info.info.x11.window);
#endif
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
                continue;
            }
        }

        gst_element_set_state(video_pipeline.get(), GST_STATE_PLAYING);
        gst_element_set_state(audio_pipeline.get(), GST_STATE_PLAYING);
        conn->setRemoteDescription(*answer);
        break;
    }

    video_window->run(conn, video_track, ordered_channel, unordered_channel);

    conn->close();
    gst_element_set_state(video_pipeline.get(), GST_STATE_NULL);
    gst_element_set_state(audio_pipeline.get(), GST_STATE_NULL);
    return 0;
}
