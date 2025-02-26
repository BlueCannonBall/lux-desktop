#include "video.hpp"
#include "json.hpp"
#include "keys.hpp"
#include <FL/fl_ask.H>
#include <iostream>
#include <math.h>
#include <stdlib.h>
#include <string.h>

using nlohmann::json;

Uint32 VIDEO_FRAME_EVENT = SDL_RegisterEvents(1);

void VideoWindow::set_keyboard_grab(bool grabbed) {
    if (grabbed) {
        if (system("qdbus org.kde.kglobalaccel /kglobalaccel blockGlobalShortcuts true") != 0) {
            std::cerr << "Warning: Qt D-Bus call failed" << std::endl;
        }
        SDL_SetWindowKeyboardGrab(window, SDL_TRUE);
    } else {
        if (system("qdbus org.kde.kglobalaccel /kglobalaccel blockGlobalShortcuts false") != 0) {
            std::cerr << "Warning: Qt D-Bus call failed" << std::endl;
        }
        SDL_SetWindowKeyboardGrab(window, SDL_FALSE);
    }
}

void VideoWindow::letterbox(int& x, int& y, int& width, int& height) const {
    int window_width;
    int window_height;
    SDL_GetWindowSize(window, &window_width, &window_height);

    float video_aspect_ratio = (float) video.width / video.height;
    float window_aspect_ratio = (float) window_width / window_height;
    if (video_aspect_ratio > window_aspect_ratio) {
        x = 0;
        width = window_width;
        height = ((float) window_width / video.width) * video.height;
        y = (window_height - height) / 2;
    } else if (video_aspect_ratio < window_aspect_ratio) {
        y = 0;
        width = ((float) window_height / video.height) * video.width;
        height = window_height;
        x = (window_width - width) / 2;
    } else {
        x = 0;
        y = 0;
        width = window_width;
        height = window_height;
    }
}

void VideoWindow::window_pos_to_video_pos(int x, int y, int& x_ret, int& y_ret) const {
    int window_width;
    int window_height;
    SDL_GetWindowSize(window, &window_width, &window_height);

    std::lock_guard<std::mutex> lock(video.mutex);
    float video_aspect_ratio = (float) video.width / video.height;
    float window_aspect_ratio = (float) window_width / window_height;
    if (video_aspect_ratio > window_aspect_ratio) {
        x_ret = x / ((float) window_width / video.width);
        y_ret = (y - ((1.f - window_aspect_ratio / video_aspect_ratio) * window_height / 2.f)) / ((float) window_width / video.width);
    } else if (video_aspect_ratio < window_aspect_ratio) {
        x_ret = (x - ((1.f - video_aspect_ratio / window_aspect_ratio) * window_width / 2.f)) / ((float) window_height / video.height);
        y_ret = y / ((float) window_height / video.height);
    } else {
        x_ret = x / ((float) window_width / video.width);
        y_ret = y / ((float) window_height / video.height);
    }
}

void VideoWindow::run(std::shared_ptr<rtc::PeerConnection> conn, std::shared_ptr<rtc::DataChannel> ordered_channel, std::shared_ptr<rtc::DataChannel> unordered_channel) {
    SDL_Texture* texture = nullptr;
    for (bool running = true, dirty = false; running;) {
        if (conn->iceState() == rtc::PeerConnection::IceState::Closed ||
            conn->iceState() == rtc::PeerConnection::IceState::Disconnected ||
            conn->iceState() == rtc::PeerConnection::IceState::Failed) {
            SDL_SetWindowFullscreen(window, 0);
            if (!view_only) {
                SDL_SetRelativeMouseMode(SDL_FALSE);
                set_keyboard_grab(false);
            }
            fl_alert("The connection has closed.");
            break;
        }

        SDL_Event event;
        if (SDL_WaitEvent(&event)) {
            do {
                switch (event.type) {
                case SDL_QUIT:
                    goto cleanup;

                case SDL_WINDOWEVENT:
                    switch (event.window.event) {
                    case SDL_WINDOWEVENT_EXPOSED:
                        dirty = true;
                        break;

                    case SDL_WINDOWEVENT_FOCUS_LOST:
                        if (!view_only) set_keyboard_grab(false);
                        break;

                    case SDL_WINDOWEVENT_FOCUS_GAINED:
                        if (!view_only) set_keyboard_grab(true);
                        break;
                    }
                    break;

                case SDL_KEYDOWN:
                    if (!view_only &&
                        !event.key.repeat &&
                        event.key.keysym.sym != SDLK_F9 &&
                        event.key.keysym.sym != SDLK_F11 &&
                        event.key.keysym.sym != SDLK_BRIGHTNESSDOWN &&
                        event.key.keysym.sym != SDLK_BRIGHTNESSUP &&
                        event.key.keysym.sym != SDLK_VOLUMEDOWN &&
                        event.key.keysym.sym != SDLK_VOLUMEUP &&
                        ordered_channel->isOpen()) {
                        json message = {
                            {"type", "keydown"},
                            {"key", sdl_to_browser_key(event.key.keysym.sym)},
                        };
                        ordered_channel->send(message.dump());
                    }
                    break;

                case SDL_KEYUP:
                    if (!event.key.repeat) {
                        if (event.key.keysym.sym == SDLK_F11) {
                            Uint32 flags = SDL_GetWindowFlags(window);
                            if (flags & SDL_WINDOW_FULLSCREEN) {
                                SDL_SetWindowFullscreen(window, 0);
                            } else {
                                SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
                            }
                        } else if (!view_only) {
                            if (event.key.keysym.sym == SDLK_F9) {
                                if (!client_side_mouse) {
                                    SDL_SetRelativeMouseMode(SDL_GetRelativeMouseMode() ? SDL_FALSE : SDL_TRUE);
                                }
                            } else if (event.key.keysym.sym != SDLK_BRIGHTNESSDOWN &&
                                       event.key.keysym.sym != SDLK_BRIGHTNESSUP &&
                                       event.key.keysym.sym != SDLK_VOLUMEDOWN &&
                                       event.key.keysym.sym != SDLK_VOLUMEUP &&
                                       ordered_channel->isOpen()) {
                                json message = {
                                    {"type", "keyup"},
                                    {"key", sdl_to_browser_key(event.key.keysym.sym)},
                                };
                                ordered_channel->send(message.dump());
                            }
                        }
                    }
                    break;

                case SDL_MOUSEMOTION:
                    if (!view_only) {
                        if (client_side_mouse) {
                            if (ordered_channel->isOpen()) {
                                int x;
                                int y;
                                window_pos_to_video_pos(event.motion.x, event.motion.y, x, y);

                                json message = {
                                    {"type", "mousemoveabs"},
                                    {"x", x},
                                    {"y", y},
                                };
                                ordered_channel->send(message.dump());
                            }
                        } else {
                            if (unordered_channel->isOpen()) {
                                json message = {
                                    {"type", "mousemove"},
                                    {"x", event.motion.xrel},
                                    {"y", event.motion.yrel},
                                };
                                unordered_channel->send(message.dump());
                            }
                        }
                    }
                    break;

                case SDL_MOUSEBUTTONDOWN:
                    if (!view_only) {
                        if (!client_side_mouse && !SDL_GetRelativeMouseMode()) {
                            SDL_SetRelativeMouseMode(SDL_TRUE);
                        } else if (ordered_channel->isOpen()) {
                            json message = {
                                {"type", "mousedown"},
                                {"button", event.button.button - 1},
                            };
                            ordered_channel->send(message.dump());
                        }
                    }
                    break;

                case SDL_MOUSEBUTTONUP:
                    if (!view_only && ordered_channel->isOpen()) {
                        json message = {
                            {"type", "mouseup"},
                            {"button", event.button.button - 1},
                        };
                        ordered_channel->send(message.dump());
                    }
                    break;

                case SDL_MOUSEWHEEL:
                    if (!view_only && unordered_channel->isOpen()) {
                        json message = {
                            {"type", "wheel"},
                            {"x", (int) roundf(event.wheel.preciseX * 120.f)},
                            {"y", (int) roundf(event.wheel.preciseY * -120.f)},
                        };
                        unordered_channel->send(message.dump());
                    }
                    break;
                }
            } while (SDL_PollEvent(&event));
        }

        video.mutex.lock();
        if (video.sample) {
            if (video.resized) {
                SDL_DestroyTexture(texture);
                texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, video.width, video.height);
                video.resized = false;
            }

            GstBuffer* buf = gst_sample_get_buffer(video.sample);
            GstMapInfo map;
            gst_buffer_map(buf, &map, GST_MAP_READ);

            void* pixels;
            int pitch;
            SDL_LockTexture(texture, nullptr, &pixels, &pitch);
            memcpy(pixels, map.data, video.width * video.height * 3);
            SDL_UnlockTexture(texture);
            dirty = true;

            gst_buffer_unmap(buf, &map);
            video.set_sample(nullptr);
        }
        video.mutex.unlock();

        if (dirty) {
            SDL_Rect dest;
            letterbox(dest.x, dest.y, dest.w, dest.h);
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, nullptr, &dest);
            SDL_RenderPresent(renderer);
            dirty = false;
        }
    }
cleanup:
    SDL_DestroyTexture(texture);
}
