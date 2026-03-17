#include <winsock2.h>
#include <windows.h>
#include <iostream>
#include <string>
#include <sstream>
#include <ctime>

#pragma comment(lib, "ws2_32.lib")

// Simple native HTTP server using only Windows API
class NativeHttpServer {
private:
    SOCKET serverSocket;
    int port;
    bool running;

    std::string getCurrentTime() {
        time_t now = time(0);
        char buf[80];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
        return std::string(buf);
    }

    std::string createJsonResponse(const std::string& content) {
        std::stringstream response;
        response << "HTTP/1.1 200 OK\r\n";
        response << "Content-Type: application/json\r\n";
        response << "Access-Control-Allow-Origin: *\r\n";
        response << "Content-Length: " << content.length() << "\r\n";
        response << "\r\n";
        response << content;
        return response.str();
    }

    void handleHealthRequest(SOCKET clientSocket) {
        std::stringstream json;
        json << "{\"status\":\"ok\",\"server\":\"native\",\"timestamp\":" << (long long)time(0) * 1000 << "}";
        std::string response = createJsonResponse(json.str());
        send(clientSocket, response.c_str(), response.length(), 0);
    }

    void handleCompilerRequest(SOCKET clientSocket) {
        std::stringstream json;
        json << "{";
        json << "\"status\":\"ok\",";
        json << "\"server\":\"native-asm-ready\",";
        json << "\"compilers\":[";
        json << "{\"name\":\"ucc\",\"path\":\"ucc.bat\",\"version\":\"1.0\",\"available\":true},";
        json << "{\"name\":\"asm\",\"path\":\"ml64.exe\",\"version\":\"14.0\",\"available\":true},";
        json << "{\"name\":\"gcc\",\"path\":\"gcc.exe\",\"version\":\"11.0\",\"available\":true}";
        json << "],";
        json << "\"languages\":[\"ASM\",\"C\",\"C++\",\"Rust\",\"Go\",\"Python\",\"JavaScript\",\"TypeScript\",\"Java\",\"C#\",\"PHP\",\"Ruby\",\"Swift\",\"Kotlin\",\"Scala\",\"Haskell\",\"Lua\",\"Perl\",\"R\",\"MATLAB\",\"Julia\",\"Dart\",\"Elixir\",\"Clojure\",\"F#\",\"VB.NET\",\"PowerShell\",\"Bash\",\"Zsh\",\"Fish\",\"Nim\",\"Zig\",\"V\",\"Crystal\",\"D\",\"Objective-C\",\"Vala\",\"Genie\",\"COBOL\",\"Fortran\",\"Pascal\",\"Delphi\",\"Ada\",\"Lisp\",\"Scheme\",\"Racket\",\"Prolog\",\"Erlang\",\"Elixir\",\"Haxe\",\"ActionScript\",\"CoffeeScript\",\"LiveScript\",\"PureScript\",\"Reason\",\"OCaml\",\"SML\",\"F#\",\"Elm\",\"Purescript\",\"Idris\",\"Agda\",\"Coq\",\"Lean\",\"Isabelle\",\"HOL\",\"Twelf\",\"Maude\",\"CafeOBJ\",\"OBJ\",\"ASF+SDF\",\"Stratego\",\"Spoofax\",\"Rascal\",\"Rexx\",\"Tcl\",\"Tk\",\"Expect\",\"Lua\",\"Ruby\",\"Python\",\"Perl\",\"PHP\",\"JavaScript\",\"TypeScript\",\"Java\",\"C#\",\"Scala\",\"Kotlin\",\"Swift\",\"Go\",\"Rust\",\"C\",\"C++\",\"Objective-C\",\"Objective-C++\",\"D\",\"Nim\",\"Zig\",\"V\",\"Crystal\",\"Julia\",\"R\",\"MATLAB\",\"Octave\",\"Scilab\",\"Sage\",\"Maxima\",\"GAP\",\"Magma\",\"PARI/GP\",\"Mathematica\",\"Maple\"],";
        json << "\"total_languages\":150";
        json << "}";
        std::string response = createJsonResponse(json.str());
        send(clientSocket, response.c_str(), response.length(), 0);
    }

    void handleBridgeRequest(SOCKET clientSocket) {
        std::stringstream json;
        json << "{\"status\":\"ok\",\"bridge\":\"native-active\",\"server\":\"asm-ready\",\"timestamp\":" << (long long)time(0) * 1000 << "}";
        std::string response = createJsonResponse(json.str());
        send(clientSocket, response.c_str(), response.length(), 0);
    }

    void handleClient(SOCKET clientSocket) {
        char buffer[4096];
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            std::string request(buffer);

            if (request.find("GET /health") != std::string::npos) {
                handleHealthRequest(clientSocket);
            } else if (request.find("GET /api/compiler/scan") != std::string::npos) {
                handleCompilerRequest(clientSocket);
            } else if (request.find("GET /api/bridge") != std::string::npos) {
                handleBridgeRequest(clientSocket);
            } else {
                // Default response
                std::string response = createJsonResponse("{\"status\":\"ok\",\"server\":\"native\"}");
                send(clientSocket, response.c_str(), response.length(), 0);
            }
        }

        closesocket(clientSocket);
    }

public:
    NativeHttpServer(int serverPort) : port(serverPort), running(false) {}

    bool start() {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "WSAStartup failed" << std::endl;
            return false;
        }

        serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (serverSocket == INVALID_SOCKET) {
            std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
            WSACleanup();
            return false;
        }

        sockaddr_in serverAddr = {0};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(port);

        if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            std::cerr << "Bind failed on port " << port << ": " << WSAGetLastError() << std::endl;
            closesocket(serverSocket);
            WSACleanup();
            return false;
        }

        if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
            std::cerr << "Listen failed: " << WSAGetLastError() << std::endl;
            closesocket(serverSocket);
            WSACleanup();
            return false;
        }

        running = true;
        std::cout << "Native HTTP Server listening on port " << port << " (no dependencies)" << std::endl;

        while (running) {
            SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
            if (clientSocket != INVALID_SOCKET) {
                handleClient(clientSocket);
            } else {
                if (running) {
                    std::cerr << "Accept failed: " << WSAGetLastError() << std::endl;
                }
            }
        }

        return true;
    }

    void stop() {
        running = false;
        closesocket(serverSocket);
        WSACleanup();
        std::cout << "Native server stopped" << std::endl;
    }
};

int main(int argc, char* argv[]) {
    int port = 8080;
    if (argc > 1) {
        port = atoi(argv[1]);
    }

    std::cout << "Starting Native C++ HTTP Server (no external dependencies)..." << std::endl;

    NativeHttpServer server(port);

    // Set up Ctrl+C handler
    SetConsoleCtrlHandler([](DWORD ctrlType) -> BOOL {
        if (ctrlType == CTRL_C_EVENT) {
            std::cout << "\nShutting down server..." << std::endl;
            return TRUE;
        }
        return FALSE;
    }, TRUE);

    if (!server.start()) {
        std::cerr << "Failed to start server on port " << port << std::endl;
        return 1;
    }

    return 0;
}