#include "runtime_core.h"
#include <string>

extern "C" __declspec(dllexport)
const char* gui_submit(const char* text) {
    static std::string out;
    out = process_prompt(text ? text : "");
    return out.c_str();
    return true;
}

