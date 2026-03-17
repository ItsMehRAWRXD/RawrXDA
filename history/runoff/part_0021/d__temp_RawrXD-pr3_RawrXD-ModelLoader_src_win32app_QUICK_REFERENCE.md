# RawrXD Language Features - Quick Reference

## 📋 Quick Start (5 minutes)

### Initialize (Once at Startup)
```cpp
#include "lsp_json_parser.h"
#include "lsp_message_parser.h"

int main() {
    JsonParser_GlobalInit();  // Set up slabs
    // ... rest of setup
}
```

### Parse Incoming LSP Message
```cpp
const uint8_t* buffer = receive_from_server();
size_t bufLen = ...;

LspMessageParser parser(buffer, bufLen);
if (!parser.Parse()) {
    log_error(parser.GetErrorMessage());
    return;
}

LspMessage* msg = parser.GetMessage();
```

### Handle Request
```cpp
if (parser.IsRequest()) {
    std::string method = parser.GetMethod();
    uint32_t id = parser.GetId();
    
    if (method == "textDocument/completion") {
        handle_completion(msg, id);
    } else if (method == "textDocument/rename") {
        handle_rename(msg, id);
    }
}
```

### Extract Parameters (Zero-Copy)
```cpp
// Parse params JSON
if (msg->Params) {
    JsonParserWrapper params(msg->Params, strlen(...));
    JsonValue* root = params.Parse();
    
    // Get fields - NO allocation
    std::string uri;
    params.GetStringField(L"textDocument.uri", uri);
    
    int64_t line = params.GetIntField(L"position.line");
    int64_t char = params.GetIntField(L"position.character");
}
```

### Send Response
```cpp
std::string response = R"({
    "jsonrpc": "2.0",
    "id": )" + std::to_string(id) + R"(,
    "result": {
        "items": [
            {"label": "completion1", "kind": 1},
            {"label": "completion2", "kind": 1}
        ]
    }
})";

send_to_client(response);
```

## 🏗️ Architecture at a Glance

```
Network Buffer
    ↓
JsonParser (tokenize + tree)
    ↓
LspMessage (validate + extract)
    ↓
Handler (completion/hover/rename/etc)
    ↓
Response JSON
    ↓
Network Send
```

## 📚 Key APIs

### JSON Parser
```cpp
// Initialization
JsonParser_GlobalInit()

// Parsing
JsonValue* JsonParser_ParseValue(JsonParser*)
void JsonParser_NextToken(JsonParser*)

// Field access (zero-copy)
JsonValue* Json_FindString(JsonValue* root, const wchar_t* fieldName)
uint64_t Json_GetStringField(JsonValue*, const wchar_t*, const char**, uint64_t*)
int64_t Json_GetIntField(JsonValue*, const wchar_t*)
uint64_t Json_GetBoolField(JsonValue*, const wchar_t*)
```

### LSP Message Parser
```cpp
// Main entry point
LspMessage* LspMessage_ParseFromJson(const uint8_t* buffer, size_t len)

// Validation
MessageValidation* LspMessage_Validate(LspMessage*)

// Statistics
ParseStats* LspMessage_GetStats()
void LspMessage_ResetStats()
```

### Provider Registration
```cpp
// Register completion
registerCompletionItemProvider(L"cpp", &myProvider, L".");

// Register rename
registerRenameProvider(L"cpp", &myRenameProvider);

// Register signature help
registerSignatureHelpProvider(L"cpp", &mySignatureProvider, L"(,");

// Register inlay hints
registerInlayHintsProvider(L"cpp", &myHintProvider);
```

## 🧮 Message Types

### Request
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "textDocument/completion",
    "params": { ... }
}
```

### Response
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "result": { ... }
}
```

### Error Response
```json
{
    "jsonrpc": "2.0",
    "id": 1,
    "error": {
        "code": -32600,
        "message": "Invalid request"
    }
}
```

### Notification (no response)
```json
{
    "jsonrpc": "2.0",
    "method": "textDocument/didOpen",
    "params": { ... }
}
```

## 🎯 Common LSP Methods

| Method | Purpose | Provider |
|--------|---------|----------|
| `textDocument/completion` | Auto-complete suggestions | CompletionProvider |
| `textDocument/hover` | Info on hover | HoverProvider |
| `textDocument/definition` | Go to definition | DefinitionProvider |
| `textDocument/rename` | **NEW** Rename identifier | RenameProvider |
| `textDocument/signatureHelp` | **NEW** Function signatures | SignatureHelpProvider |
| `textDocument/inlayHint` | **NEW** Type hints | InlayHintProvider |
| `textDocument/references` | Find all usages | ReferenceProvider |
| `textDocument/documentSymbol` | List symbols in file | DocumentSymbolProvider |
| `textDocument/codeAction` | Quick fixes | CodeActionProvider |

## 💾 Memory Model

- **Slab Allocator** - Pre-allocated pools
  - JSON Parser: 256KB
  - JSON Trees: 1MB
  - Messages: 1MB (from main allocator)
- **No malloc/free** - O(1) deterministic allocation
- **Zero-copy strings** - Pointers into original buffer
- **Max limits:**
  - JSON depth: 32 levels
  - String length: 64KB
  - Message size: 1MB

## 🔍 Error Codes

### JSON Parser Errors
- `0` = None
- `1` = Unexpected character
- `2` = Unterminated string
- `3` = Invalid number
- `4` = Max depth exceeded (32 levels)
- `5` = Invalid escape sequence
- `6` = Trailing comma

### LSP Protocol Errors
- `-32700` = Parse error
- `-32600` = Invalid request
- `-32601` = Method not found
- `-32602` = Invalid params
- `-32603` = Internal error
- `-32000` to `-32099` = Server errors

## 📊 Instrumentation

```cpp
// Get statistics
auto stats = LspMessageParser::GetStats();

printf("Success: %llu\n", stats.SuccessCount);
printf("Failed: %llu\n", stats.FailCount);
printf("Bytes: %llu\n", stats.TotalBytesRead);
printf("Max depth: %u\n", stats.MaxDepth);
printf("Max fields: %u\n", stats.MaxFieldCount);
printf("Avg parse: %u us\n", stats.AvgParseTimeUs);

// Reset for next session
LspMessageParser::ResetStats();
```

## ✅ Checklist: Adding a New Provider

1. Define provider struct:
```cpp
class MyProvider {
public:
    void* Provide(const std::string& uri, uint32_t line, uint32_t char);
    void Resolve(void* item);
};
```

2. Register in Activate():
```cpp
void Activate(ExtensionContext& ctx) override {
    vscode::registerMyProvider(L"mylang", &myProvider);
}
```

3. Handle in dispatch:
```cpp
if (method == "textDocument/myfeature") {
    handle_my_feature(msg);
}
```

4. Extract params and call provider:
```cpp
void handle_my_feature(LspMessage* msg) {
    // Parse params
    // Call provider.Provide()
    // Send response
}
```

## 🧪 Testing Template

```cpp
#include "gtest/gtest.h"
#include "lsp_message_parser.h"

TEST(LspParser, ParseCompletion) {
    const char* json = R"({
        "jsonrpc": "2.0",
        "id": 1,
        "method": "textDocument/completion",
        "params": {
            "textDocument": {"uri": "file:///test.cpp"},
            "position": {"line": 5, "character": 10}
        }
    })";
    
    LspMessageParser parser((const uint8_t*)json, strlen(json));
    EXPECT_TRUE(parser.Parse());
    EXPECT_EQ("textDocument/completion", parser.GetMethod());
    EXPECT_EQ(1, parser.GetId());
}
```

## 🚀 Performance Tips

1. **Reuse parsers** - Don't create new ones per message
2. **Query fields once** - Cache results if accessed multiple times
3. **Check statistics** - Profile with `g_LspParseStats`
4. **Pre-allocate buffers** - For response building
5. **Use zero-copy** - Extract string pointers, not copies

## 🐛 Common Issues

**"Parse failed: Unterminated string"**
- Check for unescaped quotes in JSON string values
- Ensure all `\` are escaped as `\\`

**"Unknown method"**
- Verify method name matches LSP spec
- Check caps (JSON-RPC is case-sensitive)

**"Max depth exceeded"**
- Deeply nested JSON (>32 levels)
- Likely malformed or very large document
- Increase limit in lsp_json_parser.asm if needed

**Memory growth**
- Check that handlers properly free responses
- Verify slab isn't leaking (should be O(1))

## 📖 Files Reference

| File | Purpose |
|------|---------|
| `lsp_json_parser.asm` | JSON tokenizer + tree builder |
| `lsp_json_parser.h` | JSON C API |
| `lsp_message_parser.asm` | JSON→LSP bridge |
| `lsp_message_parser.h` | LSP C++ wrapper |
| `lsp_extended_providers.asm` | Rename/Signature/InlayHint |
| `lsp_extended_providers.h` | Provider interfaces |
| `lsp_integration_example.cpp` | Full example |
| `LSP_INTEGRATION_GUIDE.md` | Architecture guide |

---

**Tip:** Start with `lsp_integration_example.cpp` to see the full flow!
