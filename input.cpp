#include "input.hpp"
#include <FL/Fl_Window.H>
#include <FL/x.H>
#include <assert.h>
#include <iostream>
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
private:
    friend class RawMouseManager;

    HWND window;

public:
    Platform(Fl_Window* window):
        window(fl_xid(window)) {}
};

RawMouseManager::RawMouseManager(Fl_Window* window):
    platform(std::make_unique<Platform>(window)),
    window(window) {
    RAWINPUTDEVICE device;
    device.usUsagePage = HID_USAGE_PAGE_GENERIC;
    device.usUsage = HID_USAGE_GENERIC_MOUSE;
    device.dwFlags = RIDEV_INPUTSINK;
    device.hwndTarget = platform->window;
    // Not inside assert(): NDEBUG would drop the registration itself, and raw
    // mouse input would silently stop working in exactly the builds we ship
    if (!RegisterRawInputDevices(&device, 1, sizeof device)) {
        std::cerr << "Error: Failed to register raw mouse input" << std::endl;
    }
}

RawMouseManager::~RawMouseManager() {
    unlock_mouse();

    RAWINPUTDEVICE device;
    device.usUsagePage = HID_USAGE_PAGE_GENERIC;
    device.usUsage = HID_USAGE_GENERIC_MOUSE;
    device.dwFlags = RIDEV_REMOVE;
    device.hwndTarget = nullptr;
    if (!RegisterRawInputDevices(&device, 1, sizeof device)) {
        std::cerr << "Error: Failed to unregister raw mouse input" << std::endl;
    }
}

void RawMouseManager::lock_mouse() {
    window->cursor(FL_CURSOR_NONE);
    RECT rect;
    GetClientRect(platform->window, &rect);
    POINT ul = {rect.left, rect.top};
    POINT lr = {rect.right, rect.bottom};
    ClientToScreen(platform->window, &ul);
    ClientToScreen(platform->window, &lr);
    RECT screen_rect = {ul.x + 1, ul.y + 1, lr.x - 2, lr.y - 2};
    ClipCursor(&screen_rect);
    mouse_locked = true;
}

void RawMouseManager::unlock_mouse() {
    window->cursor(FL_CURSOR_DEFAULT);
    ClipCursor(nullptr);
    mouse_locked = false;
}

std::optional<RawMouseEvent> RawMouseManager::parse_event(void* event) {
    if (mouse_locked) {
        auto message = (MSG*) event;
        if (message->message == WM_INPUT) {
            UINT size;
            if (GetRawInputData((HRAWINPUT) message->lParam, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER)) == 0) {
                auto data = new BYTE[size];
                if (GetRawInputData((HRAWINPUT) message->lParam, RID_INPUT, data, &size, sizeof(RAWINPUTHEADER)) == size) {
                    RAWINPUT* raw_event = (RAWINPUT*) data;
                    if (raw_event->header.dwType == RIM_TYPEMOUSE) {
                        RawMouseEvent ret;

                        RAWMOUSE raw_mouse_event = raw_event->data.mouse;
                        ret.x = raw_mouse_event.lLastX;
                        ret.y = raw_mouse_event.lLastY;

                        RECT rect;
                        GetClientRect(platform->window, &rect);
                        POINT center = {
                            rect.right / 2,
                            rect.bottom / 2,
                        };
                        ClientToScreen(platform->window, &center);
                        SetCursorPos(center.x, center.y);

                        return ret;
                    }
                }
                delete[] data;
            }
        }
    }
    return std::nullopt;
}

class KeyboardGrabManager::Platform {
private:
    friend class KeyboardGrabManager;

    static HWND window;
    static HHOOK hook;
    static bool keyboard_grabbed;

    static void post_key(WORD vkCode, UINT type, DWORD flags) {
        UINT scancode = MapVirtualKey(vkCode, MAPVK_VK_TO_VSC);
        LPARAM lParam = 1 | (scancode << 16); // Repeat count = 1
        if (flags & LLKHF_EXTENDED) lParam |= (1U << 24);
        // Bit 29 is the context code, which is how a window is told Alt was held.
        // FLTK reads it directly to set FL_ALT, so a synthesised WM_SYSKEYDOWN
        // without it arrives claiming the Alt combination was not one
        if (flags & LLKHF_ALTDOWN) lParam |= (1U << 29);
        if (type == WM_KEYUP || type == WM_SYSKEYUP) {
            lParam |= (1U << 31); // Transition (up)
            lParam |= (1U << 30); // Previous state
        }
        PostMessage(window, type, vkCode, lParam);
    }

    // The hook is installed globally, so being grabbed is not enough to justify
    // swallowing a key: the pointer can sit over our window while another
    // application is focused, and eating the shell's shortcuts desktop-wide would
    // be far worse now that more than four keys are taken. GA_ROOT so that a
    // focused child window still counts as ours
    static bool foreground_is_ours() {
        HWND foreground = GetForegroundWindow();
        return foreground && window && GetAncestor(foreground, GA_ROOT) == GetAncestor(window, GA_ROOT);
    }

    static LRESULT CALLBACK hook_cb(int nCode, WPARAM wParam, LPARAM lParam) {
        if (keyboard_grabbed &&
            nCode == HC_ACTION &&
            (wParam == WM_KEYDOWN ||
                wParam == WM_SYSKEYDOWN ||
                wParam == WM_KEYUP ||
                wParam == WM_SYSKEYUP) &&
            foreground_is_ours()) {
            auto event_info = (KBDLLHOOKSTRUCT*) lParam;
            switch (event_info->vkCode) {
            case VK_LWIN:
            case VK_RWIN:
            case VK_TAB:
            case VK_SNAPSHOT:
                post_key(event_info->vkCode, wParam, event_info->flags);
                return 1;

            case VK_MENU:
            case VK_LMENU:
            case VK_RMENU:
                // Alt itself is left on the ordinary path, which already delivers
                // it and is known to work. Swallowing it here would gain nothing
                // and would risk the window never learning Alt is down
                break;

            default:
                // Windows classifies a key as a system key exactly when Alt is
                // held (and for F10), which is precisely where window manager
                // bindings live: GlazeWM's Alt+1..9, Alt+Enter, Alt+arrows.
                // Taking the whole class beats enumerating combinations we would
                // otherwise keep discovering one complaint at a time
                if (wParam == WM_SYSKEYDOWN || wParam == WM_SYSKEYUP) {
                    post_key(event_info->vkCode, wParam, event_info->flags);
                    return 1;
                }
                break;
            }
        }
        return CallNextHookEx(hook, nCode, wParam, lParam);
    }

public:
    Platform(Fl_Window* window) {
        assert(this->window == nullptr);
        this->window = fl_xid(window);

        assert(hook == nullptr);
        hook = SetWindowsHookEx(WH_KEYBOARD_LL, hook_cb, GetModuleHandle(NULL), 0);
    }
    Platform(const Platform&) = delete;
    Platform(Platform&&) = delete;

    Platform& operator=(const Platform&) = delete;
    Platform& operator=(Platform&&) = delete;

    ~Platform() {
        if (hook) {
            UnhookWindowsHookEx(hook);
            hook = nullptr;
        }
        if (window) window = nullptr;
    }
};

HWND KeyboardGrabManager::Platform::window = nullptr;
HHOOK KeyboardGrabManager::Platform::hook = nullptr;
bool KeyboardGrabManager::Platform::keyboard_grabbed = false;

KeyboardGrabManager::KeyboardGrabManager(Fl_Window* window):
    platform(std::make_unique<Platform>(window)) {}

KeyboardGrabManager::~KeyboardGrabManager() {
    ungrab_keyboard();
}

void KeyboardGrabManager::grab_keyboard() {
    keyboard_grabbed = platform->keyboard_grabbed = true;
}

void KeyboardGrabManager::ungrab_keyboard() {
    keyboard_grabbed = platform->keyboard_grabbed = false;
}
#else
class RawMouseManager::Platform {
private:
    friend class RawMouseManager;

    Window window;
    Display* display;
    int xi_opcode = -1; // Never matches a real event, so a failed query just disables raw input

public:
    Platform(Fl_Window* window, Display* display = fl_x11_display()):
        window(fl_xid(window)),
        display(display) {
        int event;
        int error;
        // Not inside assert(): NDEBUG would drop the query and leave xi_opcode unset
        if (XQueryExtension(display, "XInputExtension", &xi_opcode, &event, &error) != True) {
            std::cerr << "Error: XInput extension is unavailable, raw mouse input is disabled" << std::endl;
        }
    }
};

RawMouseManager::RawMouseManager(Fl_Window* window):
    platform(std::make_unique<Platform>(window)),
    window(window) {
    XIEventMask masks[1];
    unsigned char mask[(XI_LASTEVENT + 7) / 8] = {0};
    masks[0].deviceid = XIAllMasterDevices;
    masks[0].mask_len = sizeof mask;
    masks[0].mask = mask;
    XISetMask(mask, XI_RawMotion);
    XISelectEvents(platform->display, DefaultRootWindow(platform->display), masks, 1);
    XFlush(platform->display);
}

RawMouseManager::~RawMouseManager() {
    unlock_mouse();

    XIEventMask masks[1];
    unsigned char mask[] = {0};
    masks[0].deviceid = XIAllMasterDevices;
    masks[0].mask_len = sizeof mask;
    masks[0].mask = mask;
    XISelectEvents(platform->display, DefaultRootWindow(platform->display), masks, 1);
    XFlush(platform->display);
}

void RawMouseManager::lock_mouse() {
    window->cursor(FL_CURSOR_NONE);
    if (!mouse_locked) {
        XGrabPointer(platform->display,
            platform->window,
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
            platform->window,
            None,
            CurrentTime);
        mouse_locked = true;
    }
}

void RawMouseManager::unlock_mouse() {
    window->cursor(FL_CURSOR_DEFAULT);
    if (mouse_locked) {
        XUngrabPointer(platform->display, CurrentTime);
        mouse_locked = false;
    }
}

std::optional<RawMouseEvent> RawMouseManager::parse_event(void* event) {
    if (mouse_locked) {
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
                            ret.x = *value;
                        } else if (i == 1) {
                            ret.y = *value;
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
    }
    return std::nullopt;
}

class KeyboardGrabManager::Platform {
private:
    friend class KeyboardGrabManager;

    Window window;
    Display* display;

public:
    Platform(Fl_Window* window, Display* display = fl_x11_display()):
        window(fl_xid(window)),
        display(display) {}
};

KeyboardGrabManager::KeyboardGrabManager(Fl_Window* window):
    platform(std::make_unique<Platform>(window)) {}

KeyboardGrabManager::~KeyboardGrabManager() {
    ungrab_keyboard();
}

void KeyboardGrabManager::grab_keyboard() {
    if (!keyboard_grabbed) {
        if (int result = XGrabKeyboard(platform->display, platform->window, False, GrabModeAsync, GrabModeAsync, CurrentTime); result != GrabSuccess) {
            std::cerr << "Error: Failed to grab keyboard: " << result << std::endl;
        } else {
            keyboard_grabbed = true;
        }
    }
}

void KeyboardGrabManager::ungrab_keyboard() {
    if (keyboard_grabbed) {
        XUngrabKeyboard(platform->display, CurrentTime);
        keyboard_grabbed = false;
    }
}
#endif
