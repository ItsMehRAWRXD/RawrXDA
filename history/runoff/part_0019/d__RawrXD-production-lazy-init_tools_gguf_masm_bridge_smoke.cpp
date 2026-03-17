#include <windows.h>
#include <iostream>
#include "gguf_robust_masm_bridge.hpp"

int main() {
    auto& bridge = rawrxd::gguf::masm::Bridge::Instance();
    bool loaded = bridge.EnsureLoaded();
    std::cout << "MASM bridge loaded: " << (loaded ? "yes" : "no") << std::endl;
    return loaded ? 0 : 1;
}
