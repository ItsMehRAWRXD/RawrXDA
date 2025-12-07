#include "hotkey.h"
#include <windows.h>
namespace TinyHome::Hotkey {
    bool registerKeys(HWND h) {
        return RegisterHotKey(h, 1, MOD_CONTROL | MOD_SHIFT, 'H');
    }
}
