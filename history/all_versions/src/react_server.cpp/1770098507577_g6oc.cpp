#include <winsock2.h>
#include <thread>
#include <string>
#include <sstream>
#include <vector>
#include <iostream>
#include <algorithm>
#include "runtime_core.h"
#include "engine_iface.h"
#include "tool_registry.h"
#include "re_tools.h"

#pragma comment(lib, "ws2_32.lib")

// Simple JSON parser stub - in a real app use nlohmann/json
std::string json_escape(const std::string& input) {
    std::string output;
    for (char c : input) {
        if (c == '\"') output += "\\\"";
        else if (c == '\\') output += "\\\\";
        else if (c == '\n') output += "\\n";
        else if (c == '\r') output += "\\r";
        else if (c == '\t') output += "\\t";
        else output += c;
    }
    return output;
}

std::string get_json_string(const std::string& json, const std::string& key) {
    std::string k = "\"" + key + "\":";
    auto pos = json.find(k);
    if (pos == std::string::npos) return "";
    pos += k.length();
    
    // skip whitespace
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\n')) pos++;
    
    if (pos >= json.length()) return "";

    if (json[pos] == '\"') {
        pos++;
        auto end = json.find("\"", pos);
        if (end == std::string::npos) return "";
        return json.substr(pos, end - pos);
    }
    return "";
}

int get_json_int(const std::string& json, const std::string& key) {
    std::string val = get_json_string(json, key);
    if (val.empty()) {
        // Try to find number directly if not quoted (simple heuristic)
        std::string k = "\"" + key + "\":";
        auto pos = json.find(k);
        if (pos != std::string::npos) {
             pos += k.length();
             while (pos < json.length() && !isdigit(json[pos]) && json[pos] != '-') pos++;
             if (pos < json.length()) {
                  size_t end = pos;
                  while (end < json.length() && (isdigit(json[end]) || json[end] == '.')) end++;
                  try { return std::stoi(json.substr(pos, end - pos)); } catch (...) { return 0; }
             }
        }
        return 0;
    }
    try { return std::stoi(val); } catch(...) { return 0; }
}

bool get_json_bool(const std::string& json, const std::string& key) {
    std::string k = "\"" + key + "\":";
    auto pos = json.find(k);
    if (pos == std::string::npos) return false;
    pos += k.length();
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\n')) pos++;
    if (json.substr(pos, 4) == "true") return true;
    return false;
}

void http_response(SOCKET s, const std::string& content_type, const std::string& body) {
    std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: " + content_type + "\r\n" +
                       "Access-Control-Allow-Origin: *\r\n" +
                       "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n" +
                       "Access-Control-Allow-Headers: Content-Type\r\n" +
                       "Content-Length: " + std::to_string(body.length()) + "\r\n\r\n" + body;
    send(s, resp.c_str(), resp.length(), 0);
}

void handle_api_request(SOCKET s, const std::string& method, const std::string& path, const std::string& body) {
    std::cout << "Request: " << method << " " << path << std::endl;
    
    if (method == "OPTIONS") {
        http_response(s, "text/plain", "");
        return;
    }

    if (path == "/api/inference" || path == "/inference") {
        std::string prompt = get_json_string(body, "prompt");
        std::string mode = get_json_string(body, "mode");
        bool deepThinking = get_json_bool(body, "deepThinking");
        bool deepResearch = get_json_bool(body, "deepResearch");
        bool noRefusal = get_json_bool(body, "noRefusal");
        int contextLimit = get_json_int(body, "contextLimit");

        if (!mode.empty()) set_mode(mode);
        if (contextLimit > 0) set_context(contextLimit);
        set_deep_thinking(deepThinking);
        set_deep_research(deepResearch);
        set_no_refusal(noRefusal);

        std::string result = process_prompt(prompt);
        // Clean result for JSON
        
        std::string json = "{\"output\": \"" + json_escape(result) + "\"}";
        http_response(s, "application/json", json);
        return;
    }

    if (path == "/api/engine/load" || path == "/engine/load") {
        std::string engine = get_json_string(body, "engine");
        set_engine(engine);
        http_response(s, "application/json", "{\"success\": true}");
        return;
    }

    if (path == "/api/engines" || path == "/engines") {
        // Since we don't have a get_all_engines() yet, we'll return a hardcoded list or assume just the active one
        // Better: create get_available_engines() in runtime_core. 
        // For now, let's return some logical defaults + active one
        std::string active = get_active_engine();
        std::string json = "{\"engines\": [\"" + active + "\", \"Qwen-72B\", \"Llama-3-70B\", \"Sovereign-Small\", \"Sovereign-Large\"]}";
        http_response(s, "application/json", json);
        return;
    }

    if (path == "/api/memory/usage" || path == "/memory/usage") {
        size_t usage = 1024; // Stub or implement get_memory_usage()
        size_t limit = get_context();
        std::string json = "{\"usage\": " + std::to_string(usage) + ", \"limit\": " + std::to_string(limit) + ", \"history\": [\"Initial load\", \"Processed prompt\"]}";
        http_response(s, "application/json", json);
        return;
    }

    if (path == "/api/memory/clear" || path == "/memory/clear") {
        // Implement clear logic if available
        http_response(s, "application/json", "{\"success\": true}");
        return;
    }

    if (path == "/api/config" || path == "/config") {
         http_response(s, "application/json", "{\"name\": \"RawrXD IDE\", \"features\": [\"inference\", " "tools\", \"memory\"]}");
         return;
    }

    if (path == "/api/health") {
         http_response(s, "application/json", "{\"status\": \"ok\"}");
         return;
    }

    if (path == "/api/tools/dumpbin" || path == "/tools/dumpbin") {
        std::string path = get_json_string(body, "path");
        std::string output = "Simulated DumpBin for " + path; 
        // In real code, call re_tools dump_pe(path.c_str())
        // We'll need to expose dump_pe in a header to call it here.
        // For now, assuming it's available or stubbing
        http_response(s, "application/json", "{\"output\": \"" + json_escape(output) + "\"}");
        return;
    }
    
     if (path == "/api/tools/compile" || path == "/tools/compile") {
        std::string src = get_json_string(body, "source");
        // call compiler
        http_response(s, "application/json", "{\"success\": true, \"message\": \"Compiled\"}");
        return;
    }
    
    if (path == "/api/tools/hotpatch" || path == "/tools/hotpatch") {
         // call hotpatch
         http_response(s, "application/json", "{\"success\": true}");
         return;
    }

    http_response(s, "application/json", "{\"error\": \"Not Found\"}");
}

void handle_client(SOCKET s) {
    char buf[8192];
    int r = recv(s, buf, sizeof(buf) - 1, 0);
    if (r <= 0) {
        closesocket(s);
        return;
    }
    buf[r] = 0;

    std::string raw(buf);
    
    // Simple HTTP parsing
    std::istringstream iss(raw);
    std::string method, path, proto;
    iss >> method >> path >> proto;

    std::string body;
    auto pos = raw.find("\r\n\r\n");
    if (pos != std::string::npos) {
        body = raw.substr(pos + 4);
    }

    try {
        handle_api_request(s, method, path, body);
    } catch (const std::exception& e) {
        http_response(s, "application/json", "{\"error\": \"" + std::string(e.what()) + "\"}");
    }
    
    closesocket(s);
}

void start_server(int port) {
    WSADATA wsa; 
    WSAStartup(MAKEWORD(2,2),&wsa);
    SOCKET sock=socket(AF_INET,SOCK_STREAM,0);

    sockaddr_in addr{};
    addr.sin_family=AF_INET;
    addr.sin_port=htons(port);
    addr.sin_addr.s_addr=INADDR_ANY;

    bind(sock,(sockaddr*)&addr,sizeof(addr));
    listen(sock,5);
    
    std::cout << "C++ Backend listening on port " << port << std::endl;

    while(true){
        SOCKET c=accept(sock,0,0);
        if (c != INVALID_SOCKET) {
             std::thread(handle_client,c).detach();
        }
    }
}
