#include "Win32IDE_PeekOverlay.cpp"
#include <iostream>

int main() {
    std::cout << "Peek Overlay implementation test" << std::endl;

    // Test that the classes can be instantiated
    Win32IDE ide(nullptr, nullptr, 0);

    // Test findDefinitionsAt and findReferencesAt
    std::vector<PeekItem> defs = ide.findDefinitionsAt(1, 1);
    std::vector<PeekItem> refs = ide.findReferencesAt(1, 1);

    std::cout << "Definitions found: " << defs.size() << std::endl;
    std::cout << "References found: " << refs.size() << std::endl;

    return 0;
}