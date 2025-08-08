#include "mouse.hpp"
#include <FL/Fl_Window.H>
#include <FL/x.H>
#include <assert.h>
#ifdef _WIN32
// clang-format off
    #include <windows.h>
    #include <hidusage.h>
// clang-format on
#else
    #include <X11/Xlib.h>
    #include <X11/extensions/XInput2.h>
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
    Platform(const Platform&) = delete;
    Platform(Platform&&) = delete;

    Platform& operator=(const Platform&) = delete;
    Platform& operator=(Platform&&) = delete;

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

    Display* display;
    int xi_opcode;

public:
    Platform(Display* display = fl_x11_display()):
        display(display) {
        int event;
        int error;
        assert(XQueryExtension(display, "XInputExtension", &xi_opcode, &event, &error) == True);
    }
};

RawMouseManager::RawMouseManager():
    platform(std::make_unique<Platform>()) {
    XIEventMask masks[1];
    unsigned char mask[(XI_LASTEVENT + 7) / 8] = {0};
    masks[0].deviceid = XIAllMasterDevices;
    masks[0].mask_len = sizeof(mask);
    masks[0].mask = mask;
    XISetMask(mask, XI_RawMotion);
    XISelectEvents(platform->display, DefaultRootWindow(platform->display), masks, 1);
    XFlush(platform->display);
}

RawMouseManager::~RawMouseManager() {
    if (mouse_locked) {
        XUngrabPointer(platform->display, CurrentTime);
    }

    XIEventMask masks[1];
    unsigned char mask[] = {0};
    masks[0].deviceid = XIAllMasterDevices;
    masks[0].mask_len = sizeof(mask);
    masks[0].mask = mask;
    XISelectEvents(platform->display, DefaultRootWindow(platform->display), masks, 1);
    XFlush(platform->display);
}

void RawMouseManager::lock_mouse(Fl_Window* window) {
    window->cursor(FL_CURSOR_NONE);
    if (!mouse_locked) {
        Window x11_window = fl_xid(window);
        XGrabPointer(platform->display,
            x11_window,
            False,
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
                ButtonMotionMask |
                KeymapStateMask,
            GrabModeAsync,
            GrabModeAsync,
            x11_window,
            None,
            CurrentTime);
        mouse_locked = true;
    }
}

void RawMouseManager::unlock_mouse(Fl_Window* window) {
    window->cursor(FL_CURSOR_DEFAULT);
    if (mouse_locked) {
        XUngrabPointer(platform->display, CurrentTime);
        mouse_locked = false;
    }
}

std::optional<RawMouseEvent> RawMouseManager::parse_event(void* event) {
    auto x11_event = (XEvent*) event;
    if (XGenericEventCookie* cookie = &x11_event->xcookie;
        cookie->type == GenericEvent &&
        cookie->extension == platform->xi_opcode &&
        XGetEventData(platform->display, cookie)) {
        if (cookie->evtype == XI_RawMotion) {
            RawMouseEvent ret;
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
            XFreeEventData(platform->display, cookie);
            return ret;
        }
        XFreeEventData(platform->display, cookie);
    }
    return std::nullopt;
}
#endif
