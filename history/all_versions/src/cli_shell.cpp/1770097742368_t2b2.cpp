#include <iostream>
#include "runtime_core.h"
#include <string>

// Simple stub for set_mode/set_engine which are declared in runtime_core.h but implemented maybe in runtime_core.cpp?
// Wait, I need to check if runtime_core.cpp implements them. 
// The prompt's runtime_core.cpp snippet didn't explicitly show them, but the header declares them.
// I will implement them in runtime_core.cpp later if missing, or add simple implementations here if standalone.
// For now, I'll assume they are linked.

int main() {
    init_runtime();

    std::string line;
    while (true) {
        std::cout << "rawrxd> ";
        std::getline(std::cin, line);

        if (line == "!quit") break;
        if (line.starts_with("!mode ")) {
            set_mode(line.substr(6));
            continue;
        }
        if (line.starts_with("!engine ")) {
            set_engine(line.substr(8));
            continue;
        }
        if (line.starts_with("!deep ")) {
            set_deep_thinking(line.substr(6) == "on");
            continue;
        }
        if (line.starts_with("!research ")) {
            set_deep_research(line.substr(10) == "on");
            continue;
        }
        if (line.starts_with("!max ")) {
            try {
                set_context(std::stoull(line.substr(5)));
            } catch (...) {}
            continue;
        }


        std::string out = process_prompt(line);
        std::cout << out << "\n";
    }
    return 0;
}
