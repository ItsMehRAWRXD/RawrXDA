#include "../src/agentic_loop_state.h"
#include "../src/agentic_memory_system.h"
#include <iostream>
int main() {
    AgenticLoopState loop;
    loop.startIteration("test");
    loop.addToMemory("key", "value");
    auto json = loop.serializeState();
    std::cout << "serializeState length: " << json.length() << std::endl;
    std::cout << "serializeState: " << json.substr(0, 200) << "..." << std::endl;
    
    AgenticMemorySystem mem;
    mem.storeMemory(MemoryType::Fact, "test", "test");
    auto memJson = mem.exportState();
    std::cout << "exportState keys: ";
    for (auto& [k,v] : memJson.items()) std::cout << k << " ";
    std::cout << std::endl;
    return 0;
}
