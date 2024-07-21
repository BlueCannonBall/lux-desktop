#include "mouse.hpp"
#include <cassert>
#ifdef _WIN32
    #include <gdk/win32/gdkwin32.h>
    #include <hidusage.h>
    #include <windows.h>
#else
    #include <X11/Xlib.h>
    #include <X11/extensions/XInput2.h>
    #include <gdk/x11/gdkx.h>
#endif

#ifdef _WIN32
class RawMouseManager::Platform {
protected:
    friend class RawMouseManager;

    HINSTANCE instance = nullptr;
    HWND window = nullptr;

public:
    Platform() {
        instance = GetModuleHandle(nullptr);
        assert(instance);

        WNDCLASS window_class = {0};
        window_class.lpfnWndProc = DefWindowProc;
        window_class.hInstance = instance;
        window_class.lpszClassName = "RawMouseManager";
        assert(RegisterClass(&window_class));

        window = CreateWindowEx(
            0,
            "RawMouseManager",
            "Raw Input",
            0,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            nullptr,
            nullptr,
            instance,
            nullptr);
        assert(window);
    }

    ~Platform() {
        if (window) DestroyWindow(window);
        if (instance) UnregisterClass("RawMouseManager", instance);
    }
};

RawMouseManager::RawMouseManager():
    platform(std::make_unique<Platform>()) {
    RAWINPUTDEVICE device;
    device.usUsagePage = HID_USAGE_PAGE_GENERIC;
    device.usUsage = HID_USAGE_GENERIC_MOUSE;
    device.dwFlags = RIDEV_INPUTSINK;
    device.hwndTarget = platform->window;
    assert(RegisterRawInputDevices(&device, 1, sizeof(device)));
}

RawMouseManager::~RawMouseManager() {
    RAWINPUTDEVICE device;
    device.usUsagePage = HID_USAGE_PAGE_GENERIC;
    device.usUsage = HID_USAGE_GENERIC_MOUSE;
    device.dwFlags = RIDEV_REMOVE;
    device.hwndTarget = nullptr;
    assert(RegisterRawInputDevices(&device, 1, sizeof(device)));
}

void RawMouseManager::lock_mouse(GdkSurface* surface) {
    HWND window = GDK_SURFACE_HWND(surface);
    RECT rect;
    GetWindowRect(window, &rect);
    ClipCursor(&rect);
    mouse_locked = true;
}

void RawMouseManager::unlock_mouse(GdkSurface*) {
    if (mouse_locked) {
        ClipCursor(nullptr);
        mouse_locked = false;
    }
}

bool RawMouseManager::pending() const {
    MSG message;
    return PeekMessage(&message, platform->window, 0, 0, PM_NOREMOVE);
}

RawMouseEvent RawMouseManager::next_event() {
    RawMouseEvent ret;
    MSG message;
    while (PeekMessage(&message, platform->window, 0, 0, PM_REMOVE)) {
        if (message.message == WM_INPUT) {
            UINT size;
            if (GetRawInputData((HRAWINPUT) message.lParam, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER))) {
                auto data = new BYTE[size];
                if (GetRawInputData((HRAWINPUT) message.lParam, RID_INPUT, data, &size, sizeof(RAWINPUTHEADER)) == size) {
                    RAWINPUT* raw_event = (RAWINPUT*) data;
                    if (raw_event->header.dwType == RIM_TYPEMOUSE) {
                        RAWMOUSE raw_mouse_event = raw_event->data.mouse;
                        ret.x += raw_mouse_event.lLastX;
                        ret.y += raw_mouse_event.lLastY;
                    }
                }
                delete[] data;
            }
        } else {
            TranslateMessage(&message);
            DispatchMessage(&message);
        }
    }
    return ret;
}
#else
class RawMouseManager::Platform {
protected:
    friend class RawMouseManager;

    Display* display = nullptr;
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

RawMouseManager::~RawMouseManager() = default;

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
