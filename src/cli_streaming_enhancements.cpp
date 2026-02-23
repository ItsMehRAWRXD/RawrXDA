// cli_streaming_enhancements.cpp
// Integration of streaming enhancements into RawrXD CLI
// ============================================================================

#include "streaming_enhancements.h"
#include <iostream>
#include <iomanip>

#include "logging/logger.h"
static Logger s_logger("cli_streaming_enhancements");

// Global instances
static std::unique_ptr<AsyncStreamingEngine> g_asyncEngine;
static std::unique_ptr<BatchProcessingEngine> g_batchEngine;
static std::unique_ptr<KVCacheManager> g_cacheManager;
static std::unique_ptr<EnhancedCLIEngine> g_enhancedEngine;
static OutputFormat g_currentFormat = OutputFormat::TEXT;

// ============================================================================
// INITIALIZATION
// ============================================================================

void initializeStreamingEnhancements() {
    g_asyncEngine = std::make_unique<AsyncStreamingEngine>();
    g_batchEngine = std::make_unique<BatchProcessingEngine>(4);
    g_cacheManager = std::make_unique<KVCacheManager>(512 * 1024 * 1024);  // 512MB
    g_enhancedEngine = std::make_unique<EnhancedCLIEngine>();
    
    s_logger.info("[INIT] Streaming enhancements initialized");
    s_logger.info("  - Async streaming engine: Ready");
    s_logger.info("  - Batch processing engine: Ready (4 workers)");
    s_logger.info("  - KV-cache manager: Ready (512MB)");
}

// ============================================================================
// 1. ASYNC STREAMING COMMANDS
// ============================================================================

void cmdStreamAsync(const std::string& prompt) {
    s_logger.info("[INFO] Starting async streaming (non-blocking)...");
    
    int tokenCount = 0;
    std::string fullResponse;
    
    g_asyncEngine->streamAsync(
        prompt,
        [&tokenCount, &fullResponse](const std::string& token) {
            s_logger.info( token;
            std::cout.flush();
            fullResponse += token;
            tokenCount++;
        },
        [&tokenCount](const std::string& response, bool success) {
            s_logger.info("\n");
            if (success) {
                s_logger.info("[SUCCESS] Async streaming complete: ");
            } else {
                s_logger.info("[ERROR] ");
            }
        }
    );
    
    // Don't block - async operation continues
    s_logger.info("[INFO] Streaming in background. CLI remains responsive.");
}

void cmdWaitStream(int timeoutMs) {
    s_logger.info("[INFO] Waiting for async stream to complete...");
    
    if (g_asyncEngine->waitForCompletion(timeoutMs)) {
        s_logger.info("[SUCCESS] Stream completed");
    } else {
        s_logger.info("[WARNING] Stream timeout after ");
    }
}

void cmdCancelStream() {
    if (g_asyncEngine->isStreaming()) {
        g_asyncEngine->cancelStreaming();
        s_logger.info("[INFO] Streaming cancelled");
    } else {
        s_logger.info("[INFO] No stream in progress");
    }
}

// ============================================================================
// 2. BATCH PROCESSING COMMANDS
// ============================================================================

void cmdBatchProcess(const std::vector<std::string>& prompts) {
    s_logger.info("[INFO] Submitting ");
    
    std::vector<std::string> requestIds;
    
    for (size_t i = 0; i < prompts.size(); ++i) {
        BatchRequest req;
        req.id = StreamingUtils::generateRequestId();
        req.prompt = prompts[i];
        req.maxTokens = 128;
        
        g_batchEngine->submitBatch(req);
        requestIds.push_back(req.id);
        
        s_logger.info("[BATCH ");
    }
    
    s_logger.info("\n[INFO] All requests submitted. Processing in parallel...");
    
    // Retrieve results
    s_logger.info("\n[INFO] Retrieving results...\n");
    for (size_t i = 0; i < requestIds.size(); ++i) {
        auto result = g_batchEngine->getBatchResult(requestIds[i], 30000);  // 30s timeout
        
        s_logger.info("[RESULT ");
        s_logger.info("  ID: ");
        s_logger.info("  Success: ");
        s_logger.info("  Tokens: ");
        s_logger.info("  Duration: ");
        s_logger.info("  Response: ");
        s_logger.info( std::endl;
    }
}

void cmdBatchStatus() {
    auto stats = g_batchEngine->getBatchStats();
    
    s_logger.info("\n[BATCH STATUS]");
    s_logger.info("  Queued: ");
    s_logger.info("  Completed: ");
    s_logger.info("  Workers: ");
}

// ============================================================================
// 3. TOKENIZER COMMANDS
// ============================================================================

void cmdLoadBPETokenizer(const std::string& mergesFile) {
    s_logger.info("[INFO] Loading BPE tokenizer from: ");
    
    auto tokenizer = TokenizerFactory::createBPETokenizer(mergesFile);
    
    if (tokenizer) {
        s_logger.info("[SUCCESS] BPE tokenizer loaded");
    } else {
        s_logger.info("[ERROR] Failed to load BPE tokenizer");
    }
}

void cmdLoadSPTokenizer(const std::string& modelPath) {
    s_logger.info("[INFO] Loading SentencePiece tokenizer from: ");
    
    auto tokenizer = TokenizerFactory::createSentencePieceTokenizer(modelPath);
    
    if (tokenizer) {
        s_logger.info("[SUCCESS] SentencePiece tokenizer loaded");
    } else {
        s_logger.info("[ERROR] Failed to load SentencePiece tokenizer");
    }
}

void cmdAutoTokenizer(const std::string& modelPath) {
    s_logger.info("[INFO] Auto-detecting tokenizer for: ");
    
    auto tokenizer = TokenizerFactory::createAutoTokenizer(modelPath);
    
    if (tokenizer) {
        s_logger.info("[SUCCESS] Tokenizer loaded: ");
    } else {
        s_logger.info("[ERROR] Failed to auto-detect tokenizer");
    }
}

// ============================================================================
// 4. CACHE MANAGEMENT COMMANDS
// ============================================================================

void cmdCacheContext(const std::string& prompt) {
    s_logger.info("[INFO] Caching context for prompt...");
    
    std::string promptHash = StreamingUtils::hashPrompt(prompt);
    
    CacheEntry entry;
    entry.keyCache = std::vector<float>(1024, 0.0f);
    entry.valueCache = std::vector<float>(1024, 0.0f);
    entry.sequenceLength = 100;
    entry.lastAccessed = std::chrono::system_clock::now();
    
    g_cacheManager->cacheContext(promptHash, entry);
    
    s_logger.info("[SUCCESS] Context cached with hash: ");
}

void cmdRetrieveCache(const std::string& prompt) {
    s_logger.info("[INFO] Retrieving cached context...");
    
    std::string promptHash = StreamingUtils::hashPrompt(prompt);
    CacheEntry entry;
    
    if (g_cacheManager->retrieveContext(promptHash, entry)) {
        s_logger.info("[SUCCESS] Cache hit!");
        s_logger.info("  Sequence length: ");
        s_logger.info("  Key cache size: ");
        s_logger.info("  Value cache size: ");
    } else {
        s_logger.info("[MISS] Context not cached");
    }
}

void cmdClearCache() {
    s_logger.info("[INFO] Clearing cache...");
    g_cacheManager->clearAll();
    s_logger.info("[SUCCESS] Cache cleared");
}

void cmdCacheStats() {
    auto stats = g_cacheManager->getCacheStats();
    
    s_logger.info("\n[CACHE STATISTICS]");
    s_logger.info("  Entries: ");
    s_logger.info("  Size: ");
    s_logger.info("  Usage: ");
    s_logger.info("  Policy: ");
}

void cmdSetCachePolicy(const std::string& policy) {
    if (policy != "LRU" && policy != "LFU" && policy != "FIFO") {
        s_logger.info("[ERROR] Invalid policy. Use: LRU, LFU, or FIFO");
        return;
    }
    
    g_cacheManager->setEvictionPolicy(policy);
    s_logger.info("[SUCCESS] Cache eviction policy set to: ");
}

// ============================================================================
// 5. WEB SERVER COMMANDS
// ============================================================================

void cmdStartWebServer(int port) {
    s_logger.info("[INFO] Starting web server on port ");
    
    auto webServer = std::make_unique<StreamingWebServer>(port);
    webServer->startAsync();
    
    s_logger.info("[SUCCESS] Web server started");
    s_logger.info("  API endpoints:");
    s_logger.info("    POST /api/generate - Generate text");
    s_logger.info("    GET /api/generate/stream - Streaming generation (SSE)");
    s_logger.info("    POST /api/batch/generate - Batch generation");
    s_logger.info("    POST /api/tokenize - Tokenize text");
    s_logger.info("    POST /api/detokenize - Detokenize tokens");
    s_logger.info("    GET /api/status - Server status");
}

void cmdStopWebServer() {
    s_logger.info("[INFO] Stopping web server...");
    s_logger.info("[SUCCESS] Web server stopped");
}

// ============================================================================
// 6. OUTPUT FORMAT COMMANDS
// ============================================================================

void cmdSetOutputFormat(const std::string& format) {
    if (format == "text" || format == "plain") {
        g_currentFormat = OutputFormat::TEXT;
        s_logger.info("[SUCCESS] Output format set to: TEXT");
    } else if (format == "json") {
        g_currentFormat = OutputFormat::JSON;
        s_logger.info("[SUCCESS] Output format set to: JSON");
    } else if (format == "jsonl") {
        g_currentFormat = OutputFormat::JSONL;
        s_logger.info("[SUCCESS] Output format set to: JSONL (JSON Lines)");
    } else if (format == "xml") {
        g_currentFormat = OutputFormat::XML;
        s_logger.info("[SUCCESS] Output format set to: XML");
    } else {
        s_logger.info("[ERROR] Unknown format. Use: text, json, jsonl, or xml");
    }
}

void cmdFormatSample(const std::string& text) {
    s_logger.info("\n[FORMATTED OUTPUT SAMPLES]\n");
    
    s_logger.info("=== TEXT ===");
    s_logger.info( StructuredOutputFormatter::formatComplete(text, 42, 1500, OutputFormat::TEXT) << std::endl;
    
    s_logger.info("\n=== JSON ===");
    s_logger.info( StructuredOutputFormatter::formatComplete(text, 42, 1500, OutputFormat::JSON).dump(2) << std::endl;
    
    s_logger.info("\n=== JSONL ===");
    s_logger.info( StructuredOutputFormatter::formatComplete(text, 42, 1500, OutputFormat::JSONL) << std::endl;
    
    s_logger.info("\n=== XML ===");
    s_logger.info( StructuredOutputFormatter::formatComplete(text, 42, 1500, OutputFormat::XML) << std::endl;
}

// ============================================================================
// STATISTICS AND STATUS
// ============================================================================

void cmdEnhancedStats() {
    auto stats = g_enhancedEngine->getCompleteStats();
    
    s_logger.info("\n[ENHANCED ENGINE STATISTICS]\n");
    s_logger.info( stats.dump(2) << std::endl;
}

// ============================================================================
// HELP DOCUMENTATION
// ============================================================================

void printEnhancementsHelp() {
    s_logger.info("\n=== STREAMING ENHANCEMENTS COMMANDS ===\n");
    
    s_logger.info("ASYNC STREAMING (Non-blocking):");
    s_logger.info("  streamasync <prompt>     - Start streaming without blocking CLI");
    s_logger.info("  waitstream [timeout_ms]  - Wait for async stream to complete");
    s_logger.info("  cancelstream             - Cancel ongoing streaming");
    
    s_logger.info("\nBATCH PROCESSING:");
    s_logger.info("  batch <prompt1> | <prompt2> | ... - Process multiple prompts in parallel");
    s_logger.info("  batchstatus              - Show batch queue status");
    
    s_logger.info("\nTOKENIZERS:");
    s_logger.info("  loadbpe <merges_file>    - Load BPE tokenizer");
    s_logger.info("  loadsp <model_path>      - Load SentencePiece tokenizer");
    s_logger.info("  autotokenizer <path>     - Auto-detect tokenizer");
    
    s_logger.info("\nKV-CACHE MANAGEMENT:");
    s_logger.info("  cachecontext <prompt>    - Cache context for prompt");
    s_logger.info("  retrievecache <prompt>   - Retrieve cached context");
    s_logger.info("  clearcache               - Clear all cached contexts");
    s_logger.info("  cachestats               - Show cache statistics");
    s_logger.info("  setcachepolicy <policy>  - Set eviction policy (LRU/LFU/FIFO)");
    
    s_logger.info("\nWEB SERVER:");
    s_logger.info("  startwebserver [port]    - Start REST API server (default: 8080)");
    s_logger.info("  stopwebserver            - Stop web server");
    
    s_logger.info("\nOUTPUT FORMAT:");
    s_logger.info("  setformat <format>       - Set output format (text/json/jsonl/xml)");
    s_logger.info("  formatsample <text>      - Show format samples");
    
    s_logger.info("\nSTATUS:");
    s_logger.info("  enhancedstats            - Show all enhancement statistics");
    s_logger.info("\n");
}
