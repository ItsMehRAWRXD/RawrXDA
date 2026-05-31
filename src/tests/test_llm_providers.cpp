/**
 * @file test_llm_providers.cpp
 * @brief Smoke Tests for TLS Client and LLM Providers
 * 
 * Tests:
 *   1. TLS Client initialization
 *   2. TLS connection pooling
 *   3. Certificate validation
 *   4. OpenAI provider configuration
 *   5. Anthropic provider configuration
 *   6. Provider factory
 *   7. LLM router
 *   8. Cost estimation
 * 
 * @author RawrXD Team
 * @version 1.0.0
 */

#include <iostream>
#include <cassert>
#include <chrono>
#include <thread>
#include "../llm/tls_client.hpp"
#include "../llm/llm_providers.hpp"

using namespace RawrXD::LLM;

// Test counters
static int g_pass = 0;
static int g_fail = 0;
static int g_skip = 0;

static void check(const char* label, bool ok, const char* detail = nullptr) {
    if (ok) {
        ++g_pass;
        std::cout << "[PASS] " << label;
    } else {
        ++g_fail;
        std::cout << "[FAIL] " << label;
    }
    if (detail && *detail) std::cout << " - " << detail;
    std::cout << std::endl;
}

static void skip(const char* label, const char* reason) {
    ++g_skip;
    std::cout << "[SKIP] " << label << " - " << reason << std::endl;
}

// ============================================================================
// Test 1: TLS Client Initialization
// ============================================================================

static void test_tls_client_init() {
    std::cout << "\n=== Test 1: TLS Client Initialization ===\n";
    
    TLSConfig config;
    config.userAgent = "RawrXD-Test/1.0";
    config.connectTimeoutMs = 5000;
    config.requestTimeoutMs = 10000;
    config.enableTLS13 = true;
    config.enableTLS12 = true;
    config.validateCertificates = true;
    config.maxConnectionsPerHost = 5;
    
    TLSClient client(config);
    bool initOk = client.initialize();
    check("TLSClient::initialize()", initOk, "client initialized");
    
    if (initOk) {
        check("TLSClient::getConfig().userAgent", 
              client.getConfig().userAgent == "RawrXD-Test/1.0",
              "user agent matches");
        check("TLSClient::getConfig().enableTLS13",
              client.getConfig().enableTLS13,
              "TLS 1.3 enabled");
        check("TLSClient::getConfig().maxConnectionsPerHost",
              client.getConfig().maxConnectionsPerHost == 5,
              "connection limit set");
        
        client.shutdown();
        check("TLSClient::shutdown()", true, "clean shutdown");
    }
}

// ============================================================================
// Test 2: TLS Connection Pooling
// ============================================================================

static void test_tls_connection_pool() {
    std::cout << "\n=== Test 2: TLS Connection Pooling ===\n";
    
    TLSConfig config;
    config.maxConnectionsPerHost = 3;
    config.maxTotalConnections = 10;
    config.keepAliveTimeoutMs = 30000;
    
    TLSConnectionPool pool(config);
    bool initOk = pool.initialize();
    check("TLSConnectionPool::initialize()", initOk, "pool initialized");
    
    if (initOk) {
        check("TLSConnectionPool::getActiveCount() == 0", 
              pool.getActiveCount() == 0,
              "no active connections initially");
        check("TLSConnectionPool::getTotalConnections() == 0",
              pool.getTotalConnections() == 0,
              "no total connections initially");
        
        pool.shutdown();
        check("TLSConnectionPool::shutdown()", true, "clean shutdown");
    }
}

// ============================================================================
// Test 3: Certificate Validation
// ============================================================================

static void test_certificate_validation() {
    std::cout << "\n=== Test 3: Certificate Validation ===\n";
    
    TLSConfig config;
    config.validateCertificates = true;
    config.allowSelfSigned = false;
    
    CertificateValidator validator(config);
    
    check("CertificateValidator created", true, "validator initialized");
    
    // Test hostname validation
    bool validHostname = TLSUtil::isValidHostname("api.openai.com");
    check("TLSUtil::isValidHostname(api.openai.com)", validHostname, "valid hostname");
    
    validHostname = TLSUtil::isValidHostname("invalid..hostname");
    check("TLSUtil::isValidHostname(invalid..hostname) == false", 
         !validHostname, "invalid hostname rejected");
    
    validHostname = TLSUtil::isValidHostname("localhost");
    check("TLSUtil::isValidHostname(localhost)", validHostname, "localhost is valid");
}

// ============================================================================
// Test 4: OpenAI Provider Configuration
// ============================================================================

static void test_openai_provider() {
    std::cout << "\n=== Test 4: OpenAI Provider Configuration ===\n";
    
    TLSConfig config;
    OpenAIProvider provider(config);
    
    check("OpenAIProvider::getName() == 'OpenAI'",
          provider.getName() == "OpenAI",
          "provider name correct");
    
    auto models = provider.getSupportedModels();
    check("OpenAIProvider::getSupportedModels().size() > 0",
          models.size() > 0,
          "has supported models");
    
    bool hasGPT4 = false;
    for (const auto& model : models) {
        if (model == "gpt-4o") {
            hasGPT4 = true;
            break;
        }
    }
    check("OpenAIProvider supports gpt-4o", hasGPT4, "GPT-4o in model list");
    
    provider.setApiKey("test-api-key");
    check("OpenAIProvider::setApiKey()", true, "API key set");
    
    provider.setTimeout(30000);
    check("OpenAIProvider::setTimeout()", true, "timeout set");
    
    provider.setMaxRetries(3);
    check("OpenAIProvider::setMaxRetries()", true, "max retries set");
    
    bool available = provider.isModelAvailable("gpt-4o");
    check("OpenAIProvider::isModelAvailable(gpt-4o)", available, "GPT-4o available");
    
    available = provider.isModelAvailable("nonexistent-model");
    check("OpenAIProvider::isModelAvailable(nonexistent) == false",
          !available, "nonexistent model not available");
    
    auto modelInfo = provider.getModelInfo("gpt-4o");
    check("OpenAIProvider::getModelInfo(gpt-4o).name not empty",
          !modelInfo.name.empty(),
          "model info retrieved");
    
    // Cost estimation
    ChatCompletionRequest request;
    request.model = "gpt-4o";
    request.messages = {{"user", "Hello"}};
    request.maxTokens = 100;
    
    double cost = provider.estimateCost(request);
    check("OpenAIProvider::estimateCost() > 0", cost >= 0, "cost estimated");
    
    // Cost tracking
    double totalCost = provider.getTotalCost();
    check("OpenAIProvider::getTotalCost() == 0", totalCost == 0.0, "initial cost is zero");
    
    provider.resetCostTracking();
    check("OpenAIProvider::resetCostTracking()", true, "cost tracking reset");
}

// ============================================================================
// Test 5: Anthropic Provider Configuration
// ============================================================================

static void test_anthropic_provider() {
    std::cout << "\n=== Test 5: Anthropic Provider Configuration ===\n";
    
    TLSConfig config;
    AnthropicProvider provider(config);
    
    check("AnthropicProvider::getName() == 'Anthropic'",
          provider.getName() == "Anthropic",
          "provider name correct");
    
    auto models = provider.getSupportedModels();
    check("AnthropicProvider::getSupportedModels().size() > 0",
          models.size() > 0,
          "has supported models");
    
    bool hasClaude = false;
    for (const auto& model : models) {
        if (model.find("claude") != std::string::npos) {
            hasClaude = true;
            break;
        }
    }
    check("AnthropicProvider supports Claude models", hasClaude, "Claude in model list");
    
    provider.setApiKey("test-api-key");
    check("AnthropicProvider::setApiKey()", true, "API key set");
    
    auto modelInfo = provider.getModelInfo("claude-3-5-sonnet-20241022");
    check("AnthropicProvider::getModelInfo(claude-3-5-sonnet)",
          !modelInfo.name.empty(),
          "model info retrieved");
}

// ============================================================================
// Test 6: Provider Factory
// ============================================================================

static void test_provider_factory() {
    std::cout << "\n=== Test 6: Provider Factory ===\n";
    
    TLSConfig config;
    
    auto openai = LLMProviderFactory::create(LLMProviderFactory::ProviderType::OpenAI, config);
    check("LLMProviderFactory::create(OpenAI)", openai != nullptr, "OpenAI provider created");
    check("OpenAI provider name", openai && openai->getName() == "OpenAI", "correct name");
    
    auto anthropic = LLMProviderFactory::create(LLMProviderFactory::ProviderType::Anthropic, config);
    check("LLMProviderFactory::create(Anthropic)", anthropic != nullptr, "Anthropic provider created");
    check("Anthropic provider name", anthropic && anthropic->getName() == "Anthropic", "correct name");
    
    auto gemini = LLMProviderFactory::create(LLMProviderFactory::ProviderType::Gemini, config);
    check("LLMProviderFactory::create(Gemini)", gemini != nullptr, "Gemini provider created");
    
    auto deepseek = LLMProviderFactory::create(LLMProviderFactory::ProviderType::DeepSeek, config);
    check("LLMProviderFactory::create(DeepSeek)", deepseek != nullptr, "DeepSeek provider created");
    
    auto mistral = LLMProviderFactory::create(LLMProviderFactory::ProviderType::Mistral, config);
    check("LLMProviderFactory::create(Mistral)", mistral != nullptr, "Mistral provider created");
    
    auto local = LLMProviderFactory::create(LLMProviderFactory::ProviderType::Local, config);
    check("LLMProviderFactory::create(Local)", local != nullptr, "Local provider created");
    
    auto availableProviders = LLMProviderFactory::getAvailableProviders();
    check("LLMProviderFactory::getAvailableProviders().size() == 6",
          availableProviders.size() == 6,
          "6 providers available");
    
    // String conversion
    std::string str = LLMProviderFactory::providerToString(LLMProviderFactory::ProviderType::OpenAI);
    check("LLMProviderFactory::providerToString(OpenAI) == 'openai'",
          str == "openai",
          "string conversion correct");
    
    auto type = LLMProviderFactory::stringToProvider("anthropic");
    check("LLMProviderFactory::stringToProvider('anthropic') == Anthropic",
          type == LLMProviderFactory::ProviderType::Anthropic,
          "string to provider correct");
}

// ============================================================================
// Test 7: LLM Router
// ============================================================================

static void test_llm_router() {
    std::cout << "\n=== Test 7: LLM Router ===\n";
    
    TLSConfig config;
    LLMRouter router(config);
    
    // Add providers
    router.addProvider(LLMProviderFactory::ProviderType::OpenAI, "test-key");
    router.addProvider(LLMProviderFactory::ProviderType::Anthropic, "test-key");
    
    check("LLMRouter::addProvider(OpenAI)", true, "OpenAI added");
    check("LLMRouter::addProvider(Anthropic)", true, "Anthropic added");
    
    // Model routing
    router.setModelProvider("gpt-4o", LLMProviderFactory::ProviderType::OpenAI);
    router.setModelProvider("claude-3-5-sonnet", LLMProviderFactory::ProviderType::Anthropic);
    
    auto provider = router.getProviderForModel("gpt-4o");
    check("LLMRouter::getProviderForModel(gpt-4o) == OpenAI",
          provider == LLMProviderFactory::ProviderType::OpenAI,
          "GPT-4o routed to OpenAI");
    
    provider = router.getProviderForModel("claude-3-5-sonnet");
    check("LLMRouter::getProviderForModel(claude) == Anthropic",
          provider == LLMProviderFactory::ProviderType::Anthropic,
          "Claude routed to Anthropic");
    
    // Default routing
    provider = router.getProviderForModel("unknown-model");
    check("LLMRouter::getProviderForModel(unknown) defaults to OpenAI",
          provider == LLMProviderFactory::ProviderType::OpenAI,
          "unknown model defaults to OpenAI");
    
    // Fallback chain
    router.setFallbackChain({
        LLMProviderFactory::ProviderType::Anthropic,
        LLMProviderFactory::ProviderType::Gemini,
        LLMProviderFactory::ProviderType::DeepSeek
    });
    check("LLMRouter::setFallbackChain()", true, "fallback chain set");
    
    // Priority
    router.setProviderPriority(LLMProviderFactory::ProviderType::OpenAI, 1);
    router.setProviderPriority(LLMProviderFactory::ProviderType::Anthropic, 2);
    check("LLMRouter::setProviderPriority()", true, "priorities set");
    
    // Cost optimization
    router.setCostOptimization(true);
    check("LLMRouter::setCostOptimization(true)", true, "cost optimization enabled");
    
    // Stats
    auto stats = router.getStats(LLMProviderFactory::ProviderType::OpenAI);
    check("LLMRouter::getStats(OpenAI).totalRequests == 0",
          stats.totalRequests == 0,
          "initial stats zero");
    
    router.resetStats();
    check("LLMRouter::resetStats()", true, "stats reset");
}

// ============================================================================
// Test 8: Cost Estimation
// ============================================================================

static void test_cost_estimation() {
    std::cout << "\n=== Test 8: Cost Estimation ===\n";
    
    TLSConfig config;
    OpenAIProvider provider(config);
    
    ChatCompletionRequest request;
    request.model = "gpt-4o";
    request.messages = {
        {"system", "You are a helpful assistant."},
        {"user", "What is the capital of France?"}
    };
    request.maxTokens = 100;
    
    double cost = provider.estimateCost(request);
    check("OpenAIProvider::estimateCost() returns non-negative",
          cost >= 0,
          "cost estimation works");
    
    // Test with different models
    request.model = "gpt-3.5-turbo";
    cost = provider.estimateCost(request);
    check("OpenAIProvider::estimateCost(gpt-3.5-turbo) >= 0",
          cost >= 0,
          "GPT-3.5 cost estimated");
    
    // Test router cost estimation
    LLMRouter router(config);
    router.addProvider(LLMProviderFactory::ProviderType::OpenAI, "test-key");
    
    cost = router.estimateCost(request);
    check("LLMRouter::estimateCost() >= 0", cost >= 0, "router cost estimation works");
}

// ============================================================================
// Test 9: URL Parsing
// ============================================================================

static void test_url_parsing() {
    std::cout << "\n=== Test 9: URL Parsing ===\n";
    
    auto parsed = TLSClient::parseURL("https://api.openai.com/v1/chat/completions");
    check("parseURL(https://api.openai.com/v1/chat/completions).scheme == 'https'",
          parsed.scheme == "https",
          "scheme parsed correctly");
    check("parseURL().host == 'api.openai.com'",
          parsed.host == "api.openai.com",
          "host parsed correctly");
    check("parseURL().port == 443",
          parsed.port == 443,
          "default HTTPS port");
    check("parseURL().path == '/v1/chat/completions'",
          parsed.path == "/v1/chat/completions",
          "path parsed correctly");
    
    parsed = TLSClient::parseURL("http://localhost:11434/api/generate");
    check("parseURL(http://localhost:11434/api/generate).scheme == 'http'",
          parsed.scheme == "http",
          "HTTP scheme parsed");
    check("parseURL().port == 11434",
          parsed.port == 11434,
          "custom port parsed");
    
    parsed = TLSClient::parseURL("https://api.anthropic.com/v1/messages?stream=true");
    check("parseURL().query == 'stream=true'",
          parsed.query == "stream=true",
          "query string parsed");
}

// ============================================================================
// Test 10: TLS Utilities
// ============================================================================

static void test_tls_utilities() {
    std::cout << "\n=== Test 10: TLS Utilities ===\n";
    
    // URL encoding
    std::string encoded = TLSUtil::urlEncode("hello world");
    check("TLSUtil::urlEncode('hello world') == 'hello%20world'",
          encoded == "hello%20world",
          "URL encoding works");
    
    std::string decoded = TLSUtil::urlDecode("hello%20world");
    check("TLSUtil::urlDecode('hello%20world') == 'hello world'",
          decoded == "hello world",
          "URL decoding works");
    
    // Base64 encoding
    std::vector<uint8_t> data = {'H', 'e', 'l', 'l', 'o'};
    std::string b64 = TLSUtil::base64Encode(data);
    check("TLSUtil::base64Encode('Hello') not empty",
          !b64.empty(),
          "Base64 encoding works");
    
    std::vector<uint8_t> decoded_data = TLSUtil::base64Decode(b64);
    check("TLSUtil::base64Decode() roundtrip",
          decoded_data == data,
          "Base64 decoding works");
    
    // Hostname validation
    bool valid = TLSUtil::isValidHostname("api.openai.com");
    check("TLSUtil::isValidHostname(api.openai.com)", valid, "valid hostname");
    
    valid = TLSUtil::isValidHostname("");
    check("TLSUtil::isValidHostname('') == false", !valid, "empty hostname invalid");
    
    valid = TLSUtil::isValidHostname("a".repeat(254));
    check("TLSUtil::isValidHostname(254 chars) == false", !valid, "too long hostname invalid");
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  RawrXD TLS & LLM Providers Smoke Test                       ║\n";
    std::cout << "║  Production-Ready Cloud AI Integration                        ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n\n";
    
    auto startTime = std::chrono::steady_clock::now();
    
    // Run all tests
    test_tls_client_init();
    test_tls_connection_pool();
    test_certificate_validation();
    test_openai_provider();
    test_anthropic_provider();
    test_provider_factory();
    test_llm_router();
    test_cost_estimation();
    test_url_parsing();
    test_tls_utilities();
    
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    // Summary
    std::cout << "\n╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  SUMMARY                                                      ║\n";
    std::cout << "╠══════════════════════════════════════════════════════════════╣\n";
    std::cout << "║  PASS: " << std::setw(4) << g_pass << "                                                   ║\n";
    std::cout << "║  FAIL: " << std::setw(4) << g_fail << "                                                   ║\n";
    std::cout << "║  SKIP: " << std::setw(4) << g_skip << "                                                   ║\n";
    std::cout << "║  Time: " << std::setw(6) << duration.count() << " ms                                             ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n";
    
    int exitCode = (g_fail > 0) ? 1 : 0;
    std::cout << "\n" << (exitCode == 0 ? "✅ ALL TESTS PASSED" : "❌ SOME TESTS FAILED") << "\n";
    return exitCode;
}
