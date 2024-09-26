#include "Polyweb/polyweb.hpp"
#include "glib.hpp"
#include "json.hpp"
#include "setup.hpp"
#include "video.hpp"
#include <FL/Fl.H>
#include <FL/fl_ask.H>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <inttypes.h>
#include <iostream>
#include <math.h>
#include <memory>
#include <mutex>
#include <rtc/rtc.hpp>
#include <string.h>
#include <string>
#include <thread>

using nlohmann::json;

struct VideoInfo {
    std::mutex mutex;
    std::condition_variable cv;
    int width;
    int height;
    GstSample* sample = nullptr;

    ~VideoInfo() {
        if (sample) {
            gst_sample_unref(sample);
            sample = nullptr;
        }
    }

    void set_sample(GstSample* sample) {
        if (this->sample != sample) {
            if (this->sample) gst_sample_unref(this->sample);
            this->sample = sample;
        }
    }
};

int main(int argc, char* argv[]) {
    Fl::visual(FL_DOUBLE | FL_INDEX);
    rtc::InitLogger(rtc::LogLevel::Warning);
    pn::init();
    gst_init(&argc, &argv);

    float bitrate;
    bool client_side_mouse;

    std::shared_ptr<rtc::PeerConnection> conn;
    std::shared_ptr<rtc::Track> track;
    std::shared_ptr<rtc::DataChannel> ordered_channel;
    std::shared_ptr<rtc::DataChannel> unordered_channel;

    glib::Object<GstElement> pipeline;
    VideoInfo video_info;

    for (;; conn.reset(),
         track.reset(),
         ordered_channel.reset(),
         unordered_channel.reset(),
         pipeline.reset()) {
        SetupWindow setup_window;
        if (!setup_window.run()) {
            return 0;
        }
        bitrate = setup_window.bitrate;
        client_side_mouse = setup_window.client_side_mouse;

        rtc::Configuration config;
        config.iceServers.emplace_back("stun.l.google.com:19302");
        conn = std::make_shared<rtc::PeerConnection>(config);

        rtc::Description::Video media("video", rtc::Description::Direction::RecvOnly);
        media.addH264Codec(96);

        auto session = std::make_shared<rtc::RtcpReceivingSession>();
        track = conn->addTrack(media);
        track->setRtcpHandler(session);

        ordered_channel = conn->createDataChannel("ordered-input");
        unordered_channel = conn->createDataChannel("unordered-input",
            {
                .reliability = {
                    .unordered = true,
                },
            });

        conn->onStateChange([track, bitrate](rtc::PeerConnection::State state) {
            std::cout << "State: " << state << std::endl;
            if (state == rtc::PeerConnection::State::Connected) {
                track->requestBitrate(bitrate * 1000);
            }
        });

        std::mutex gathering_mutex;
        std::condition_variable gathering_cv;
        conn->onGatheringStateChange([&gathering_cv](rtc::PeerConnection::GatheringState state) {
            std::cout << "Gathering state: " << state << std::endl;
            if (state == rtc::PeerConnection::GatheringState::Complete) {
                gathering_cv.notify_one();
            }
        });

        conn->setLocalDescription();

        std::unique_lock<std::mutex> gathering_lock(gathering_mutex);
        gathering_cv.wait(gathering_lock);
        gathering_lock.unlock();

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
            {"password", setup_window.password},
            {"show_mouse", !client_side_mouse},
            {"offer", pw::base64_encode(offer.data(), offer.size())},
        };
        std::cout << "Sending offer: " << req_body_json << std::endl;

        pw::HTTPResponse resp;
        if (pw::fetch("POST", "https://" + setup_window.address + "/offer", resp, req_body_json.dump(), {{"Content-Type", "application/json"}}) == PN_ERROR) {
            fl_alert("Failed to connect: %s", pw::universal_strerror().c_str());
            continue;
        } else if (resp.status_code != 200) {
            fl_alert("Failed to login: Response has status code %" PRIu16, resp.status_code);
            continue;
        }

        std::unique_ptr<rtc::Description> answer;
        try {
            json answer_json = json::parse(resp.body_string());
            auto sdp_data = pw::base64_decode(answer_json["Offer"].get<std::string>());
            answer_json = json::parse(sdp_data);
            answer = std::make_unique<rtc::Description>(answer_json["sdp"].get<std::string>(), answer_json["type"].get<std::string>());
        } catch (const std::exception& e) {
            fl_alert("Failed to start streaming: Failed to parse server answer: %s", e.what());
            continue;
        }

        pipeline = gst_pipeline_new(nullptr);

        GstElement* appsrc = gst_element_factory_make("appsrc", nullptr);
        {
            GstCaps* caps = gst_caps_new_simple("application/x-rtp", "media", G_TYPE_STRING, "video", "encoding-name", G_TYPE_STRING, "H264", "clock-rate", G_TYPE_INT, 90000, nullptr);
            g_object_set(appsrc, "caps", caps, "format", GST_FORMAT_TIME, "is-live", TRUE, "do-timestamp", TRUE, nullptr);
            gst_caps_unref(caps);
        }
        track->onMessage([appsrc](rtc::binary message) {
            if (appsrc) {
                GstBuffer* buf = gst_buffer_new_and_alloc(message.size());

                GstMapInfo map;
                gst_buffer_map(buf, &map, GST_MAP_WRITE);
                memcpy(map.data, message.data(), message.size());
                gst_buffer_unmap(buf, &map);

                GstFlowReturn flow;
                g_signal_emit_by_name(appsrc, "push-buffer", buf, &flow);
                gst_buffer_unref(buf);
                if (flow != GST_FLOW_OK) {
                    std::cerr << "Warning: Flow is " << flow << std::endl;
                }
            }
        },
            nullptr);

        GstElement* rtph264depay = gst_element_factory_make("rtph264depay", nullptr);

        GstElement* avdec_h264 = gst_element_factory_make("avdec_h264", nullptr);
        {
            glib::Object<GstPad> pad = gst_element_get_static_pad(avdec_h264, "src");
            gst_pad_add_probe(pad.get(), GST_PAD_PROBE_TYPE_EVENT_BOTH, [](GstPad* pad, GstPadProbeInfo* info, gpointer data) {
                auto video_info = (VideoInfo*) data;
                GstEvent* event = GST_PAD_PROBE_INFO_EVENT(info);
                if (GST_EVENT_TYPE(event) == GST_EVENT_CAPS) {
                    GstCaps* caps = gst_caps_new_any();
                    gst_event_parse_caps(event, &caps);

                    GstStructure* caps_struct = gst_caps_get_structure(caps, 0);
                    gst_structure_get_int(caps_struct, "width", &video_info->width);
                    gst_structure_get_int(caps_struct, "height", &video_info->height);
                    video_info->cv.notify_one();

                    gst_caps_unref(caps);
                }
                return GST_PAD_PROBE_OK;
            },
                &video_info,
                nullptr);
        }

        GstElement* videoconvert = gst_element_factory_make("videoconvert", nullptr);

        GstElement* appsink = gst_element_factory_make("appsink", nullptr);
        {
            GstCaps* caps = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "RGB", nullptr);
            g_object_set(appsink, "caps", caps, "emit-signals", TRUE, nullptr);
            gst_caps_unref(caps);
        }
        glib::connect_signal(appsink, "new-sample", [&video_info](GstElement* appsink) {
            GstSample* sample = gst_app_sink_pull_sample(GST_APP_SINK(appsink));
            video_info.mutex.lock();
            video_info.set_sample(sample);
            video_info.mutex.unlock();
            return GST_FLOW_OK;
        });

        gst_bin_add_many(GST_BIN(pipeline.get()),
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
            fl_alert("Failed to link GStreamer elements");
            continue;
        }

        gst_element_set_state(pipeline.get(), GST_STATE_PLAYING);
        conn->setRemoteDescription(*answer);
        break;
    }

    std::unique_lock<std::mutex> lock(video_info.mutex);
    video_info.cv.wait(lock);
    lock.unlock();

    std::thread([bitrate, conn, unordered_channel, track]() {
        size_t cursor = 0;
        std::vector<float> rtts;
        for (; conn->iceState() != rtc::PeerConnection::IceState::Disconnected &&
               conn->iceState() != rtc::PeerConnection::IceState::Failed;
             std::this_thread::sleep_for(std::chrono::milliseconds(250))) {
            if (unordered_channel->isOpen()) {
                unordered_channel->send("{\"type\":\"ping\"}");
            }

            auto rtt_optional = conn->rtt();
            if (rtt_optional.has_value()) {
                float rtt = rtt_optional.value().count();
                if (rtts.size() < 30) {
                    rtts.push_back(rtt);
                } else {
                    rtts[cursor++] = rtt;
                    if (cursor >= 30) cursor = 0;
                }
                float average_rtt = std::reduce(rtts.begin(), rtts.end()) / rtts.size();
                track->requestBitrate(bitrate * sqrtf(average_rtt / rtt) * 1000);
            }
        }
    }).detach();

    VideoWindow video_window(video_info.width, video_info.height, client_side_mouse);
    video_window.run(video_info.mutex, &video_info.sample, ordered_channel, unordered_channel);

    gst_element_set_state(pipeline.get(), GST_STATE_PAUSED);
    return 0;
}
