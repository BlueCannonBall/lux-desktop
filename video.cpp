#include "video.hpp"
#include "json.hpp"
#include "keys.hpp"
#include <FL/fl_ask.H>
#include <gst/video/videooverlay.h>
#include <math.h>
#if !defined(_WIN32) && !defined(__APPLE__)
    #include <iostream>
    #include <stdlib.h>
    #include <string.h>
#endif

using nlohmann::json;

bool is_kde() {
#if !defined(_WIN32) && !defined(__APPLE__)
    char* xdg_current_desktop;
    return (xdg_current_desktop = getenv("XDG_CURRENT_DESKTOP")) && !strcmp(xdg_current_desktop, "KDE");
#else
    return false;
#endif
}

void VideoWindow::set_keyboard_grab(bool grabbed) {
    if (grabbed) {
#if !defined(_WIN32) && !defined(__APPLE__)
        if (is_kde() && system("qdbus org.kde.kglobalaccel /kglobalaccel blockGlobalShortcuts true") != 0) {
            std::cerr << "Warning: Qt D-Bus call failed (ignore unless on KDE)" << std::endl;
        }
#endif
        SDL_SetWindowKeyboardGrab(window, SDL_TRUE);
    } else {
#if !defined(_WIN32) && !defined(__APPLE__)
        if (is_kde() && system("qdbus org.kde.kglobalaccel /kglobalaccel blockGlobalShortcuts false") != 0) {
            std::cerr << "Warning: Qt D-Bus call failed (ignore unless on KDE)" << std::endl;
        }
#endif
        SDL_SetWindowKeyboardGrab(window, SDL_FALSE);
    }
}

void VideoWindow::position_in_video(int x, int y, int& x_ret, int& y_ret) const {
    int window_width;
    int window_height;
    SDL_GetWindowSize(window, &window_width, &window_height);

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

void VideoWindow::run(std::shared_ptr<rtc::PeerConnection> conn, std::shared_ptr<rtc::Track> track, std::shared_ptr<rtc::DataChannel> ordered_channel, std::shared_ptr<rtc::DataChannel> unordered_channel) {
    for (;;) {
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
                    gst_element_set_state(GST_ELEMENT(overlay), GST_STATE_NULL);
                    return;

                case SDL_WINDOWEVENT:
                    switch (event.window.event) {
                    case SDL_WINDOWEVENT_EXPOSED:
                        gst_video_overlay_expose(GST_VIDEO_OVERLAY(overlay));
                        break;

                    case SDL_WINDOWEVENT_FOCUS_LOST:
                        if (!view_only) {
                            set_keyboard_grab(false);
                        }
                        break;

                    case SDL_WINDOWEVENT_FOCUS_GAINED:
                        if (!view_only) {
                            set_keyboard_grab(true);
                        }
                        break;
                    }
                    break;

                case SDL_KEYDOWN:
                    if (!view_only &&
                        event.key.keysym.sym != SDLK_F5 &&
                        event.key.keysym.sym != SDLK_F9 &&
                        event.key.keysym.sym != SDLK_F11 &&
                        ordered_channel->isOpen()) {
                        json message = {
                            {"type", "keydown"},
                            {"key", sdl_to_browser_key(event.key.keysym.sym)},
                        };
                        ordered_channel->send(message.dump());
                    }
                    break;

                case SDL_KEYUP:
                    if (event.key.keysym.sym == SDLK_F5) {
                        track->requestKeyframe();
                    } else if (event.key.keysym.sym == SDLK_F11) {
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
                        } else if (ordered_channel->isOpen()) {
                            json message = {
                                {"type", "keyup"},
                                {"key", sdl_to_browser_key(event.key.keysym.sym)},
                            };
                            ordered_channel->send(message.dump());
                        }
                    }
                    break;

                case SDL_MOUSEMOTION:
                    if (!view_only) {
                        if (client_side_mouse) {
                            if (ordered_channel->isOpen()) {
                                int x;
                                int y;
                                position_in_video(event.motion.x, event.motion.y, x, y);

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
    }
}
