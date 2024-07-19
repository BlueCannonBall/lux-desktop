#pragma once

#include <gdk/gdk.h>
#include <memory>

struct RawMouseEvent {
    float x = 0;
    float y = 0;
};

class RawMouseManager {
protected:
    class Platform;
    std::unique_ptr<Platform> platform;

public:
    bool mouse_locked = false;

    RawMouseManager();
    RawMouseManager(const RawMouseManager&) = delete;
    RawMouseManager(RawMouseManager&&) = delete;
    ~RawMouseManager();

    void lock_mouse(GdkSurface* surface);
    void unlock_mouse(GdkSurface* surface);

    bool pending() const;
    RawMouseEvent next_event();
};
