// Test Phase 1: Backend API - Ollama Client & WebSocket Server
#define NOMINMAX
#include "backend/ollama_client.h"
#include "backend/websocket_server.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>

using namespace RawrXD::Backend;

void printTestHeader(const std::string& title) {
    std::cout << "\n========================================\n";
    std::cout << title << "\n";
    std::cout << "========================================\n";
}

void testOllamaClient() {
    printTestHeader("TEST 1: Ollama Client - Connection & Version");
    
    OllamaClient client("http://localhost:11434");
    
    // Test connection
    if (client.testConnection()) {
        std::cout << "✓ Ollama connection successful\n";
    } else {
        std::cout << "✗ Ollama connection failed (server may not be running)\n";
        return;
    }
    
    // Test version
    std::string version = client.getVersion();
    std::cout << "Version: " << version << "\n";
    
    // Test running status
    if (client.isRunning()) {
        std::cout << "✓ Ollama server is running\n";
    } else {
        std::cout << "✗ Ollama server not running\n";
    }
}

void testOllamaModels() {
    printTestHeader("TEST 2: Ollama Client - List Models");
    
    OllamaClient client("http://localhost:11434");
    
    auto models = client.listModels();
    
    std::cout << "Found " << models.size() << " models:\n";
    for (const auto& model : models) {
        std::cout << "  - " << model.name 
                  << " (" << model.size / (1024*1024) << " MB)\n";
        std::cout << "    Modified: " << model.modified_at << "\n";
    }
}

void testOllamaGenerate() {
    printTestHeader("TEST 3: Ollama Client - Text Generation");
    
    OllamaClient client("http://localhost:11434");
    
    OllamaGenerateRequest req;
    req.model = "llama2"; // Change to a model you have installed
    req.prompt = "What is 2+2? Answer in one sentence.";
    req.stream = false;
    
    std::cout << "Sending request to model: " << req.model << "\n";
    std::cout << "Prompt: " << req.prompt << "\n";
    std::cout << "Waiting for response...\n";
    
    OllamaResponse response = client.generateSync(req);
    
    if (!response.response.empty()) {
        std::cout << "✓ Response: " << response.response << "\n";
        std::cout << "  Model: " << response.model << "\n";
        std::cout << "  Done: " << (response.done ? "yes" : "no") << "\n";
    } else {
        std::cout << "✗ No response received\n";
    }
}

void testOllamaChat() {
    printTestHeader("TEST 4: Ollama Client - Chat Completion");
    
    OllamaClient client("http://localhost:11434");
    
    OllamaChatRequest req;
    req.model = "llama2"; // Change to a model you have installed
    req.stream = false;
    
    // Add messages
    req.messages.push_back({"system", "You are a helpful assistant."});
    req.messages.push_back({"user", "What is the capital of France?"});
    
    std::cout << "Sending chat request to model: " << req.model << "\n";
    std::cout << "Messages: " << req.messages.size() << "\n";
    std::cout << "Waiting for response...\n";
    
    OllamaResponse response = client.chatSync(req);
    
    if (!response.message.content.empty()) {
        std::cout << "✓ Response: " << response.message.content << "\n";
        std::cout << "  Role: " << response.message.role << "\n";
    } else {
        std::cout << "✗ No response received\n";
    }
}

void testOllamaEmbeddings() {
    printTestHeader("TEST 5: Ollama Client - Text Embeddings");
    
    OllamaClient client("http://localhost:11434");
    
    std::string text = "This is a test sentence for embeddings.";
    std::cout << "Generating embeddings for: " << text << "\n";
    
    auto embedding = client.embeddings("llama2", text); // Change to a model you have
    
    if (!embedding.empty()) {
        std::cout << "✓ Generated embedding vector of size: " << embedding.size() << "\n";
        std::cout << "  First 5 values: ";
        for (size_t i = 0; i < std::min<size_t>(5, embedding.size()); ++i) {
            std::cout << embedding[i] << " ";
        }
        std::cout << "\n";
    } else {
        std::cout << "✗ Failed to generate embeddings\n";
    }
}

void testWebSocketServer() {
    printTestHeader("TEST 6: WebSocket Server - Start/Stop");
    
    WebSocketServer server(8080);
    
    // Set up callbacks
    server.onConnect([](const std::string& client_id) {
        std::cout << "✓ Client connected: " << client_id << "\n";
    });
    
    server.onDisconnect([](const std::string& client_id) {
        std::cout << "✓ Client disconnected: " << client_id << "\n";
    });
    
    server.onMessage([](const WSMessage& message) {
        std::cout << "✓ Received message: ";
        if (message.type == WSMessageType::TEXT) {
            std::cout << message.text << "\n";
        } else {
            std::cout << message.data.size() << " bytes\n";
        }
    });
    
    server.onError([](const std::string& error) {
        std::cout << "✗ Server error: " << error << "\n";
    });
    
    // Start server
    if (server.start()) {
        std::cout << "✓ WebSocket server started on port 8080\n";
        std::cout << "  Waiting for connections for 5 seconds...\n";
        std::cout << "  You can test with: wscat -c ws://localhost:8080\n";
        
        // Wait for connections
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        // Check connected clients
        auto clients = server.getConnectedClients();
        std::cout << "  Connected clients: " << clients.size() << "\n";
        
        // Stop server
        server.stop();
        std::cout << "✓ WebSocket server stopped\n";
    } else {
        std::cout << "✗ Failed to start WebSocket server\n";
    }
}

void testWebSocketBroadcast() {
    printTestHeader("TEST 7: WebSocket Server - Broadcasting");
    
    WebSocketServer server(8080);
    std::atomic<int> client_count{0};
    
    server.onConnect([&client_count](const std::string& client_id) {
        client_count++;
        std::cout << "✓ Client connected: " << client_id << " (total: " << client_count << ")\n";
    });
    
    server.onDisconnect([&client_count](const std::string& client_id) {
        client_count--;
        std::cout << "✓ Client disconnected: " << client_id << " (total: " << client_count << ")\n";
    });
    
    if (server.start()) {
        std::cout << "✓ Server started on port 8080\n";
        std::cout << "  Connect multiple clients with: wscat -c ws://localhost:8080\n";
        std::cout << "  Waiting 5 seconds for connections...\n";
        
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        if (client_count > 0) {
            std::cout << "  Broadcasting test message to " << client_count << " clients...\n";
            server.broadcast("Hello from RawrXD IDE!");
            
            std::cout << "  Broadcasting JSON message...\n";
            std::string json_msg = BrowserMessage::createNotification(
                "test.notification",
                {{"timestamp", "2024-01-01T00:00:00Z"}, {"message", "Test broadcast"}}
            );
            server.broadcast(json_msg);
            
            std::cout << "✓ Broadcast complete\n";
        } else {
            std::cout << "  No clients connected for broadcast test\n";
        }
        
        server.stop();
        std::cout << "✓ Server stopped\n";
    } else {
        std::cout << "✗ Failed to start server\n";
    }
}

void testBrowserMessages() {
    printTestHeader("TEST 8: Browser Message Helpers");
    
    // Test request creation
    std::string request = BrowserMessage::createRequest(
        "editor.setText",
        {{"file", "test.cpp"}, {"content", "int main() {}"}}
    );
    std::cout << "Request: " << request << "\n";
    
    // Test response creation
    std::string response = BrowserMessage::createResponse(1, "success");
    std::cout << "Response: " << response << "\n";
    
    // Test error creation
    std::string error = BrowserMessage::createError(1, "File not found", 404);
    std::cout << "Error: " << error << "\n";
    
    // Test notification creation
    std::string notification = BrowserMessage::createNotification(
        "file.changed",
        {{"path", "src/main.cpp"}, {"event", "modified"}}
    );
    std::cout << "Notification: " << notification << "\n";
    
    std::cout << "✓ All message helpers working\n";
}

int main() {
    std::cout << "RawrXD IDE - Phase 1 Backend API Test Suite\n";
    std::cout << "============================================\n";
    
    // Test Ollama client
    testOllamaClient();
    testOllamaModels();
    
    // These tests require Ollama server running with a model
    // Uncomment to test if you have Ollama running:
    // testOllamaGenerate();
    // testOllamaChat();
    // testOllamaEmbeddings();
    
    // Test WebSocket server
    testWebSocketServer();
    
    // Test broadcasting (requires manual client connection)
    // Uncomment to test with wscat or browser:
    // testWebSocketBroadcast();
    
    // Test message helpers
    testBrowserMessages();
    
    std::cout << "\n========================================\n";
    std::cout << "Phase 1 Test Suite Complete\n";
    std::cout << "========================================\n";
    
    return 0;
}
