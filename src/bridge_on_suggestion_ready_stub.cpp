#include <cstdint>

extern "C" void Bridge_OnSuggestionReady(const wchar_t* text, int len) {
    (void)text;
    (void)len;
    // Stub for lanes where ASM UI callback provider is not linked.
}
