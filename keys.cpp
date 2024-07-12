#include "keys.hpp"
#include <gdk/gdkkeysyms.h>

std::string gdk_to_browser_key(guint key) {
    switch (key) {
    // Basic keys
    case GDK_KEY_Escape: return "Escape";
    case GDK_KEY_BackSpace: return "Backspace";
    case GDK_KEY_Tab: return "Tab";
    case GDK_KEY_Return:
    case GDK_KEY_KP_Enter: return "Enter";
    case GDK_KEY_Control_L: return "ControlLeft";
    case GDK_KEY_Control_R: return "ControlRight";
    case GDK_KEY_Shift_L: return "ShiftLeft";
    case GDK_KEY_Shift_R: return "ShiftRight";
    case GDK_KEY_Alt_L: return "AltLeft";
    case GDK_KEY_Alt_R: return "AltRight";
    case GDK_KEY_Caps_Lock: return "CapsLock";
    case GDK_KEY_space: return "Space";

    // Digits and their shifted symbols
    case GDK_KEY_0:
    case GDK_KEY_KP_0:
    case GDK_KEY_parenright: return "Digit0";
    case GDK_KEY_1:
    case GDK_KEY_KP_1:
    case GDK_KEY_exclam: return "Digit1";
    case GDK_KEY_2:
    case GDK_KEY_KP_2:
    case GDK_KEY_at: return "Digit2";
    case GDK_KEY_3:
    case GDK_KEY_KP_3:
    case GDK_KEY_numbersign: return "Digit3";
    case GDK_KEY_4:
    case GDK_KEY_KP_4:
    case GDK_KEY_dollar: return "Digit4";
    case GDK_KEY_5:
    case GDK_KEY_KP_5:
    case GDK_KEY_percent: return "Digit5";
    case GDK_KEY_6:
    case GDK_KEY_KP_6:
    case GDK_KEY_asciicircum: return "Digit6";
    case GDK_KEY_7:
    case GDK_KEY_KP_7:
    case GDK_KEY_ampersand: return "Digit7";
    case GDK_KEY_8:
    case GDK_KEY_KP_8:
    case GDK_KEY_asterisk: return "Digit8";
    case GDK_KEY_9:
    case GDK_KEY_KP_9:
    case GDK_KEY_parenleft: return "Digit9";

    // Letters
    case GDK_KEY_a:
    case GDK_KEY_A: return "KeyA";
    case GDK_KEY_b:
    case GDK_KEY_B: return "KeyB";
    case GDK_KEY_c:
    case GDK_KEY_C: return "KeyC";
    case GDK_KEY_d:
    case GDK_KEY_D: return "KeyD";
    case GDK_KEY_e:
    case GDK_KEY_E: return "KeyE";
    case GDK_KEY_f:
    case GDK_KEY_F: return "KeyF";
    case GDK_KEY_g:
    case GDK_KEY_G: return "KeyG";
    case GDK_KEY_h:
    case GDK_KEY_H: return "KeyH";
    case GDK_KEY_i:
    case GDK_KEY_I: return "KeyI";
    case GDK_KEY_j:
    case GDK_KEY_J: return "KeyJ";
    case GDK_KEY_k:
    case GDK_KEY_K: return "KeyK";
    case GDK_KEY_l:
    case GDK_KEY_L: return "KeyL";
    case GDK_KEY_m:
    case GDK_KEY_M: return "KeyM";
    case GDK_KEY_n:
    case GDK_KEY_N: return "KeyN";
    case GDK_KEY_o:
    case GDK_KEY_O: return "KeyO";
    case GDK_KEY_p:
    case GDK_KEY_P: return "KeyP";
    case GDK_KEY_q:
    case GDK_KEY_Q: return "KeyQ";
    case GDK_KEY_r:
    case GDK_KEY_R: return "KeyR";
    case GDK_KEY_s:
    case GDK_KEY_S: return "KeyS";
    case GDK_KEY_t:
    case GDK_KEY_T: return "KeyT";
    case GDK_KEY_u:
    case GDK_KEY_U: return "KeyU";
    case GDK_KEY_v:
    case GDK_KEY_V: return "KeyV";
    case GDK_KEY_w:
    case GDK_KEY_W: return "KeyW";
    case GDK_KEY_x:
    case GDK_KEY_X: return "KeyX";
    case GDK_KEY_y:
    case GDK_KEY_Y: return "KeyY";
    case GDK_KEY_z:
    case GDK_KEY_Z: return "KeyZ";

    // Function keys
    case GDK_KEY_F1: return "F1";
    case GDK_KEY_F2: return "F2";
    case GDK_KEY_F3: return "F3";
    case GDK_KEY_F4: return "F4";
    case GDK_KEY_F5: return "F5";
    case GDK_KEY_F6: return "F6";
    case GDK_KEY_F7: return "F7";
    case GDK_KEY_F8: return "F8";
    case GDK_KEY_F9: return "F9";
    case GDK_KEY_F10: return "F10";
    case GDK_KEY_F11: return "F11";
    case GDK_KEY_F12: return "F12";

    // Punctuation and symbols (including shifted versions)
    case GDK_KEY_grave:
    case GDK_KEY_asciitilde: return "Backquote";
    case GDK_KEY_minus:
    case GDK_KEY_underscore: return "Minus";
    case GDK_KEY_equal:
    case GDK_KEY_plus: return "Equal";
    case GDK_KEY_bracketleft:
    case GDK_KEY_braceleft: return "BracketLeft";
    case GDK_KEY_bracketright:
    case GDK_KEY_braceright: return "BracketRight";
    case GDK_KEY_backslash:
    case GDK_KEY_bar: return "Backslash";
    case GDK_KEY_semicolon:
    case GDK_KEY_colon: return "Semicolon";
    case GDK_KEY_apostrophe:
    case GDK_KEY_quotedbl: return "Quote";
    case GDK_KEY_comma:
    case GDK_KEY_less: return "Comma";
    case GDK_KEY_period:
    case GDK_KEY_greater: return "Period";
    case GDK_KEY_slash:
    case GDK_KEY_question: return "Slash";

    // Numpad
    case GDK_KEY_Num_Lock: return "NumLock";
    case GDK_KEY_KP_Multiply: return "NumpadMultiply";
    case GDK_KEY_KP_Add: return "NumpadAdd";
    case GDK_KEY_KP_Subtract: return "NumpadSubtract";
    case GDK_KEY_KP_Decimal: return "NumpadDecimal";
    case GDK_KEY_KP_Divide: return "NumpadDivide";

    // Navigation keys
    case GDK_KEY_Home: return "Home";
    case GDK_KEY_End: return "End";
    case GDK_KEY_Page_Up: return "PageUp";
    case GDK_KEY_Page_Down: return "PageDown";
    case GDK_KEY_Insert: return "Insert";
    case GDK_KEY_Delete: return "Delete";
    case GDK_KEY_Left: return "ArrowLeft";
    case GDK_KEY_Up: return "ArrowUp";
    case GDK_KEY_Right: return "ArrowRight";
    case GDK_KEY_Down: return "ArrowDown";

    // Miscellaneous keys
    case GDK_KEY_Print: return "PrintScreen";
    case GDK_KEY_Scroll_Lock: return "ScrollLock";
    case GDK_KEY_Pause: return "Pause";
    case GDK_KEY_Super_L: return "MetaLeft";
    case GDK_KEY_Super_R: return "MetaRight";
    case GDK_KEY_Menu: return "ContextMenu";

    default: return "Unidentified";
    }
}
