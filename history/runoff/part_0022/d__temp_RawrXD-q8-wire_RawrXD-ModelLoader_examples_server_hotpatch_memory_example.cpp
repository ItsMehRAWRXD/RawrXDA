// server_hotpatch_memory_example.cpp
// Demonstrates direct memory manipulation in GGUF Server Hotpatcher
// Zero-copy, high-performance request/response modification

#include "../src/qtapp/gguf_server_hotpatch.hpp"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>

void example_basic_memory_patching()
{
    qInfo() << "=== Example 1: Basic In-Place Memory Patching ===";
    
    GGUFServerHotpatch hotpatcher;
    
    // Original request body
    QByteArray requestBody = R"({"model":"llama3","prompt":"Hello","temperature":0.8})";
    
    // Lock the memory region for direct access
    auto region = hotpatcher.lockMemoryRegion(requestBody);
    
    // Find and replace "0.8" with "0.7" (same size - zero-copy)
    bool success = hotpatcher.findAndReplaceInMemory(region, "0.8", "0.7");
    
    hotpatcher.unlockMemoryRegion(region);
    
    qInfo() << "Patched request:" << requestBody;
    qInfo() << "Success:" << success;
    // Output: {"model":"llama3","prompt":"Hello","temperature":0.7}
}

void example_pattern_matching()
{
    qInfo() << "\n=== Example 2: Fast Pattern Matching ===";
    
    GGUFServerHotpatch hotpatcher;
    
    QByteArray responseBody = R"({
        "model": "llama3",
        "created_at": "2024-12-03T10:30:00Z",
        "response": "I can help you with that!",
        "done": true
    })";
    
    auto region = hotpatcher.lockMemoryRegion(responseBody);
    
    // Check if pattern exists (ultra-fast Boyer-Moore-Horspool)
    bool hasResponse = hotpatcher.memoryContainsPattern(region, "\"response\"");
    qInfo() << "Contains 'response' field:" << hasResponse;
    
    // Find all occurrences of "true"
    QList<size_t> offsets = hotpatcher.findPatternOffsets(region, "true");
    qInfo() << "Found 'true' at offsets:" << offsets;
    
    hotpatcher.unlockMemoryRegion(region);
}

void example_json_field_patching()
{
    qInfo() << "\n=== Example 3: In-Memory JSON Field Patching ===";
    
    GGUFServerHotpatch hotpatcher;
    
    QByteArray requestBody = R"({"model":"llama3","temperature":0.80,"max_tokens":512})";
    
    auto region = hotpatcher.lockMemoryRegion(requestBody);
    
    // Patch temperature value (must be same size!)
    bool success1 = hotpatcher.patchJsonFieldInMemory(region, "temperature", "0.75");
    qInfo() << "Temperature patched:" << success1;
    
    // Patch max_tokens (3 digits to 3 digits)
    bool success2 = hotpatcher.patchJsonFieldInMemory(region, "max_tokens", "256");
    qInfo() << "Max tokens patched:" << success2;
    
    hotpatcher.unlockMemoryRegion(region);
    
    qInfo() << "Final request:" << requestBody;
    // Output: {"model":"llama3","temperature":0.75,"max_tokens":256}
}

void example_direct_buffer_access()
{
    qInfo() << "\n=== Example 4: Direct Buffer Access (Advanced) ===";
    
    GGUFServerHotpatch hotpatcher;
    
    QByteArray buffer = "POST /api/generate HTTP/1.1\r\nHost: localhost\r\n";
    
    size_t bufferSize = 0;
    char* directAccess = hotpatcher.getDirectBufferAccess(buffer, bufferSize);
    
    // Direct memory manipulation at byte level
    if (bufferSize > 10) {
        // Change "POST" to "PUT " (same size)
        directAccess[0] = 'P';
        directAccess[1] = 'U';
        directAccess[2] = 'T';
        directAccess[3] = ' ';
    }
    
    hotpatcher.releaseDirectBufferAccess(buffer);
    
    qInfo() << "Modified buffer:" << buffer;
    // Output: PUT  /api/generate HTTP/1.1...
}

void example_streaming_response_modification()
{
    qInfo() << "\n=== Example 5: Streaming Response Chunk Modification ===";
    
    GGUFServerHotpatch hotpatcher;
    
    // Simulated SSE streaming chunk
    QByteArray chunk = "data: {\"response\":\"I am an AI assistant\"}\n\n";
    
    auto region = hotpatcher.lockMemoryRegion(chunk);
    
    // Replace "AI assistant" with "helpful model" (same length)
    bool success = hotpatcher.findAndReplaceInMemory(
        region,
        "AI assistant",
        "helpful model"
    );
    
    hotpatcher.unlockMemoryRegion(region);
    
    qInfo() << "Modified chunk:" << chunk;
    qInfo() << "Success:" << success;
    // Output: data: {"response":"I am an helpful model"}
}

void example_high_performance_filtering()
{
    qInfo() << "\n=== Example 6: High-Performance Content Filtering ===";
    
    GGUFServerHotpatch hotpatcher;
    
    QByteArray response = R"({
        "model": "llama3",
        "response": "Here is some confidential information that should be filtered.",
        "done": true
    })";
    
    auto region = hotpatcher.lockMemoryRegion(response);
    
    // Fast check for sensitive patterns
    QStringList forbiddenPatterns = {"confidential", "password", "secret"};
    
    bool hasForbiddenContent = false;
    for (const QString& pattern : forbiddenPatterns) {
        if (hotpatcher.memoryContainsPattern(region, pattern.toUtf8())) {
            hasForbiddenContent = true;
            qWarning() << "Found forbidden pattern:" << pattern;
            
            // Replace with asterisks (same length)
            QByteArray censored(pattern.length(), '*');
            hotpatcher.findAndReplaceInMemory(region, pattern.toUtf8(), censored);
        }
    }
    
    hotpatcher.unlockMemoryRegion(region);
    
    qInfo() << "Filtered response:" << response;
    qInfo() << "Had forbidden content:" << hasForbiddenContent;
    // Output: {...,"response": "Here is some ************ information..."}
}

void example_performance_benchmark()
{
    qInfo() << "\n=== Example 7: Performance Comparison ===";
    
    GGUFServerHotpatch hotpatcher;
    
    // Large response body (typical LLM response)
    QByteArray largeResponse;
    for (int i = 0; i < 1000; i++) {
        largeResponse += R"({"chunk":"This is a response fragment","index":)" 
                        + QByteArray::number(i) + "},";
    }
    
    // Method 1: Traditional approach (parse + modify + serialize)
    QElapsedTimer timer1;
    timer1.start();
    QJsonDocument doc = QJsonDocument::fromJson(largeResponse);
    QJsonObject obj = doc.object();
    obj["modified"] = true;
    QByteArray result1 = QJsonDocument(obj).toJson();
    qint64 traditional_ms = timer1.elapsed();
    
    // Method 2: Direct memory manipulation (zero-copy)
    QElapsedTimer timer2;
    timer2.start();
    auto region = hotpatcher.lockMemoryRegion(largeResponse);
    hotpatcher.findAndReplaceInMemory(region, "\"chunk\"", "\"block\"");
    hotpatcher.unlockMemoryRegion(region);
    qint64 zerocopy_ms = timer2.elapsed();
    
    qInfo() << "Traditional approach:" << traditional_ms << "ms";
    qInfo() << "Zero-copy approach:" << zerocopy_ms << "ms";
    qInfo() << "Speedup:" << (double)traditional_ms / zerocopy_ms << "x";
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    
    qInfo() << "GGUF Server Hotpatcher - Direct Memory Manipulation Examples\n";
    
    example_basic_memory_patching();
    example_pattern_matching();
    example_json_field_patching();
    example_direct_buffer_access();
    example_streaming_response_modification();
    example_high_performance_filtering();
    example_performance_benchmark();
    
    qInfo() << "\n=== All Examples Completed ===";
    
    return 0;
}
