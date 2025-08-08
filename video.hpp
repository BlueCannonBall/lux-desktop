#pragma once

#include "connection.hpp"
#include "glib.hpp"
#include "mouse.hpp"
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <SDL2/SDL.h>
#include <assert.h>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>
#include <memory>
#include <mutex>
#include <rtc/rtc.hpp>

struct VideoInfo {
    std::mutex mutex;
    int width;
    int height;
};

class VideoWindow : public Fl_Window {
protected:
    ConnectionInfo conn_info;

    std::shared_ptr<rtc::PeerConnection> conn;
    std::shared_ptr<rtc::Track> video_track;
    std::shared_ptr<rtc::Track> audio_track;
    std::shared_ptr<rtc::DataChannel> ordered_channel;
    std::shared_ptr<rtc::DataChannel> unordered_channel;

    RawMouseManager mouse_manager;

    VideoInfo video_info;
    glib::Object<GstElement> video_pipeline;
    glib::Object<GstElement> audio_pipeline;
    GstVideoOverlay* overlay = nullptr;

    bool connected = false;
    bool playing = false;

    static int system_event_handler(void* event, void* data);

public:
    VideoWindow(int x, int y, int width, int height, ConnectionInfo conn_info);

    bool is_connected() const;
    bool is_playing() const;
    void show() override;
    void hide() override;
    void draw() override;
    int handle(int event) override;
    void position_in_video(int x, int y, int& x_ret, int& y_ret);
};
