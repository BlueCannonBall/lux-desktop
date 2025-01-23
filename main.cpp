#include "Polyweb/polyweb.hpp"
#include "bandwidth.hpp"
#include "glib.hpp"
#include "json.hpp"
#include "setup.hpp"
#include "video.hpp"
#include "waiter.hpp"
#include <FL/Fl.H>
#include <FL/fl_ask.H>
#include <chrono>
#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <inttypes.h>
#include <math.h>
#include <memory>
#include <rtc/rtc.hpp>
#include <string.h>
#include <string>
#include <thread>

using nlohmann::json;

int main(int argc, char* argv[]) {
    Fl::scheme("gtk+");
    rtc::InitLogger(rtc::LogLevel::Warning);
    pn::init();
    pw::threadpool.resize(0); // The Polyweb threadpool is only used by Polyweb in server applications
    gst_init(&argc, &argv);

    bool client_side_mouse;
    BandwidthEstimator bandwidth_estimator;
    pw::ClientConfig client_config;

    std::shared_ptr<rtc::PeerConnection> conn;
    std::shared_ptr<rtc::Track> video_track;
    std::shared_ptr<rtc::Track> audio_track;
    std::shared_ptr<rtc::DataChannel> ordered_channel;
    std::shared_ptr<rtc::DataChannel> unordered_channel;

    glib::Object<GstElement> video_pipeline;
    glib::Object<GstElement> audio_pipeline;
    Video video;

    for (;; conn.reset(),
        video_track.reset(),
        ordered_channel.reset(),
        unordered_channel.reset(),
        video_pipeline.reset(),
        audio_pipeline.reset()) {
        SetupWindow setup_window;
        if (!setup_window.run()) {
            return 0;
        }
        client_side_mouse = setup_window.client_side_mouse;
        bandwidth_estimator = BandwidthEstimator(setup_window.bitrate);
        client_config.verify_mode = setup_window.verify_certs ? SSL_VERIFY_PEER : SSL_VERIFY_NONE;

        rtc::Configuration config;
        config.iceServers.emplace_back("stun.l.google.com:19302");
        conn = std::make_shared<rtc::PeerConnection>(config);

        {
            rtc::Description::Video video("video", rtc::Description::Direction::RecvOnly);
            video.addH264Codec(96);
            video_track = conn->addTrack(video);

            rtc::Description::Audio audio("audio", rtc::Description::Direction::RecvOnly);
            audio.addOpusCodec(97);
            audio_track = conn->addTrack(audio);
        }

        {
            auto video_session = std::make_shared<rtc::RtcpReceivingSession>();
            auto audio_session = std::make_shared<rtc::RtcpReceivingSession>();
            video_track->setRtcpHandler(video_session);
            audio_track->setRtcpHandler(audio_session);
        }

        ordered_channel = conn->createDataChannel("ordered-input");
        unordered_channel = conn->createDataChannel("unordered-input",
            {
                .reliability = {
                    .unordered = true,
                },
            });

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
            {"show_mouse", !client_side_mouse},
            {"offer", pw::base64_encode(offer.data(), offer.size())},
        };

        pw::HTTPResponse resp;
        if (pw::fetch("POST", "https://" + setup_window.address + "/offer", resp, req_json.dump(), {{"Content-Type", "application/json"}}, client_config) == PN_ERROR) {
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
                g_object_set(appsrc, "caps", caps, "format", GST_FORMAT_TIME, "is-live", TRUE, "do-timestamp", TRUE, nullptr);
                gst_caps_unref(caps);
            }
            video_track->onMessage([appsrc, &bandwidth_estimator](rtc::binary message) {
                if (message.size() >= sizeof(rtc::RtpHeader)) {
                    rtc::RtpHeader rtp_header;
                    memcpy(&rtp_header, message.data(), sizeof rtp_header);
                    bandwidth_estimator.update(message.size(), rtp_header.seqNumber());
                }

                if (appsrc) {
                    GstBuffer* buf = gst_buffer_new_and_alloc(message.size());

                    GstMapInfo map;
                    gst_buffer_map(buf, &map, GST_MAP_WRITE);
                    memcpy(map.data, message.data(), message.size());
                    gst_buffer_unmap(buf, &map);

                    GstFlowReturn flow;
                    g_signal_emit_by_name(appsrc, "push-buffer", buf, &flow);
                    gst_buffer_unref(buf);
                }
            },
                nullptr);

            GstElement* rtph264depay = gst_element_factory_make("rtph264depay", nullptr);

            GstElement* avdec_h264 = gst_element_factory_make("avdec_h264", nullptr);
            {
                glib::Object<GstPad> pad = gst_element_get_static_pad(avdec_h264, "src");
                gst_pad_add_probe(pad.get(), GST_PAD_PROBE_TYPE_EVENT_BOTH, [](GstPad* pad, GstPadProbeInfo* info, gpointer data) {
                    auto video = (Video*) data;
                    GstEvent* event = GST_PAD_PROBE_INFO_EVENT(info);
                    if (GST_EVENT_TYPE(event) == GST_EVENT_CAPS) {
                        GstCaps* caps = gst_caps_new_any();
                        gst_event_parse_caps(event, &caps);

                        GstStructure* caps_struct = gst_caps_get_structure(caps, 0);
                        video->mutex.lock();
                        gst_structure_get_int(caps_struct, "width", &video->width);
                        gst_structure_get_int(caps_struct, "height", &video->height);
                        video->set_sample(nullptr);
                        video->resized = true;
                        video->mutex.unlock();
                        video->cv.notify_one();

                        gst_caps_unref(caps);
                    }
                    return GST_PAD_PROBE_OK;
                },
                    &video,
                    nullptr);
            }

            GstElement* videoconvert = gst_element_factory_make("videoconvert", nullptr);

            GstElement* appsink = gst_element_factory_make("appsink", nullptr);
            {
                GstCaps* caps = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "RGB", nullptr);
                g_object_set(appsink, "caps", caps, "emit-signals", TRUE, nullptr);
                gst_caps_unref(caps);
            }
            glib::connect_signal(appsink, "new-sample", [&video](GstElement* appsink) {
                GstSample* sample = gst_app_sink_pull_sample(GST_APP_SINK(appsink));
                std::lock_guard<std::mutex> lock(video.mutex);
                video.set_sample(sample);
                return GST_FLOW_OK;
            });

            gst_bin_add_many(GST_BIN(video_pipeline.get()),
                appsrc,
                rtph264depay,
                avdec_h264,
                videoconvert,
                appsink,
                nullptr);
            if (!gst_element_link_many(appsrc,
                    rtph264depay,
                    avdec_h264,
                    videoconvert,
                    appsink,
                    nullptr)) {
                fl_alert("Failed to link GStreamer elements (video pipeline)");
                continue;
            }
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
                if (appsrc) {
                    GstBuffer* buf = gst_buffer_new_and_alloc(message.size());

                    GstMapInfo map;
                    gst_buffer_map(buf, &map, GST_MAP_WRITE);
                    memcpy(map.data, message.data(), message.size());
                    gst_buffer_unmap(buf, &map);

                    GstFlowReturn flow;
                    g_signal_emit_by_name(appsrc, "push-buffer", buf, &flow);
                    gst_buffer_unref(buf);
                }
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

    // Wait for video resolution to be available
    {
        std::unique_lock<std::mutex> lock(video.mutex);
        video.cv.wait(lock);
        lock.unlock();
    }

    std::thread([conn, video_track, unordered_channel, &bandwidth_estimator]() {
        while (conn->iceState() != rtc::PeerConnection::IceState::Closed &&
               conn->iceState() != rtc::PeerConnection::IceState::Disconnected &&
               conn->iceState() != rtc::PeerConnection::IceState::Failed) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            video_track->requestBitrate(bandwidth_estimator.estimate(0.5) * 1000);
        }
    }).detach();

    VideoWindow video_window(video, client_side_mouse);
    video_window.run(conn, ordered_channel, unordered_channel);
    return 0;
}
