#pragma once

#include <SDL2/SDL.h>
#include <assert.h>
#include <condition_variable>
#include <gst/gstsample.h>
#include <memory>
#include <mutex>
#include <rtc/rtc.hpp>
#include <stddef.h>

extern Uint32 VIDEO_FRAME_EVENT;

struct Video {
    std::mutex mutex;
    std::condition_variable cv;
    int width;
    int height;
    int stride;
    size_t offset;
    bool resized = false;
    GstSample* sample = nullptr;

    ~Video() {
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

class VideoWindow {
protected:
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;

    void set_keyboard_grab(bool grabbed);

public:
    Video& video;
    const bool client_side_mouse;
    const bool view_only;

    VideoWindow(Video& video, bool client_side_mouse = false, bool view_only = false, int window_width = 960, int window_height = 540):
        video(video),
        client_side_mouse(client_side_mouse),
        view_only(view_only) {
        SDL_SetHint(SDL_HINT_APP_NAME, "Lux Client");
        SDL_SetHint(SDL_HINT_SCREENSAVER_INHIBIT_ACTIVITY_NAME, "Watching a video");
        SDL_SetHint(SDL_HINT_ALLOW_ALT_TAB_WHILE_GRABBED, "0");
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
        SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "1");
        SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
        SDL_InitSubSystem(SDL_INIT_VIDEO);

        window = SDL_CreateWindow("Lux Client", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, window_width, window_height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
        if (!view_only) {
            set_keyboard_grab(true);
            SDL_SetRelativeMouseMode(client_side_mouse ? SDL_FALSE : SDL_TRUE);
        }

        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    }
    VideoWindow(const VideoWindow&) = delete;
    VideoWindow(VideoWindow&&) = delete;

    ~VideoWindow() {
        SDL_DestroyRenderer(renderer);

        if (!view_only) {
            SDL_SetRelativeMouseMode(SDL_FALSE);
            set_keyboard_grab(false);
        }
        SDL_DestroyWindow(window);

        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }

    void letterbox(int& x, int& y, int& width, int& height) const;
    void position_in_video(int x, int y, int& x_ret, int& y_ret) const;
    void run(std::shared_ptr<rtc::PeerConnection> conn, std::shared_ptr<rtc::Track> track, std::shared_ptr<rtc::DataChannel> ordered_channel = nullptr, std::shared_ptr<rtc::DataChannel> unordered_channel = nullptr);
};
