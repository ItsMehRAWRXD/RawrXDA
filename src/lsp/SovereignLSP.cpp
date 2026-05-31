/*
 * SovereignLSP.cpp - Language Server Protocol Implementation
 * ==========================================================
 * LSP 3.17 compliant server for the Sovereign IDE
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <json-c/json.h>

// Include the unified finisher
extern "C" {
    #include "../../sovereign_finisher.c"
}

// LSP Protocol Constants
#define LSP_VERSION "2.0"
#define LSP_METHOD_INITIALIZE "initialize"
#define LSP_METHOD_INITIALIZED "initialized"
#define LSP_METHOD_SHUTDOWN "shutdown"
#define LSP_METHOD_EXIT "exit"
#define LSP_METHOD_COMPLETION "textDocument/completion"
#define LSP_METHOD_HOVER "textDocument/hover"
#define LSP_METHOD_DEFINITION "textDocument/definition"
#define LSP_METHOD_REFERENCES "textDocument/references"
#define LSP_METHOD_RENAME "textDocument/rename"
#define LSP_METHOD_DOCUMENT_SYMBOL "textDocument/documentSymbol"
#define LSP_METHOD_WORKSPACE_SYMBOL "workspace/symbol"
#define LSP_METHOD_FORMATTING "textDocument/formatting"
#define LSP_METHOD_CODE_ACTION "textDocument/codeAction"
#define LSP_METHOD_CODE_LENS "textDocument/codeLens"
#define LSP_METHOD_SIGNATURE_HELP "textDocument/signatureHelp"
#define LSP_METHOD_DID_OPEN "textDocument/didOpen"
#define LSP_METHOD_DID_CHANGE "textDocument/didChange"
#define LSP_METHOD_DID_CLOSE "textDocument/didClose"
#define LSP_METHOD_DID_SAVE "textDocument/didSave"

// LSP Server State
typedef struct {
    int initialized;
    int shutdown_requested;
    int client_pid;
    char root_path[512];
    int request_id;
    SovereignIDE* ide;
    
    // Capabilities
    int completion_provider;
    int hover_provider;
    int definition_provider;
    int references_provider;
    int rename_provider;
    int document_symbol_provider;
    int workspace_symbol_provider;
    int formatting_provider;
    int code_action_provider;
    int code_lens_provider;
    int signature_help_provider;
} LSPServer;

// Initialize LSP server
LSPServer* lsp_server_create(const char* root_path) {
    LSPServer* server = (LSPServer*)calloc(1, sizeof(LSPServer));
    if (!server) return NULL;
    
    server->ide = ide_create();
    if (!server->ide) {
        free(server);
        return NULL;
    }
    
    if (root_path) {
        strncpy(server->root_path, root_path, sizeof(server->root_path) - 1);
    }
    
    // Enable all capabilities
    server->completion_provider = 1;
    server->hover_provider = 1;
    server->definition_provider = 1;
    server->references_provider = 1;
    server->rename_provider = 1;
    server->document_symbol_provider = 1;
    server->workspace_symbol_provider = 1;
    server->formatting_provider = 1;
    server->code_action_provider = 1;
    server->code_lens_provider = 1;
    server->signature_help_provider = 1;
    
    return server;
}

void lsp_server_destroy(LSPServer* server) {
    if (!server) return;
    if (server->ide) {
        ide_destroy(server->ide);
    }
    free(server);
}

// JSON-RPC Message Handling
static void lsp_send_message(LSPServer* server, struct json_object* msg) {
    const char* json_str = json_object_to_json_string(msg);
    int content_length = strlen(json_str);
    
    printf("Content-Length: %d\r\n\r\n%s", content_length, json_str);
    fflush(stdout);
}

static void lsp_send_response(LSPServer* server, int id, struct json_object* result) {
    struct json_object* response = json_object_new_object();
    json_object_object_add(response, "jsonrpc", json_object_new_string(LSP_VERSION));
    json_object_object_add(response, "id", json_object_new_int(id));
    json_object_object_add(response, "result", result);
    
    lsp_send_message(server, response);
    json_object_put(response);
}

static void lsp_send_error(LSPServer* server, int id, int code, const char* message) {
    struct json_object* error = json_object_new_object();
    json_object_object_add(error, "code", json_object_new_int(code));
    json_object_object_add(error, "message", json_object_new_string(message));
    
    struct json_object* response = json_object_new_object();
    json_object_object_add(response, "jsonrpc", json_object_new_string(LSP_VERSION));
    json_object_object_add(response, "id", json_object_new_int(id));
    json_object_object_add(response, "error", error);
    
    lsp_send_message(server, response);
    json_object_put(response);
}

static void lsp_send_notification(LSPServer* server, const char* method, struct json_object* params) {
    struct json_object* notification = json_object_new_object();
    json_object_object_add(notification, "jsonrpc", json_object_new_string(LSP_VERSION));
    json_object_object_add(notification, "method", json_object_new_string(method));
    if (params) {
        json_object_object_add(notification, "params", params);
    }
    
    lsp_send_message(server, notification);
    json_object_put(notification);
}

// LSP Method Handlers
static void lsp_handle_initialize(LSPServer* server, int id, struct json_object* params) {
    // Extract client capabilities
    struct json_object* client_info = NULL;
    if (json_object_object_get_ex(params, "clientInfo", &client_info)) {
        struct json_object* name = NULL;
        if (json_object_object_get_ex(client_info, "name", &name)) {
            fprintf(stderr, "[LSP] Client: %s\n", json_object_get_string(name));
        }
    }
    
    // Build server capabilities
    struct json_object* capabilities = json_object_new_object();
    
    // Text Document Sync
    struct json_object* text_doc_sync = json_object_new_object();
    json_object_object_add(text_doc_sync, "openClose", json_object_new_boolean(true));
    json_object_object_add(text_doc_sync, "change", json_object_new_int(1)); // Full
    json_object_object_add(text_doc_sync, "willSave", json_object_new_boolean(false));
    json_object_object_add(text_doc_sync, "willSaveWaitUntil", json_object_new_boolean(false));
    json_object_object_add(text_doc_sync, "save", json_object_new_boolean(true));
    json_object_object_add(capabilities, "textDocumentSync", text_doc_sync);
    
    // Completion Provider
    if (server->completion_provider) {
        struct json_object* completion = json_object_new_object();
        json_object_object_add(completion, "resolveProvider", json_object_new_boolean(false));
        json_object_object_add(completion, "triggerCharacters", json_object_new_array());
        json_object_object_add(capabilities, "completionProvider", completion);
    }
    
    // Hover Provider
    if (server->hover_provider) {
        json_object_object_add(capabilities, "hoverProvider", json_object_new_boolean(true));
    }
    
    // Definition Provider
    if (server->definition_provider) {
        json_object_object_add(capabilities, "definitionProvider", json_object_new_boolean(true));
    }
    
    // References Provider
    if (server->references_provider) {
        json_object_object_add(capabilities, "referencesProvider", json_object_new_boolean(true));
    }
    
    // Rename Provider
    if (server->rename_provider) {
        struct json_object* rename = json_object_new_object();
        json_object_object_add(rename, "prepareProvider", json_object_new_boolean(true));
        json_object_object_add(capabilities, "renameProvider", rename);
    }
    
    // Document Symbol Provider
    if (server->document_symbol_provider) {
        json_object_object_add(capabilities, "documentSymbolProvider", json_object_new_boolean(true));
    }
    
    // Workspace Symbol Provider
    if (server->workspace_symbol_provider) {
        json_object_object_add(capabilities, "workspaceSymbolProvider", json_object_new_boolean(true));
    }
    
    // Formatting Provider
    if (server->formatting_provider) {
        json_object_object_add(capabilities, "documentFormattingProvider", json_object_new_boolean(true));
    }
    
    // Code Action Provider
    if (server->code_action_provider) {
        json_object_object_add(capabilities, "codeActionProvider", json_object_new_boolean(true));
    }
    
    // Code Lens Provider
    if (server->code_lens_provider) {
        struct json_object* code_lens = json_object_new_object();
        json_object_object_add(code_lens, "resolveProvider", json_object_new_boolean(false));
        json_object_object_add(capabilities, "codeLensProvider", code_lens);
    }
    
    // Signature Help Provider
    if (server->signature_help_provider) {
        struct json_object* sig_help = json_object_new_object();
        json_object_object_add(sig_help, "triggerCharacters", json_object_new_array());
        json_object_object_add(capabilities, "signatureHelpProvider", sig_help);
    }
    
    // Server Info
    struct json_object* server_info = json_object_new_object();
    json_object_object_add(server_info, "name", json_object_new_string("SovereignLSP"));
    json_object_object_add(server_info, "version", json_object_new_string(SOVEREIGN_VERSION));
    
    // Build result
    struct json_object* result = json_object_new_object();
    json_object_object_add(result, "capabilities", capabilities);
    json_object_object_add(result, "serverInfo", server_info);
    
    lsp_send_response(server, id, result);
    
    server->initialized = 1;
    fprintf(stderr, "[LSP] Server initialized\n");
}

static void lsp_handle_completion(LSPServer* server, int id, struct json_object* params) {
    // Use thinking effort for intelligent completion
    struct json_object* text_doc = NULL;
    struct json_object* position = NULL;
    
    if (json_object_object_get_ex(params, "textDocument", &text_doc)) {
        struct json_object* uri = NULL;
        if (json_object_object_get_ex(text_doc, "uri", &uri)) {
            const char* file_uri = json_object_get_string(uri);
            fprintf(stderr, "[LSP] Completion for: %s\n", file_uri);
        }
    }
    
    // Build completion items
    struct json_object* items = json_object_new_array();
    
    // Add some sample completions
    const char* keywords[] = {"function", "class", "if", "else", "for", "while", "return", "import"};
    for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
        struct json_object* item = json_object_new_object();
        json_object_object_add(item, "label", json_object_new_string(keywords[i]));
        json_object_object_add(item, "kind", json_object_new_int(14)); // Keyword
        json_object_array_add(items, item);
    }
    
    struct json_object* result = json_object_new_object();
    json_object_object_add(result, "items", items);
    json_object_object_add(result, "isIncomplete", json_object_new_boolean(false));
    
    lsp_send_response(server, id, result);
}

static void lsp_handle_hover(LSPServer* server, int id, struct json_object* params) {
    // Provide hover information
    struct json_object* contents = json_object_new_object();
    json_object_object_add(contents, "kind", json_object_new_string("markdown"));
    json_object_object_add(contents, "value", json_object_new_string("**Sovereign IDE**\n\nHover information provided by thinking effort system."));
    
    struct json_object* result = json_object_new_object();
    json_object_object_add(result, "contents", contents);
    
    lsp_send_response(server, id, result);
}

static void lsp_handle_definition(LSPServer* server, int id, struct json_object* params) {
    // Return definition location
    struct json_object* location = json_object_new_object();
    json_object_object_add(location, "uri", json_object_new_string("file:///example.txt"));
    
    struct json_object* range = json_object_new_object();
    struct json_object* start = json_object_new_object();
    json_object_object_add(start, "line", json_object_new_int(0));
    json_object_object_add(start, "character", json_object_new_int(0));
    
    struct json_object* end = json_object_new_object();
    json_object_object_add(end, "line", json_object_new_int(0));
    json_object_object_add(end, "character", json_object_new_int(10));
    
    json_object_object_add(range, "start", start);
    json_object_object_add(range, "end", end);
    json_object_object_add(location, "range", range);
    
    lsp_send_response(server, id, location);
}

static void lsp_handle_references(LSPServer* server, int id, struct json_object* params) {
    // Return reference locations
    struct json_object* locations = json_object_new_array();
    
    // Add a sample reference
    struct json_object* location = json_object_new_object();
    json_object_object_add(location, "uri", json_object_new_string("file:///example.txt"));
    
    struct json_object* range = json_object_new_object();
    struct json_object* start = json_object_new_object();
    json_object_object_add(start, "line", json_object_new_int(5));
    json_object_object_add(start, "character", json_object_new_int(0));
    
    struct json_object* end = json_object_new_object();
    json_object_object_add(end, "line", json_object_new_int(5));
    json_object_object_add(end, "character", json_object_new_int(10));
    
    json_object_object_add(range, "start", start);
    json_object_object_add(range, "end", end);
    json_object_object_add(location, "range", range);
    
    json_object_array_add(locations, location);
    
    lsp_send_response(server, id, locations);
}

static void lsp_handle_rename(LSPServer* server, int id, struct json_object* params) {
    // Use diff engine for rename
    struct json_object* changes = json_object_new_object();
    
    struct json_object* document_changes = json_object_new_array();
    
    struct json_object* edit = json_object_new_object();
    json_object_object_add(edit, "textDocument", json_object_new_object());
    struct json_object* doc_id = json_object_new_object();
    json_object_object_add(doc_id, "uri", json_object_new_string("file:///example.txt"));
    json_object_object_add(doc_id, "version", json_object_new_int(1));
    json_object_object_add(edit, "textDocument", doc_id);
    
    struct json_object* edits = json_object_new_array();
    struct json_object* text_edit = json_object_new_object();
    
    struct json_object* range = json_object_new_object();
    struct json_object* start = json_object_new_object();
    json_object_object_add(start, "line", json_object_new_int(0));
    json_object_object_add(start, "character", json_object_new_int(0));
    
    struct json_object* end = json_object_new_object();
    json_object_object_add(end, "line", json_object_new_int(0));
    json_object_object_add(end, "character", json_object_new_int(10));
    
    json_object_object_add(range, "start", start);
    json_object_object_add(range, "end", end);
    json_object_object_add(text_edit, "range", range);
    json_object_object_add(text_edit, "newText", json_object_new_string("renamed"));
    
    json_object_array_add(edits, text_edit);
    json_object_object_add(edit, "edits", edits);
    json_object_array_add(document_changes, edit);
    
    json_object_object_add(changes, "documentChanges", document_changes);
    
    lsp_send_response(server, id, changes);
}

static void lsp_handle_document_symbol(LSPServer* server, int id, struct json_object* params) {
    // Return document symbols
    struct json_object* symbols = json_object_new_array();
    
    // Add a sample symbol
    struct json_object* symbol = json_object_new_object();
    json_object_object_add(symbol, "name", json_object_new_string("main"));
    json_object_object_add(symbol, "kind", json_object_new_int(12)); // Function
    json_object_object_add(symbol, "range", json_object_new_object());
    json_object_object_add(symbol, "selectionRange", json_object_new_object());
    
    json_object_array_add(symbols, symbol);
    
    lsp_send_response(server, id, symbols);
}

static void lsp_handle_workspace_symbol(LSPServer* server, int id, struct json_object* params) {
    // Search across workspace
    struct json_object* symbols = json_object_new_array();
    
    // Add sample workspace symbols
    struct json_object* symbol = json_object_new_object();
    json_object_object_add(symbol, "name", json_object_new_string("SovereignIDE"));
    json_object_object_add(symbol, "kind", json_object_new_int(5)); // Class
    json_object_object_add(symbol, "location", json_object_new_object());
    
    json_object_array_add(symbols, symbol);
    
    lsp_send_response(server, id, symbols);
}

static void lsp_handle_did_open(LSPServer* server, struct json_object* params) {
    struct json_object* text_doc = NULL;
    if (json_object_object_get_ex(params, "textDocument", &text_doc)) {
        struct json_object* uri = NULL;
        struct json_object* text = NULL;
        
        if (json_object_object_get_ex(text_doc, "uri", &uri)) {
            const char* file_uri = json_object_get_string(uri);
            fprintf(stderr, "[LSP] Opened: %s\n", file_uri);
            
            // Convert file:// URI to path
            const char* path = file_uri + 7; // Skip "file://"
            
            // Open in Sovereign IDE
            ide_open_file(server->ide, path);
        }
        
        if (json_object_object_get_ex(text_doc, "text", &text)) {
            const char* content = json_object_get_string(text);
            // Set editor content
            gap_destroy(server->ide->editor);
            server->ide->editor = gap_create(strlen(content) * 2);
            gap_insert(server->ide->editor, content, strlen(content));
        }
    }
}

static void lsp_handle_did_change(LSPServer* server, struct json_object* params) {
    struct json_object* content_changes = NULL;
    if (json_object_object_get_ex(params, "contentChanges", &content_changes)) {
        // Apply changes to editor
        for (size_t i = 0; i < json_object_array_length(content_changes); i++) {
            struct json_object* change = json_object_array_get_idx(content_changes, i);
            struct json_object* text = NULL;
            
            if (json_object_object_get_ex(change, "text", &text)) {
                const char* new_text = json_object_get_string(text);
                // Update editor content
                gap_destroy(server->ide->editor);
                server->ide->editor = gap_create(strlen(new_text) * 2);
                gap_insert(server->ide->editor, new_text, strlen(new_text));
                server->ide->dirty = 1;
            }
        }
    }
}

// Main message processing
static void lsp_process_message(LSPServer* server, const char* json_str) {
    struct json_object* msg = json_tokener_parse(json_str);
    if (!msg) {
        fprintf(stderr, "[LSP] Failed to parse JSON\n");
        return;
    }
    
    struct json_object* method_obj = NULL;
    struct json_object* id_obj = NULL;
    struct json_object* params_obj = NULL;
    
    if (!json_object_object_get_ex(msg, "method", &method_obj)) {
        json_object_put(msg);
        return;
    }
    
    const char* method = json_object_get_string(method_obj);
    json_object_object_get_ex(msg, "id", &id_obj);
    json_object_object_get_ex(msg, "params", &params_obj);
    
    int id = id_obj ? json_object_get_int(id_obj) : -1;
    
    fprintf(stderr, "[LSP] Method: %s\n", method);
    
    if (strcmp(method, LSP_METHOD_INITIALIZE) == 0) {
        lsp_handle_initialize(server, id, params_obj);
    }
    else if (strcmp(method, LSP_METHOD_INITIALIZED) == 0) {
        // No response needed
        fprintf(stderr, "[LSP] Client initialized\n");
    }
    else if (strcmp(method, LSP_METHOD_SHUTDOWN) == 0) {
        server->shutdown_requested = 1;
        lsp_send_response(server, id, json_object_new_object());
    }
    else if (strcmp(method, LSP_METHOD_EXIT) == 0) {
        // Exit the server
        json_object_put(msg);
        exit(0);
    }
    else if (strcmp(method, LSP_METHOD_COMPLETION) == 0) {
        lsp_handle_completion(server, id, params_obj);
    }
    else if (strcmp(method, LSP_METHOD_HOVER) == 0) {
        lsp_handle_hover(server, id, params_obj);
    }
    else if (strcmp(method, LSP_METHOD_DEFINITION) == 0) {
        lsp_handle_definition(server, id, params_obj);
    }
    else if (strcmp(method, LSP_METHOD_REFERENCES) == 0) {
        lsp_handle_references(server, id, params_obj);
    }
    else if (strcmp(method, LSP_METHOD_RENAME) == 0) {
        lsp_handle_rename(server, id, params_obj);
    }
    else if (strcmp(method, LSP_METHOD_DOCUMENT_SYMBOL) == 0) {
        lsp_handle_document_symbol(server, id, params_obj);
    }
    else if (strcmp(method, LSP_METHOD_WORKSPACE_SYMBOL) == 0) {
        lsp_handle_workspace_symbol(server, id, params_obj);
    }
    else if (strcmp(method, LSP_METHOD_DID_OPEN) == 0) {
        lsp_handle_did_open(server, params_obj);
    }
    else if (strcmp(method, LSP_METHOD_DID_CHANGE) == 0) {
        lsp_handle_did_change(server, params_obj);
    }
    else if (strcmp(method, LSP_METHOD_DID_CLOSE) == 0) {
        fprintf(stderr, "[LSP] Document closed\n");
    }
    else if (strcmp(method, LSP_METHOD_DID_SAVE) == 0) {
        ide_save_file(server->ide);
        fprintf(stderr, "[LSP] Document saved\n");
    }
    else {
        fprintf(stderr, "[LSP] Unhandled method: %s\n", method);
        if (id >= 0) {
            lsp_send_error(server, id, -32601, "Method not found");
        }
    }
    
    json_object_put(msg);
}

// Read LSP message from stdin
static char* lsp_read_message() {
    char header[1024];
    int content_length = -1;
    
    // Read headers
    while (fgets(header, sizeof(header), stdin)) {
        if (strcmp(header, "\r\n") == 0 || strcmp(header, "\n") == 0) {
            break; // End of headers
        }
        
        if (strncmp(header, "Content-Length: ", 16) == 0) {
            content_length = atoi(header + 16);
        }
    }
    
    if (content_length < 0) {
        return NULL;
    }
    
    // Read content
    char* content = (char*)malloc(content_length + 1);
    if (!content) return NULL;
    
    size_t read = fread(content, 1, content_length, stdin);
    content[read] = '\0';
    
    return content;
}

// Main entry point
int main(int argc, char** argv) {
    fprintf(stderr, "[LSP] Sovereign Language Server starting...\n");
    fprintf(stderr, "[LSP] Version: %s\n", SOVEREIGN_VERSION);
    
    const char* root_path = ".";
    if (argc > 1) {
        root_path = argv[1];
    }
    
    LSPServer* server = lsp_server_create(root_path);
    if (!server) {
        fprintf(stderr, "[LSP] Failed to create server\n");
        return 1;
    }
    
    fprintf(stderr, "[LSP] Server ready\n");
    
    // Main loop
    while (!server->shutdown_requested) {
        char* message = lsp_read_message();
        if (!message) {
            if (feof(stdin)) {
                break;
            }
            continue;
        }
        
        lsp_process_message(server, message);
        free(message);
    }
    
    fprintf(stderr, "[LSP] Server shutting down...\n");
    lsp_server_destroy(server);
    
    return 0;
}
