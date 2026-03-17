#include <iostream>
#include <thread>
#include "runtime_core.h"
#include <string>
#include "modules/react_generator.h"

// Declaration for server start function in react_server.cpp (which we just wrote)
void start_server(int port);

int main() {
    init_runtime();
    
    std::cout << "RawrXD Artificial Intelligence Runtime v6.0 [Reverse Engineered Core]" << std::endl;
    std::cout << "Type !help for commands." << std::endl;

    std::string line;
    while (true) {
        std::cout << "rawrxd> ";
        std::getline(std::cin, line);

        if (line == "!quit") break;
        if (line == "!help") {
            std::cout << "Commands:\n"
                      << "  !mode <ask|plan|edit|bugreport|codesuggest>\n"
                      << "  !engine <name> (e.g. sovereign800b)\n"
                      << "  !deep <on|off>\n"
                      << "  !research <on|off>\n"
                      << "  !max <tokens>\n"
                      << "  !generate_ide [path] (Generates React IDE)\n"
                      << "  !server <port> (Starts backend API server)\n";
            continue;
        }
        if (line.starts_with("!mode ")) {
            set_mode(line.substr(6));
             std::cout << "Mode set.\n";
            continue;
        }
        if (line.starts_with("!engine ")) {
            set_engine(line.substr(8));
             std::cout << "Engine switched.\n";
            continue;
        }
        if (line.starts_with("!deep ")) {
            set_deep_thinking(line.substr(6) == "on");
             std::cout << "Deep thinking updated.\n";
            continue;
        }
        if (line.starts_with("!research ")) {
            set_deep_research(line.substr(10) == "on");
             std::cout << "Deep research updated.\n";
            continue;
        }
        if (line.starts_with("!max ")) {
            try {
                set_context(std::stoull(line.substr(5)));
                std::cout << "Context limit updated.\n";
            } catch (...) {}
            continue;
        }
        if (line.starts_with("!generate_ide")) {
            std::string path = "generated_ide";
            if (line.length() > 14) path = line.substr(14);
            
            std::cout << "Generating Full AI IDE at: " << path << " ...\n";
            RawrXD::ReactServerConfig config;
            config.name = "RawrXD-IDE";
            config.port = "3000";
            config.include_ide_features = true;
            config.cpp_backend_port = "8080";
            
            if (RawrXD::ReactServerGenerator::Generate(path, config)) {
                std::cout << "Success! To run:\n"
                          << "  cd " << path << "\n"
                          << "  npm install\n"
                          << "  npm start\n"
                          << "Ensure backend is running with !server 8080\n";
            } else {
                std::cout << "Failed to generate IDE.\n";
            }
            continue;
        }
        if (line.starts_with("!server ")) {
            int port = 8080;
            try { port = std::stoi(line.substr(8)); } catch(...) {}
            std::thread(start_server, port).detach();
            std::cout << "Backend server started on port " << port << "\n";
            continue;
        }


        std::string out = process_prompt(line);
        std::cout << out << "\n";
    }
    return 0;
}
