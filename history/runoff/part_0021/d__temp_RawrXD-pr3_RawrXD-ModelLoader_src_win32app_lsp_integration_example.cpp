// LSP Integration Implementation Guide
// RawrXD Language Features Engine - Production Integration
// 
// This guide shows how to integrate the JSON parser, LSP message parser,
// and provider system into your language server integration.

#include "lsp_json_parser.h"
#include "lsp_message_parser.h"
#include "lsp_extended_providers.h"
#include <iostream>
#include <chrono>
#include <cstring>

void RawrXD_OnInitialize(LspMessage* msg);
void RawrXD_OnCompletion(LspMessage* msg);
void RawrXD_OnHover(LspMessage* msg);
void RawrXD_OnDefinition(LspMessage* msg);
void RawrXD_OnRename(LspMessage* msg);
void RawrXD_OnSignatureHelp(LspMessage* msg);
void RawrXD_OnInlayHint(LspMessage* msg);
void RawrXD_OnResponse(LspMessage* msg);
void RawrXD_SendResponse(uint32_t id, const std::string& result);
void RawrXD_SendError(uint32_t id, int32_t code, const std::string& message);

//==============================================================================
// PHASE 1: Initialize Infrastructure (at startup)
//==============================================================================

void RawrXD_LspInitialize() {
    std::cout << "[LSP] Initializing language features infrastructure..." << std::endl;
    
    // Initialize global JSON parser allocators
    auto result = JsonParser_GlobalInit();
    if (!result) {
        std::cerr << "[ERROR] Failed to initialize JSON parser" << std::endl;
        return;
    }
    std::cout << "[LSP] JSON parser initialized" << std::endl;
    
    // Initialize LSP client infrastructure would go here
    // E.g., LspClient_Initialize(), provider registry setup, etc.
}

//==============================================================================
// PHASE 2: Receive and Parse LSP Messages (main loop)
//==============================================================================

void RawrXD_ProcessLspMessage(const uint8_t* buffer, size_t bufferLen) {
    // This would be called from LspClient_ReceiveLoop or a network thread
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Parse JSON-RPC message
    LspMessageParser parser(buffer, bufferLen);
    if (!parser.Parse()) {
        std::cerr << "[ERROR] Failed to parse LSP message" << std::endl;
        std::cerr << "  Stats: " << LspMessageParser::FormatStats() << std::endl;
        return;
    }
    
    auto parseTime = std::chrono::high_resolution_clock::now();
    auto parseMs = std::chrono::duration_cast<std::chrono::microseconds>(
        parseTime - startTime).count();
    
    LspMessage* msg = parser.GetMessage();
    
    std::cout << "[LSP] Received message:" << std::endl;
    std::cout << "  Type: " << (parser.IsRequest() ? "Request" : 
                                  parser.IsResponse() ? "Response" : "Notification")
              << std::endl;
    std::cout << "  ID: " << msg->Id << std::endl;
    std::cout << "  Method: " << parser.GetMethod() << std::endl;
    std::cout << "  Parse time: " << parseMs << " us" << std::endl;
    
    if (parser.HasError()) {
        std::cerr << "  ERROR " << msg->ErrorCode << ": " 
                  << parser.GetErrorMessage() << std::endl;
        return;
    }
    
    // Dispatch to handler based on method
    if (parser.IsRequest() || parser.IsNotification()) {
        std::string method = parser.GetMethod();
        
        if (method == "initialize") {
            RawrXD_OnInitialize(msg);
        } else if (method == "textDocument/completion") {
            RawrXD_OnCompletion(msg);
        } else if (method == "textDocument/hover") {
            RawrXD_OnHover(msg);
        } else if (method == "textDocument/definition") {
            RawrXD_OnDefinition(msg);
        } else if (method == "textDocument/rename") {
            RawrXD_OnRename(msg);
        } else if (method == "textDocument/signatureHelp") {
            RawrXD_OnSignatureHelp(msg);
        } else if (method == "textDocument/inlayHint") {
            RawrXD_OnInlayHint(msg);
        } else {
            std::cerr << "[WARNING] Unknown method: " << method << std::endl;
        }
    } else if (parser.IsResponse()) {
        // Handle response to our request
        RawrXD_OnResponse(msg);
    }
}

//==============================================================================
// PHASE 3: Implement Request Handlers (example: Completion)
//==============================================================================

void RawrXD_OnCompletion(LspMessage* msg) {
    std::cout << "[LSP] Processing completion request ID=" << msg->Id << std::endl;
    
    // Params are already a JSON tree
    if (!msg->Params) {
        std::cerr << "[ERROR] No params in completion request" << std::endl;
        RawrXD_SendError(msg->Id, -32602, "Missing params");
        return;
    }
    
    // Extract fields using zero-copy API
    std::string documentUri;
    JsonValue* textDocument = Json_FindString(msg->Params, L"textDocument");
    const char* uriPtr = nullptr;
    uint64_t uriLen = 0;
    if (!textDocument || !Json_GetStringField(textDocument, L"uri", &uriPtr, &uriLen)) {
        std::cerr << "[ERROR] Missing textDocument.uri" << std::endl;
        RawrXD_SendError(msg->Id, -32602, "Missing textDocument.uri");
        return;
    }
    documentUri.assign(uriPtr, uriLen);
    
    JsonValue* position = Json_FindString(msg->Params, L"position");
    int64_t line = position ? Json_GetIntField(position, L"line") : 0;
    int64_t character = position ? Json_GetIntField(position, L"character") : 0;
    
    std::cout << "[LSP] Completion at " << documentUri 
              << ":" << line << ":" << character << std::endl;
    
    // TODO: Call language-specific completion provider
    // E.g., Provider_GetMatching(PROVIDER_TYPE_COMPLETION, langId, uri, ...)
    
    // Send back completion list (simplified)
    std::string response = R"({
        "jsonrpc": "2.0",
        "id": )" + std::to_string(msg->Id) + R"(,
        "result": {
            "items": [
                {"label": "item1", "kind": 1},
                {"label": "item2", "kind": 1}
            ],
            "isIncomplete": false
        }
    })";
    
    RawrXD_SendResponse(msg->Id, response);
}

void RawrXD_OnRename(LspMessage* msg) {
    std::cout << "[LSP] Processing rename request ID=" << msg->Id << std::endl;
    
    if (!msg->Params) {
        RawrXD_SendError(msg->Id, -32602, "Missing params");
        return;
    }
    
    // Similar to completion: parse params, extract fields, process
    // TODO: Implement rename logic
}

void RawrXD_OnSignatureHelp(LspMessage* msg) {
    std::cout << "[LSP] Processing signature help request ID=" << msg->Id << std::endl;
    
    // TODO: Implement signature help
}

void RawrXD_OnInlayHint(LspMessage* msg) {
    std::cout << "[LSP] Processing inlay hint request ID=" << msg->Id << std::endl;
    
    // TODO: Implement inlay hint provider
}

void RawrXD_OnInitialize(LspMessage* msg) {
    std::cout << "[LSP] Server initialization" << std::endl;
    
    // Send initialization response with capabilities
    std::string response = R"({
        "jsonrpc": "2.0",
        "id": )" + std::to_string(msg->Id) + R"(,
        "result": {
            "capabilities": {
                "completionProvider": {"resolveProvider": true},
                "hoverProvider": true,
                "definitionProvider": true,
                "renameProvider": true,
                "signatureHelpProvider": {"triggerCharacters": ["(", ","]},
                "inlayHintProvider": true
            }
        }
    })";
    
    RawrXD_SendResponse(msg->Id, response);
}

void RawrXD_OnResponse(LspMessage* msg) {
    std::cout << "[LSP] Received response ID=" << msg->Id << std::endl;
    
    // Look up pending request and call its callback
    // This is typically done by the LSP client framework
}

void RawrXD_OnHover(LspMessage* msg) {
    std::cout << "[LSP] Processing hover request ID=" << msg->Id << std::endl;
    // TODO: Implement
}

void RawrXD_OnDefinition(LspMessage* msg) {
    std::cout << "[LSP] Processing definition request ID=" << msg->Id << std::endl;
    // TODO: Implement
}

//==============================================================================
// PHASE 4: Send Responses Back to Client
//==============================================================================

void RawrXD_SendResponse(uint32_t id, const std::string& result) {
    std::string response = R"({
        "jsonrpc": "2.0",
        "id": )" + std::to_string(id) + R"(,
        "result": )" + result + R"(
    })";
    
    // Send via socket/IPC
    // LspClient_SendRawMessage(response.data(), response.size());
    std::cout << "[LSP] Sent response: " << response.substr(0, 100) << "..." << std::endl;
}

void RawrXD_SendError(uint32_t id, int32_t code, const std::string& message) {
    std::string response = R"({
        "jsonrpc": "2.0",
        "id": )" + std::to_string(id) + R"(,
        "error": {
            "code": )" + std::to_string(code) + R"(,
            "message": ")" + message + R"("
        }
    })";
    
    std::cout << "[ERROR] Sent error response: " << response << std::endl;
}

//==============================================================================
// PHASE 5: Example Extension Implementation
//==============================================================================

class RawrXDLanguageExtension : public LanguageExtension {
private:
    class CompletionProvider : public RenameProvider {
    public:
        WorkspaceEdit* Provide(
            const std::string& documentUri,
            uint32_t line,
            uint32_t character,
            const std::string& newName) override {
            
            std::cout << "[Extension] Rename " << newName 
                      << " at " << documentUri << std::endl;
            
            auto* edit = new WorkspaceEdit();
            // Populate with actual rename edits
            return edit;
        }
    };
    
    CompletionProvider m_completionProvider;
    
public:
    void Activate(ExtensionContext& context) override {
        std::cout << "[Extension] Activating RawrXD language extension" << std::endl;
        
        // Register providers with language server
        // This maps to the internal MASM Provider_Register function
        vscode::registerRenameProvider(L"rawrxd", &m_completionProvider);
        
        std::cout << "[Extension] Providers registered" << std::endl;
    }
};

//==============================================================================
// PRODUCTION MONITORING & INSTRUMENTATION
//==============================================================================

void RawrXD_PrintStats() {
    auto stats = LspMessageParser::GetStats();
    
    std::cout << "\n=== LSP Parse Statistics ===" << std::endl;
    std::cout << "  Success: " << stats.SuccessCount << std::endl;
    std::cout << "  Failed:  " << stats.FailCount << std::endl;
    std::cout << "  Bytes:   " << stats.TotalBytesRead << std::endl;
    std::cout << "  Max depth: " << stats.MaxDepth << std::endl;
    std::cout << "  Max fields: " << stats.MaxFieldCount << std::endl;
    std::cout << "  Avg parse: " << stats.AvgParseTimeUs << " us" << std::endl;
    
    if (stats.LastErrorCode != 0) {
        std::cout << "  Last error: " << stats.LastErrorCode << std::endl;
    }
}

//==============================================================================
// MAIN ENTRY POINT
//==============================================================================

int main() {
    std::cout << "[RawrXD] Language Features Engine Starting" << std::endl;
    
    // Initialize
    RawrXD_LspInitialize();
    
    // Example: simulate receiving a message
    std::string simulatedMessage = R"({
        "jsonrpc": "2.0",
        "id": 1,
        "method": "initialize",
        "params": {}
    })";
    
    RawrXD_ProcessLspMessage(
        reinterpret_cast<const uint8_t*>(simulatedMessage.data()),
        simulatedMessage.size()
    );
    
    // Print statistics
    RawrXD_PrintStats();
    
    std::cout << "[RawrXD] Language Features Engine Ready" << std::endl;
    
    return 0;
}
