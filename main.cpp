#include "Polyweb/polyweb.hpp"
#include "theme.hpp"
#include "ui.hpp"
#include <FL/Fl.H>
#include <gst/gst.h>
#include <rtc/rtc.hpp>
#ifdef _WIN32
    #include <ios>
    #include <stdint.h>
    #include <stdio.h>
    #include <windows.h>
#elif !defined(__APPLE__)
    #include <stdlib.h>
#endif

using nlohmann::json;

int main(int argc, char* argv[]) {
#ifdef _WIN32
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        FILE* fp;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
        freopen_s(&fp, "CONIN$", "r", stdin);
        std::ios::sync_with_stdio();
    }
#elif !defined(__APPLE__)
    unsetenv("WAYLAND_DISPLAY"); // The video can't be displayed efficiently on Wayland, so Xwayland is forced
#endif

    Fl::visual(FL_DOUBLE | FL_INDEX);
    configure_fltk_colors();
    rtc::InitLogger(rtc::LogLevel::Debug);
    pn::init();
    pw::threadpool.resize(0); // The threadpool is only used by Polyweb in server applications
    gst_init(&argc, &argv);

    MainWindow window;
    window.show();
    return Fl::run();
}
