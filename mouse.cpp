#include "mouse.hpp"
#include <cassert>
#ifdef _WIN32

#else
    #include <X11/Xlib.h>
    #include <X11/extensions/XInput2.h>
#endif

#ifdef _WIN32
RawInputManager::RawInputManager() {
}
#else
RawMouseManager::RawMouseManager():
    display(XOpenDisplay(nullptr)) {
    assert(display);

    int event;
    int error;
    assert(XQueryExtension(display, "XInputExtension", &xi_opcode, &event, &error) == True);

    XIEventMask masks[1];
    unsigned char mask[(XI_LASTEVENT + 7) / 8] = {0};
    masks[0].deviceid = XIAllDevices;
    masks[0].mask_len = sizeof(mask);
    masks[0].mask = mask;
    XISetMask(mask, XI_RawMotion);
    XISelectEvents(display, DefaultRootWindow(display), masks, 1);
    XFlush(display);
}

RawMouseManager::~RawMouseManager() {
    if (display) XCloseDisplay(display);
}

bool RawMouseManager::pending() const {
    return XPending(display);
}

RawMouseEvent RawMouseManager::next_event() {
    RawMouseEvent ret;
    while (pending()) {
        XEvent event;
        XGenericEventCookie* cookie = &event.xcookie;
        XNextEvent(display, &event);
        if (cookie->type == GenericEvent && cookie->extension == xi_opcode && XGetEventData(display, cookie)) {
            if (cookie->evtype == XI_RawMotion) {
                XIRawEvent* raw_event = (XIRawEvent*) cookie->data;
                double* value = raw_event->raw_values;
                for (int i = 0; i < raw_event->valuators.mask_len * 8; ++i) {
                    if (XIMaskIsSet(raw_event->valuators.mask, i)) {
                        if (i == 0) {
                            ret.x += *value;
                        } else if (i == 1) {
                            ret.y += *value;
                        }
                        ++value;
                    }
                }
            }
            XFreeEventData(display, cookie);
        }
    }
    return ret;
}
#endif
