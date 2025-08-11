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

    Fl_Window* window;

public:
    bool mouse_locked = false;

    RawMouseManager(Fl_Window* window);
    RawMouseManager(const RawMouseManager&) = delete;
    RawMouseManager(RawMouseManager&&) = delete;

    RawMouseManager& operator=(const RawMouseManager&) = delete;
    RawMouseManager& operator=(RawMouseManager&&) = delete;

    ~RawMouseManager();

    void lock_mouse();
    void unlock_mouse();
    std::optional<RawMouseEvent> parse_event(void* event);
};

class KeyboardGrabManager {
protected:
    class Platform;
    std::unique_ptr<Platform> platform;

    Fl_Window* window;

public:
    bool keyboard_grabbed = false;

    KeyboardGrabManager(Fl_Window* window);
    KeyboardGrabManager(const KeyboardGrabManager&) = delete;
    KeyboardGrabManager(KeyboardGrabManager&&) = delete;

    KeyboardGrabManager& operator=(const KeyboardGrabManager&) = delete;
    KeyboardGrabManager& operator=(KeyboardGrabManager&&) = delete;

    ~KeyboardGrabManager();

    void grab_keyboard();
    void ungrab_keyboard();
};
