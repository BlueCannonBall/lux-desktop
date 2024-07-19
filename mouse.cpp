#include "mouse.hpp"
#include <cassert>
#include <iostream>
#ifdef _WIN32
    #include <gdk/x11/gdkx.h>
    #include <windows.h>
#else
    #include <X11/Xlib.h>
    #include <X11/extensions/XInput2.h>
    #include <gdk/x11/gdkx.h>
#endif

#ifdef _WIN32
struct RawMouseManager::Platform {
};

RawMouseManager::RawMouseManager():
    platform(std::make_unique<Platform>()) {
}
#else
class RawMouseManager::Platform {
protected:
    friend class RawMouseManager;

    Display* display;
    int xi_opcode;

public:
    Platform(const char* display_name = nullptr):
        display(XOpenDisplay(display_name)) {
        assert(display);

        int event;
        int error;
        assert(XQueryExtension(display, "XInputExtension", &xi_opcode, &event, &error) == True);
    }
    Platform(const Platform&) = delete;
    Platform(Platform&&) = delete;

    ~Platform() {
        if (display) XCloseDisplay(display);
    }
};

RawMouseManager::RawMouseManager():
    platform(std::make_unique<Platform>()) {
    XIEventMask masks[1];
    unsigned char mask[(XI_LASTEVENT + 7) / 8] = {0};
    masks[0].deviceid = XIAllDevices;
    masks[0].mask_len = sizeof(mask);
    masks[0].mask = mask;
    XISetMask(mask, XI_RawMotion);
    XISelectEvents(platform->display, DefaultRootWindow(platform->display), masks, 1);
    XFlush(platform->display);
}

void RawMouseManager::lock_mouse(GdkSurface* surface) {
    if (!mouse_locked) {
        Window window = GDK_SURFACE_XID(surface);
        Display* display = GDK_DISPLAY_XDISPLAY(gdk_surface_get_display(surface));

        XColor black;
        black.red = black.green = black.blue = 0;

        static constexpr char data[] = {0};
        Pixmap bitmap = XCreateBitmapFromData(display, window, data, 1, 1);

        Cursor cursor = XCreatePixmapCursor(display, bitmap, bitmap, &black, &black, 0, 0);
        XFreePixmap(display, bitmap);

        XGrabPointer(display,
            window,
            True,
            ButtonPressMask |
                ButtonReleaseMask |
                EnterWindowMask |
                LeaveWindowMask |
                PointerMotionMask |
                PointerMotionHintMask |
                Button1MotionMask |
                Button2MotionMask |
                Button3MotionMask |
                Button4MotionMask |
                Button5MotionMask |
                KeymapStateMask,
            GrabModeAsync,
            GrabModeAsync,
            window,
            cursor,
            CurrentTime);
        mouse_locked = true;

        XFreeCursor(display, cursor);
    }
}

void RawMouseManager::unlock_mouse(GdkSurface* surface) {
    if (mouse_locked) {
        Display* display = GDK_DISPLAY_XDISPLAY(gdk_surface_get_display(surface));
        XUngrabPointer(display, CurrentTime);
        mouse_locked = false;
    }
}

bool RawMouseManager::pending() const {
    return XPending(platform->display);
}

RawMouseEvent RawMouseManager::next_event() {
    RawMouseEvent ret;
    while (pending()) {
        XEvent event;
        XGenericEventCookie* cookie = &event.xcookie;
        XNextEvent(platform->display, &event);
        if (cookie->type == GenericEvent && cookie->extension == platform->xi_opcode && XGetEventData(platform->display, cookie)) {
            if (cookie->evtype == XI_RawMotion) {
                XIRawEvent* raw_event = (XIRawEvent*) cookie->data;
                double* value = raw_event->raw_values;
                for (int i = 0; i < raw_event->valuators.mask_len * 8; ++i) {
                    if (XIMaskIsSet(raw_event->valuators.mask, i)) {
                        if (i == 0) {
                            ret.x += *value;
                        } else if (i == 1) {
                            ret.y += *value;
                        } else {
                            break;
                        }
                        ++value;
                    }
                }
            }
            XFreeEventData(platform->display, cookie);
        }
    }
    return ret;
}
#endif

RawMouseManager::~RawMouseManager() = default;
