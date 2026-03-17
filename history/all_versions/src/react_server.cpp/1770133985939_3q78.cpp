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
#include "vsix_loader.h"
#include "hot_patcher.h"
#include "memory_core.h"    

#pragma comment(lib, "ws2_32.lib")

// Global Feature Managers
static VSIXLoader g_vsix_loader;
static HotPatcher g_hot_patcher;

// Simple JSON parser stub - in a real app use nlohmann/json

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
        if (contextLimit == 0) contextLimit = 4096;

        set_mode(mode);
        set_deep_thinking(deepThinking);
        set_deep_research(deepResearch);
        set_no_refusal(noRefusal);
        set_context(contextLimit);

        std::string response = process_prompt(prompt);
        std::string safe_response = json_escape(response);
        
        std::string json_resp = "{\"response\": \"" + safe_response + "\"}";
        http_response(s, "application/json", json_resp);
        return;
    }

    if (path == "/api/model/load") {
        std::string modelPath = get_json_string(body, "path");
        runtime_load_model(modelPath);
        http_response(s, "application/json", "{\"status\": \"loading_initiated\"}");
        return; 
    }

    if (path == "/api/plugins/load") {
        std::string vsixPath = get_json_string(body, "path");
        bool res = g_vsix_loader.LoadPlugin(vsixPath);
        http_response(s, "application/json", res ? "{\"status\": \"success\"}" : "{\"status\": \"error\"}");
        return;
    }

    if (path == "/api/patch/apply") {
        // Very basic parsing for demo
        std::string patchName = get_json_string(body, "name");
        // In real scenario, parse address/bytes
        http_response(s, "application/json", "{\"status\": \"patch_received\", \"name\": \"" + patchName + "\"}");
        return;
    }
    
    if (path == "/api/memory/status") {
        http_response(s, "application/json", "{\"status\": \"active\", \"tier\": \"TIER_4K\"}");
        return;
    }

    if (path == "/api/engine/load" || path == "/engine/load") {
        std::string engine = get_json_string(body, "engine");
        set_engine(engine);
        http_response(s, "application/json", "{\"success\": true}");
        return;
    }

    if (path == "/api/engines" || path == "/engines") {
        std::string active = get_active_engine_name();
        std::string json = "{\"engines\": [\"" + active + "\", \"RawrXD-AVX512\"]}";
        http_response(s, "application/json", json);
        return;
    }

    if (path == "/api/memory/usage" || path == "/memory/usage") {
        size_t usage = g_memory_system.GetUsage(); 
        size_t limit = g_memory_system.GetCapacity();
        float util = g_memory_system.GetUtilizationPercentage();
        auto history = g_memory_system.GetRecentHistory(5);
        
        std::string histJson = "[";
        for(size_t i=0; i<history.size(); i++) {
            histJson += "\"" + json_escape(history[i]) + "\"";
            if(i < history.size()-1) histJson += ",";
        }
        histJson += "]";

        std::string json = "{\"usage\": " + std::to_string(usage) + 
                           ", \"limit\": " + std::to_string(limit) + 
                           ", \"utilization\": " + std::to_string(util) +
                           ", \"history\": " + histJson + "}";
        http_response(s, "application/json", json);
        return;
    }

    if (path == "/api/memory/clear" || path == "/memory/clear") {
        g_memory_system.Wipe(); // or Deallocate? User likely just wants to clear context.
        // If Wipe() just zeroes, but we want to reset counters, we might need Reset().
        // For now let's assume Wipe does enough or we re-init.
        // Actually, MemoryCore::Wipe() zeroes buffer but doesn't reset counters in valid implementation? 
        // Let's assume Deallocate/Allocate loop or specialized Clear() method. 
        // The read of MemoryCore didn't show Clear(), only Wipe().
        // Let's do Deallocate -> Allocate(SameTier) for full flush.
        
        size_t cap = g_memory_system.GetCapacity();
        g_memory_system.Deallocate();
        memory_system_init(cap); // Re-init
        
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
        std::string fpath = get_json_string(body, "path");
        std::string output;
        
        if (fpath.empty()) {
            output = "Usage: Provide 'path' in JSON body.";
        } else {
            output = dump_pe(fpath.c_str());
        }
        
        http_response(s, "application/json", "{\"output\": \"" + json_escape(output) + "\"}");
        return;
    }
    
    if (path == "/api/tools/compile" || path == "/tools/compile") {
        std::string src = get_json_string(body, "source");
        if (src.empty()) {
            http_response(s, "application/json", "{\"success\": false, \"message\": \"No source code provided\"}");
            return;
        }

        std::string result = run_compiler(src.c_str());
        
        // Escape newlines for JSON
        std::string jsonMsg = json_escape(result);
        http_response(s, "application/json", "{\"success\": true, \"message\": \"" + jsonMsg + "\"}");
        return;
    }
    
    if (path == "/api/tools/hotpatch" || path == "/tools/hotpatch") {
        // Expected JSON: { "target": "0x1234ABCD", "opcodes": "909090" } (hex strings)
        std::string targetStr = get_json_string(body, "target");
        std::string opcodesStr = get_json_string(body, "opcodes");

        void* target = ParseHexAddress(targetStr);
        if (!target) {
            http_response(s, "application/json", "{\"success\": false, \"error\": \"Invalid target address\"}");
            return;
        }

        // Convert hex string opcodes to bytes
        std::vector<unsigned char> bytes;
        for (size_t i = 0; i < opcodesStr.length(); i += 2) {
            if (i + 1 >= opcodesStr.length()) break;
            std::string byteString = opcodesStr.substr(i, 2);
            try {
                bytes.push_back((unsigned char)std::stoul(byteString, nullptr, 16));
            } catch(...) {}
        }

        if (bytes.empty()) {
             http_response(s, "application/json", "{\"success\": false, \"error\": \"Invalid or empty opcodes\"}");
             return;
        }

        bool success = g_hot_patcher.ApplyPatch("UserPatch_" + targetStr, target, bytes);
        
        if (success) {
            http_response(s, "application/json", "{\"success\": true, \"message\": \"Patch applied successfully\"}");
        } else {
             http_response(s, "application/json", "{\"success\": false, \"error\": \"Patch application failed (Access Denied?)\"}");
        }
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
