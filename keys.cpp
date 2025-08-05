#include "keys.hpp"
#include <FL/Fl.H>
#include <stdio.h>

std::string_view fltk_to_browser_key(int key) {
    switch (key) {
    // ASCII Printable Characters
    case 'a': return "KeyA";
    case 'b': return "KeyB";
    case 'c': return "KeyC";
    case 'd': return "KeyD";
    case 'e': return "KeyE";
    case 'f': return "KeyF";
    case 'g': return "KeyG";
    case 'h': return "KeyH";
    case 'i': return "KeyI";
    case 'j': return "KeyJ";
    case 'k': return "KeyK";
    case 'l': return "KeyL";
    case 'm': return "KeyM";
    case 'n': return "KeyN";
    case 'o': return "KeyO";
    case 'p': return "KeyP";
    case 'q': return "KeyQ";
    case 'r': return "KeyR";
    case 's': return "KeyS";
    case 't': return "KeyT";
    case 'u': return "KeyU";
    case 'v': return "KeyV";
    case 'w': return "KeyW";
    case 'x': return "KeyX";
    case 'y': return "KeyY";
    case 'z': return "KeyZ";

    case '1': return "Digit1";
    case '2': return "Digit2";
    case '3': return "Digit3";
    case '4': return "Digit4";
    case '5': return "Digit5";
    case '6': return "Digit6";
    case '7': return "Digit7";
    case '8': return "Digit8";
    case '9': return "Digit9";
    case '0': return "Digit0";

    case ' ': return "Space";
    case '`': return "Backquote";
    case '-': return "Minus";
    case '=': return "Equal";
    case '[': return "BracketLeft";
    case ']': return "BracketRight";
    case '\\': return "Backslash";
    case ';': return "Semicolon";
    case '\'': return "Quote";
    case ',': return "Comma";
    case '.': return "Period";
    case '/': return "Slash";

    // Special Keys (from fl_ask.H)
    case FL_Escape: return "Escape";
    case FL_BackSpace: return "Backspace";
    case FL_Tab: return "Tab";
    case FL_Enter: return "Enter";
    case FL_Print: return "PrintScreen";
    case FL_Scroll_Lock: return "ScrollLock";
    case FL_Pause: return "Pause";
    case FL_Insert: return "Insert";
    case FL_Home: return "Home";
    case FL_Page_Up: return "PageUp";
    case FL_Delete: return "Delete";
    case FL_End: return "End";
    case FL_Page_Down: return "PageDown";
    case FL_Left: return "ArrowLeft";
    case FL_Up: return "ArrowUp";
    case FL_Right: return "ArrowRight";
    case FL_Down: return "ArrowDown";

    // Modifiers
    case FL_Shift_L: return "ShiftLeft";
    case FL_Shift_R: return "ShiftRight";
    case FL_Control_L: return "ControlLeft";
    case FL_Control_R: return "ControlRight";
    case FL_Caps_Lock: return "CapsLock";
    case FL_Alt_L: return "AltLeft";
    case FL_Alt_R: return "AltRight";
    case FL_Meta_L: return "MetaLeft";
    case FL_Meta_R: return "MetaRight";
    case FL_Alt_Gr: return "AltGraph";
    case FL_Menu: return "ContextMenu";
    case FL_Help: return "Help";

    // Numpad
    case FL_Num_Lock: return "NumLock";
    case FL_KP_Enter: return "NumpadEnter";

    // Media and Special Keys (PUA / XFree86)
    case FL_Stop: return "BrowserStop";
    case FL_Refresh: return "BrowserRefresh";
    case FL_Sleep: return "Sleep";
    case FL_Favorites: return "BrowserFavorites";
    case FL_Search: return "BrowserSearch";
    case FL_Home_Page: return "BrowserHome";
    case FL_Back: return "BrowserBack";
    case FL_Forward: return "BrowserForward";
    case FL_Mail: return "LaunchMail";
    case FL_Media_Play: return "MediaPlayPause";
    case FL_Media_Stop: return "MediaStop";
    case FL_Media_Prev: return "MediaTrackPrevious";
    case FL_Media_Next: return "MediaTrackNext";
    case FL_Volume_Up: return "AudioVolumeUp";
    case FL_Volume_Down: return "AudioVolumeDown";
    case FL_Volume_Mute: return "AudioVolumeMute";

    // International keys
    case FL_Yen: return "IntlYen";

    default:
        // Handle ranged keys (Numpad digits and Function keys)
        if (key >= (FL_KP + '0') && key <= (FL_KP + '9')) {
            static char buf[10];
            snprintf(buf, sizeof(buf), "Numpad%d", key - FL_KP - '0');
            return buf;
        }
        if (key >= FL_F && key <= FL_F_Last) {
            static char buf[5];
            snprintf(buf, sizeof(buf), "F%d", key - FL_F);
            return buf;
        }
        // Numpad operators are also in a range
        if (key >= FL_KP && key <= FL_KP_Last) {
            switch (key - FL_KP) {
            case '*': return "NumpadMultiply";
            case '+': return "NumpadAdd";
            case '-': return "NumpadSubtract";
            case '.': return "NumpadDecimal";
            case '/': return "NumpadDivide";
            case '=': return "NumpadEqual";
            }
        }
        return "Unidentified";
    }
}
