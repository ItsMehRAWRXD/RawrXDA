# NEXT STEPS - Concrete Action Items

**Status: Infrastructure Complete, Ready for Real Integration**

---

## PHASE 3: Real Library Linking (Days 1-2)

### Step 1: Link libcurl (2-3 hours)

**Location:** `src/library_integration.cpp`, function `HTTPClient::sendRequest()`

**Current Code (Placeholder):**
```cpp
std::string response = makeOllamaRequest("/api/generate", payload.str());
```

**What's Needed:**
```cpp
// Add at top of file:
#include <curl/curl.h>

// Replace the placeholder in sendRequest():
// Step 1: Initialize curl handle
CURL* curl = curl_easy_init();
if (!curl) {
    response.errorMessage = "Failed to initialize curl";
    return response;
}

// Step 2: Configure the request
curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());
curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, request.method.c_str());
curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

// Step 3: Add headers if present
struct curl_slist* headers = nullptr;
for (const auto& header : request.headers) {
    std::string header_str = header.first + ": " + header.second;
    headers = curl_slist_append(headers, header_str.c_str());
}
if (headers) {
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
}

// Step 4: Set request body if POST
if (!request.body.empty()) {
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
}

// Step 5: Set response callback
std::string response_body;
curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);

// Step 6: Perform request
CURLcode res = curl_easy_perform(curl);

// Step 7: Extract response code and body
long response_code = 0;
curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

// Step 8: Cleanup
curl_slist_free_all(headers);
curl_easy_cleanup(curl);

// Return result
response.statusCode = response_code;
response.body = response_body;
response.success = (res == CURLE_OK && response_code == 200);
```

**Helper Function Needed:**
```cpp
static size_t curl_callback(void* contents, size_t size, size_t nmemb, std::string* s) {
    size_t newlen = size * nmemb;
    s->append((char*)contents, newlen);
    return newlen;
}
```

**CMakeLists.txt Update Needed:**
```cmake
find_package(CURL REQUIRED)
# ... in target_link_libraries:
target_link_libraries(RawrXD-AgenticIDE PRIVATE CURL::libcurl)
```

**Verification:**
```bash
# Test locally:
cmake --build build_prod --config Release
# Should compile without curl errors
```

---

### Step 2: Link zstd (2-3 hours)

**Location:** `src/library_integration.cpp`, `CompressionHandler::compress()`

**Current Code (Placeholder):**
```cpp
// For simulation, just copy data
compressed = data;
```

**What's Needed:**
```cpp
#include <zstd.h>

// Replace in compress():
size_t maxCompressedSize = ZSTD_compressBound(data.size());
compressed.resize(maxCompressedSize);

size_t compressedSize = ZSTD_compress(
    compressed.data(),
    maxCompressedSize,
    data.data(),
    data.size(),
    compressionLevel
);

if (ZSTD_isError(compressedSize)) {
    if (m_logger) {
        m_logger->error("Compression failed: {}", 
            ZSTD_getErrorName(compressedSize));
    }
    compressed.clear();
    return compressed;
}

compressed.resize(compressedSize);
```

**For decompress():**
```cpp
unsigned long long decompressedSize = ZSTD_getFrameContentSize(
    compressedData.data(), 
    compressedData.size()
);

if (decompressedSize == ZSTD_CONTENTSIZE_ERROR) {
    if (m_logger) m_logger->error("Invalid ZSTD frame");
    return decompressed;
}

decompressed.resize(decompressedSize);

size_t actualSize = ZSTD_decompress(
    decompressed.data(),
    decompressedSize,
    compressedData.data(),
    compressedData.size()
);

if (ZSTD_isError(actualSize)) {
    if (m_logger) {
        m_logger->error("Decompression failed: {}", 
            ZSTD_getErrorName(actualSize));
    }
    decompressed.clear();
    return decompressed;
}
```

**CMakeLists.txt Update:**
```cmake
find_package(zstd REQUIRED)
# ... in target_link_libraries:
target_link_libraries(RawrXD-AgenticIDE PRIVATE zstd::libzstd_shared)
```

---

## PHASE 4: Integration with Engine (Days 2-3)

### Step 3: Wire Response Parser to RealTimeCompletionEngine (1-2 hours)

**Location:** `src/real_time_completion_engine.cpp`

**What to Add:**
```cpp
// In header includes:
#include "response_parser.h"

// In class definition:
class RealTimeCompletionEngine {
private:
    std::shared_ptr<ResponseParser> m_responseParser;
    // ... existing members
};

// In constructor:
RealTimeCompletionEngine::RealTimeCompletionEngine(...) {
    m_responseParser = std::make_shared<ResponseParser>(logger, metrics);
}

// In generateCompletionsWithModel() - MODIFY EXISTING METHOD:
std::vector<CodeCompletion> RealTimeCompletionEngine::generateCompletionsWithModel(
    const std::string& context) {
    
    try {
        // 1. Get raw model response (existing code)
        std::string rawModelOutput = m_inferenceEngine->generate(context);
        
        // 2. Parse into completions (NEW - uses ResponseParser)
        auto parsedCompletions = m_responseParser->parseResponse(rawModelOutput);
        
        // 3. Filter by confidence (NEW)
        std::vector<CodeCompletion> results;
        for (const auto& parsed : parsedCompletions) {
            if (parsed.confidence > 0.65) { // Configurable threshold
                CodeCompletion completion;
                completion.text = parsed.text;
                completion.confidence = parsed.confidence;
                completion.tokenCount = parsed.tokenCount;
                results.push_back(completion);
            }
        }
        
        // 4. Record metrics
        m_metrics->recordHistogram("parsed_completions", results.size());
        m_metrics->recordHistogram("avg_confidence", 
            std::accumulate(results.begin(), results.end(), 0.0,
                [](double sum, const auto& c) { return sum + c.confidence; }) 
            / results.size());
        
        return results;
        
    } catch (const std::exception& e) {
        m_logger->error("Failed to parse completions: {}", e.what());
        return {};
    }
}
```

**Test Locally:**
```cpp
// In test code:
RealTimeCompletionEngine engine(logger, metrics, inferenceEngine);
auto completions = engine.generateCompletionsWithModel("function add(");
// Should return parsed completions with confidence scores
```

---

### Step 4: Test ModelTester with Real Ollama (1-2 hours)

**Prerequisites:**
- libcurl linked and working
- Ollama running on localhost:11434

**Test Code:**
```cpp
// In test file or demo:
auto tester = std::make_shared<ModelTester>(logger, metrics, parser);

// Once libcurl is linked, this should work:
ModelTestResult result = tester->testWithOllama("llama2", "print('hello')", 50);

// Verify we get real response (not placeholder)
std::cout << "Latency: " << result.totalLatencyUs << " us\n";
std::cout << "Response length: " << result.response.length() << " chars\n";
std::cout << "Quality: " << result.responseQuality * 100 << "%\n";

// Generate report
std::string report = tester->generateTestReport();
std::cout << report;
```

**Debugging If It Doesn't Work:**
1. Verify Ollama is running: `curl http://localhost:11434/api/tags`
2. Check libcurl errors in logs
3. Add debug logging to see curl return codes
4. Verify firewall isn't blocking localhost:11434

---

## PHASE 5: UI Integration (Days 3-4)

### Step 5: Connect to IDE UI (2-3 hours)

**In `src/code_editor.cpp` or equivalent:**
```cpp
// When user types, trigger real completions
void CodeEditor::onTextChanged() {
    std::string context = getCurrentContext();
    
    // Get real parsed completions
    auto completions = m_completionEngine->generateCompletionsWithModel(context);
    
    // Render with confidence indicators
    for (const auto& completion : completions) {
        QListWidgetItem* item = new QListWidgetItem();
        item->setText(QString::fromStdString(completion.text));
        
        // Color by confidence
        if (completion.confidence > 0.85) {
            item->setBackground(QColor(0, 200, 0)); // Green - high confidence
        } else if (completion.confidence > 0.65) {
            item->setBackground(QColor(200, 200, 0)); // Yellow - medium confidence
        } else {
            item->setBackground(QColor(200, 100, 0)); // Orange - lower confidence
        }
        
        m_suggestionList->addItem(item);
    }
}
```

---

## Verification Checklist

After each step, verify:

### After libcurl linking:
- [ ] Compiles without errors
- [ ] Can make HTTP GET request to Ollama
- [ ] HTTP response is valid JSON (not placeholder)
- [ ] Response codes are correct

### After zstd linking:
- [ ] Compiles without errors
- [ ] Can compress data and get ratio < 1.0
- [ ] Can decompress and get original data back
- [ ] File compression/decompression works

### After ResponseParser integration:
- [ ] parseResponse() returns real completions
- [ ] Confidence scores are > 0 and <= 1.0
- [ ] Token counts are reasonable
- [ ] Metrics are recorded

### After ModelTester integration:
- [ ] testWithOllama() returns real response (not placeholder)
- [ ] Latency measurements are microsecond precision
- [ ] Quality scoring is between 0.0 and 1.0
- [ ] Reports generate correctly

### After UI integration:
- [ ] Typing triggers completion suggestions
- [ ] Completions appear with confidence colors
- [ ] Metrics show real data (not zeros)
- [ ] Performance is acceptable (< 500ms per suggestion)

---

## Timeline Summary

| Phase | Task | Est. Hours | Status |
|-------|------|-----------|--------|
| 3a | Link libcurl | 2-3 | Not started |
| 3b | Link zstd | 2-3 | Not started |
| 4a | Wire ResponseParser | 1-2 | Not started |
| 4b | Real Ollama integration | 1-2 | Not started |
| 5 | UI wiring | 2-3 | Not started |
| - | **TOTAL** | **9-13 hours** | **Ready to start** |

**Realistic completion: 3-4 focused days**

---

## Resources Provided

- ✅ `HONEST_ASSESSMENT.md` - What's real vs placeholder
- ✅ `QUICK_REFERENCE.md` - Usage examples
- ✅ `PHASE_1_2_COMPLETION_REPORT.md` - Detailed docs
- ✅ Source files with inline comments
- ✅ This document with concrete next steps

---

## Get Started

Next action: **Link libcurl** (2-3 hours)

Commands:
```bash
# 1. Install libcurl dev package (if not present)
# Windows: Already included with development tools
# Linux: sudo apt-get install libcurl4-openssl-dev
# macOS: brew install curl

# 2. Update CMakeLists.txt (instructions above)
# 3. Rebuild
cd build_prod && cmake --build . --config Release

# 4. Verify compilation succeeds without curl errors
```

Then verify with test code above.

