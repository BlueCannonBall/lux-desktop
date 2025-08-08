#pragma once

#include <FL/Fl.H>
#include <memory>
#include <optional>

struct RawMouseEvent {
    double x = 0;
    double y = 0;
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

    RawMouseManager& operator=(const RawMouseManager&) = delete;
    RawMouseManager& operator=(RawMouseManager&&) = delete;

    ~RawMouseManager();

    void lock_mouse(Fl_Window* window);
    void unlock_mouse(Fl_Window* window);

    std::optional<RawMouseEvent> parse_event(void* event);
};
