#pragma once

struct RawMouseEvent {
    double x = 0;
    double y = 0;
};

#ifndef _WIN32
typedef struct _XDisplay Display;
#endif

class RawMouseManager {
protected:
#ifndef _WIN32
    Display* display = nullptr;
    int xi_opcode;
#endif

public:
    RawMouseManager();
    RawMouseManager(const RawMouseManager&) = delete;
    RawMouseManager(RawMouseManager&&) = delete;
    ~RawMouseManager();

    bool pending() const;
    RawMouseEvent next_event();
};
