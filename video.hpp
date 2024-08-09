#pragma once

#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <array>
#include <gst/gstsample.h>
#include <memory>
#include <mutex>
#include <rtc/datachannel.hpp>

class VideoWindow {
protected:
    SDL_Window* window = nullptr;
    SDL_GLContext gl_context = nullptr;

public:
    const int video_width;
    const int video_height;
    const bool client_side_mouse;

    VideoWindow(int video_width, int video_height, bool client_side_mouse = false, int window_width = 800, int window_height = 450):
        video_width(video_width),
        video_height(video_height),
        client_side_mouse(client_side_mouse) {
        SDL_Init(SDL_INIT_VIDEO);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

        window = SDL_CreateWindow("Lux Desktop", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, window_width, window_height, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
        SDL_SetRelativeMouseMode(client_side_mouse ? SDL_FALSE : SDL_TRUE);
        SDL_SetWindowKeyboardGrab(window, SDL_TRUE);

        gl_context = SDL_GL_CreateContext(window);
        glewInit();
    }
    VideoWindow(const VideoWindow&) = delete;
    VideoWindow(VideoWindow&&) = delete;

    ~VideoWindow() {
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }

    void letterbox(int& x, int& y, int& width, int& height) const;
    std::array<GLfloat, 16> orthographic_matrix() const;
    void window_pos_to_video_pos(int x, int y, int& x_ret, int& y_ret) const;
    void run(std::mutex& sample_mutex, GstSample** sample, std::shared_ptr<rtc::DataChannel> ordered_channel, std::shared_ptr<rtc::DataChannel> unordered_channel);
};