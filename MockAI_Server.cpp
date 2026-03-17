// MockAI_Server.cpp
// Simple HTTP Server for Testing AI Integration
// Runs on localhost:8000, returns mock completions
// Compile with: cl /MD MockAI_Server.cpp

#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <cstring>
#include <cstdlib>
#include <time.h>

#pragma comment(lib, "ws2_32.lib")

#define DEFAULT_PORT "8000"
#define LISTEN_BACKLOG 1
#define BUFFER_SIZE 4096

// ============================================================================
// MOCK AI RESPONSES
// ============================================================================

struct MockCompletion {
    const char* prompt_start;
    const char* completion;
};

MockCompletion mock_completions[] = {
    { "def ", "hello():\n    print('Hello World')" },
    { "for ", "i in range(10):\n    print(i)" },
    { "if ", "x > 5:\n    print('Big')" },
    { "class ", "MyClass:\n    def __init__(self):\n        pass" },
    { "import ", "os\nimport sys" },
    { "The q", "uick brown fox jumps over the lazy dog" },
    { "Hello ", "World! This is a test." },
    { "C++ i", "s awesome for systems programming" },
    { "MASM ", "is powerful for low-level code" },
    { "Windows ", "API calls are essential for GUI apps" },
};

const char* MockAI_GetCompletion(const char* prompt) {
    // Find a matching prefix
    for (int i = 0; i < sizeof(mock_completions) / sizeof(MockCompletion); i++) {
        if (strstr(prompt, mock_completions[i].prompt_start) != NULL) {
            return mock_completions[i].completion;
        }
    }
    
    // Default completion if no match
    return " is interesting";
}

// ============================================================================
// JSON RESPONSE GENERATION
// ============================================================================

void BuildJSONResponse(char* response, size_t size, const char* tokens) {
    snprintf(response, size,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n"
        "{"
            "\"tokens\": \"%s\","
            "\"model\": \"mock-ai-1.0\","
            "\"timestamp\": %ld"
        "}",
        strlen(tokens) + 70,
        tokens,
        time(NULL)
    );
}

// ============================================================================
// HTTP REQUEST PARSER
// ============================================================================

bool ParseJSONRequest(const char* request, char* prompt, size_t prompt_size) {
    // Extract "prompt" field from JSON
    const char* prompt_start = strstr(request, "\"prompt\": \"");
    if (!prompt_start) {
        return false;
    }
    
    prompt_start += 11;  // Skip past "prompt": "
    const char* prompt_end = strchr(prompt_start, '"');
    if (!prompt_end) {
        return false;
    }
    
    size_t len = prompt_end - prompt_start;
    if (len >= prompt_size) len = prompt_size - 1;
    
    strncpy_s(prompt, prompt_size, prompt_start, len);
    prompt[len] = '\0';
    
    return true;
}

// ============================================================================
// HTTP SERVER
// ============================================================================

SOCKET ListenSocket = INVALID_SOCKET;
SOCKET ClientSocket = INVALID_SOCKET;

bool Server_Initialize() {
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != 0) {
        printf("[ERROR] WSAStartup failed: %d\n", result);
        return false;
    }
    
    struct addrinfo* result_addr = NULL;
    struct addrinfo hints = {0};
    
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;
    
    result = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result_addr);
    if (result != 0) {
        printf("[ERROR] getaddrinfo failed: %d\n", result);
        WSACleanup();
        return false;
    }
    
    ListenSocket = socket(result_addr->ai_family, result_addr->ai_socktype, result_addr->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        printf("[ERROR] socket failed: %ld\n", WSAGetLastError());
        freeaddrinfo(result_addr);
        WSACleanup();
        return false;
    }
    
    result = bind(ListenSocket, result_addr->ai_addr, (int)result_addr->ai_addrlen);
    if (result == SOCKET_ERROR) {
        printf("[ERROR] bind failed: %d\n", WSAGetLastError());
        freeaddrinfo(result_addr);
        closesocket(ListenSocket);
        WSACleanup();
        return false;
    }
    
    freeaddrinfo(result_addr);
    
    result = listen(ListenSocket, LISTEN_BACKLOG);
    if (result == SOCKET_ERROR) {
        printf("[ERROR] listen failed: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return false;
    }
    
    printf("[OK] Server listening on port %s\n", DEFAULT_PORT);
    return true;
}

void Server_HandleConnection() {
    static int request_count = 0;
    
    ClientSocket = accept(ListenSocket, NULL, NULL);
    if (ClientSocket == INVALID_SOCKET) {
        printf("[ERROR] accept failed: %d\n", WSAGetLastError());
        return;
    }
    
    request_count++;
    printf("\n[REQUEST %d] Client connected\n", request_count);
    
    // Receive HTTP request
    char buffer[BUFFER_SIZE] = {0};
    int recv_result = recv(ClientSocket, buffer, BUFFER_SIZE - 1, 0);
    
    if (recv_result < 0) {
        printf("[ERROR] recv failed\n");
        closesocket(ClientSocket);
        return;
    }
    
    buffer[recv_result] = '\0';
    printf("[REQUEST] Received %d bytes\n", recv_result);
    
    // Parse request
    char prompt[512] = {0};
    if (!ParseJSONRequest(buffer, prompt, sizeof(prompt))) {
        printf("[WARN] Failed to parse JSON request\n");
        strcpy_s(prompt, sizeof(prompt), "test");
    }
    
    printf("[REQUEST] Prompt: %s\n", prompt);
    
    // Get mock completion
    const char* completion = MockAI_GetCompletion(prompt);
    printf("[RESPONSE] Completion: %s\n", completion);
    
    // Build HTTP response
    char response[2048];
    BuildJSONResponse(response, sizeof(response), completion);
    
    // Send response
    int send_result = send(ClientSocket, response, (int)strlen(response), 0);
    if (send_result < 0) {
        printf("[ERROR] send failed\n");
    } else {
        printf("[RESPONSE] Sent %d bytes\n", send_result);
    }
    
    // Close connection
    closesocket(ClientSocket);
    printf("[OK] Connection closed\n");
}

void Server_Shutdown() {
    if (ListenSocket != INVALID_SOCKET) {
        closesocket(ListenSocket);
    }
    WSACleanup();
    printf("[OK] Server shut down\n");
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char* argv[]) {
    printf("=== RawrXD Mock AI Server ===\n");
    printf("Version: 1.0\n");
    printf("Purpose: Test AI token streaming integration\n\n");
    
    printf("[INIT] Starting server...\n");
    if (!Server_Initialize()) {
        return 1;
    }
    
    printf("\n[READY] Waiting for connections on localhost:8000\n");
    printf("Press Ctrl+C to stop\n\n");
    
    // Main server loop
    while (true) {
        Server_HandleConnection();
    }
    
    Server_Shutdown();
    return 0;
}
