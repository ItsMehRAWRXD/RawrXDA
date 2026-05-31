// tls_bridge_smoke.cpp
// Smoke tests for TLS 1.3 Secure Bridge for LLM Reverse Engineering

#include "llm_tls_bridge.hpp"
#include <iostream>
#include <cassert>
#include <chrono>
#include <thread>

using namespace llm_tls;

// Test counters
int tests_passed = 0;
int tests_failed = 0;

#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    std::cout << "Running: " #name "... "; \
    try { \
        test_##name(); \
        std::cout << "PASSED\n"; \
        tests_passed++; \
    } catch (const std::exception& e) { \
        std::cout << "FAILED: " << e.what() << "\n"; \
        tests_failed++; \
    } catch (...) { \
        std::cout << "FAILED: Unknown exception\n"; \
        tests_failed++; \
    } \
} while(0)

#define ASSERT_TRUE(cond) if (!(cond)) throw std::runtime_error("Assertion failed: " #cond)
#define ASSERT_FALSE(cond) if (cond) throw std::runtime_error("Assertion failed: NOT " #cond)
#define ASSERT_EQ(a, b) if ((a) != (b)) throw std::runtime_error("Assertion failed: " #a " == " #b)
#define ASSERT_NE(a, b) if ((a) == (b)) throw std::runtime_error("Assertion failed: " #a " != " #b)

// ═══════════════════════════════════════════════════════════════════════════════
// Utility Function Tests
// ═══════════════════════════════════════════════════════════════════════════════

TEST(base64_encode_decode) {
    // Test empty
    std::vector<uint8_t> empty;
    ASSERT_EQ(base64_encode(empty), "");
    ASSERT_EQ(base64_decode(""), empty);
    
    // Test single byte
    std::vector<uint8_t> single = {0x41}; // 'A'
    std::string encoded = base64_encode(single);
    ASSERT_EQ(encoded, "QQ==");
    auto decoded = base64_decode(encoded);
    ASSERT_EQ(decoded.size(), 1);
    ASSERT_EQ(decoded[0], 0x41);
    
    // Test multiple bytes
    std::vector<uint8_t> data = {0x48, 0x65, 0x6C, 0x6C, 0x6F}; // "Hello"
    encoded = base64_encode(data);
    ASSERT_EQ(encoded, "SGVsbG8=");
    decoded = base64_decode(encoded);
    ASSERT_EQ(decoded.size(), 5);
    ASSERT_EQ(decoded[0], 'H');
    ASSERT_EQ(decoded[1], 'e');
    ASSERT_EQ(decoded[2], 'l');
    ASSERT_EQ(decoded[3], 'l');
    ASSERT_EQ(decoded[4], 'o');
    
    // Test roundtrip with random data
    std::vector<uint8_t> random_data;
    for (int i = 0; i < 256; i++) {
        random_data.push_back(static_cast<uint8_t>(i));
    }
    encoded = base64_encode(random_data);
    decoded = base64_decode(encoded);
    ASSERT_EQ(decoded.size(), random_data.size());
    for (size_t i = 0; i < decoded.size(); i++) {
        ASSERT_EQ(decoded[i], random_data[i]);
    }
}

TEST(hex_encode_decode) {
    // Test empty
    std::vector<uint8_t> empty;
    ASSERT_EQ(hex_encode(empty), "");
    ASSERT_EQ(hex_decode(""), empty);
    
    // Test single byte
    std::vector<uint8_t> single = {0xFF};
    ASSERT_EQ(hex_encode(single), "ff");
    auto decoded = hex_decode("ff");
    ASSERT_EQ(decoded.size(), 1);
    ASSERT_EQ(decoded[0], 0xFF);
    
    // Test uppercase
    decoded = hex_decode("FF");
    ASSERT_EQ(decoded.size(), 1);
    ASSERT_EQ(decoded[0], 0xFF);
    
    // Test multiple bytes
    std::vector<uint8_t> data = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
    std::string encoded = hex_encode(data);
    ASSERT_EQ(encoded, "0123456789abcdef");
    decoded = hex_decode(encoded);
    ASSERT_EQ(decoded.size(), 8);
    for (size_t i = 0; i < data.size(); i++) {
        ASSERT_EQ(decoded[i], data[i]);
    }
}

TEST(sha256_hash) {
    // Test empty
    std::vector<uint8_t> empty;
    auto hash = sha256(empty);
    ASSERT_EQ(hash.size(), 32);
    
    // Known SHA256 hash of empty string
    std::string empty_hash = hex_encode(hash);
    ASSERT_EQ(empty_hash, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    
    // Test "abc"
    std::vector<uint8_t> abc = {'a', 'b', 'c'};
    hash = sha256(abc);
    std::string abc_hash = hex_encode(hash);
    ASSERT_EQ(abc_hash, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    
    // Test sha256_hex convenience function
    std::string hex_hash = sha256_hex("abc");
    ASSERT_EQ(hex_hash, abc_hash);
}

TEST(tls_version_string_conversion) {
    ASSERT_EQ(tls_version_string(0x0301), "TLS 1.0");
    ASSERT_EQ(tls_version_string(0x0302), "TLS 1.1");
    ASSERT_EQ(tls_version_string(0x0303), "TLS 1.2");
    ASSERT_EQ(tls_version_string(0x0304), "TLS 1.3");
    ASSERT_EQ(tls_version_string(0x0000), "Unknown");
}

// ═══════════════════════════════════════════════════════════════════════════════
// TLSConfig Tests
// ═══════════════════════════════════════════════════════════════════════════════

TEST(tls_config_defaults) {
    TLSConfig config;
    
    ASSERT_TRUE(config.verify_peer);
    ASSERT_FALSE(config.allow_self_signed);
    ASSERT_FALSE(config.enable_session_resumption);
    ASSERT_EQ(config.min_tls_version, 0x0303); // TLS 1.2
    ASSERT_EQ(config.max_tls_version, 0x0304); // TLS 1.3
    ASSERT_TRUE(config.ca_cert_path.empty());
    ASSERT_TRUE(config.client_cert_path.empty());
    ASSERT_TRUE(config.client_key_path.empty());
}

TEST(tls_config_custom) {
    TLSConfig config;
    config.verify_peer = false;
    config.allow_self_signed = true;
    config.min_tls_version = 0x0304; // TLS 1.3 only
    config.ca_cert_path = "/path/to/ca.pem";
    config.client_cert_path = "/path/to/client.pem";
    config.client_key_path = "/path/to/client.key";
    
    ASSERT_FALSE(config.verify_peer);
    ASSERT_TRUE(config.allow_self_signed);
    ASSERT_EQ(config.min_tls_version, 0x0304);
    ASSERT_EQ(config.ca_cert_path, "/path/to/ca.pem");
    ASSERT_EQ(config.client_cert_path, "/path/to/client.pem");
    ASSERT_EQ(config.client_key_path, "/path/to/client.key");
}

// ═══════════════════════════════════════════════════════════════════════════════
// LLMEndpoint Tests
// ═══════════════════════════════════════════════════════════════════════════════

TEST(llm_endpoint_defaults) {
    LLMEndpoint endpoint;
    
    ASSERT_TRUE(endpoint.host.empty());
    ASSERT_EQ(endpoint.port, 443);
    ASSERT_TRUE(endpoint.api_path.empty());
    ASSERT_TRUE(endpoint.api_key.empty());
    ASSERT_EQ(endpoint.provider, LLMProvider::OpenAI);
    ASSERT_TRUE(endpoint.use_tls);
    ASSERT_FALSE(endpoint.use_mtls);
}

TEST(llm_endpoint_openai) {
    LLMEndpoint endpoint;
    endpoint.host = "api.openai.com";
    endpoint.port = 443;
    endpoint.api_path = "/v1/chat/completions";
    endpoint.api_key = "sk-test-key";
    endpoint.provider = LLMProvider::OpenAI;
    
    ASSERT_EQ(endpoint.host, "api.openai.com");
    ASSERT_EQ(endpoint.port, 443);
    ASSERT_EQ(endpoint.api_path, "/v1/chat/completions");
    ASSERT_EQ(endpoint.provider, LLMProvider::OpenAI);
}

TEST(llm_endpoint_anthropic) {
    LLMEndpoint endpoint;
    endpoint.host = "api.anthropic.com";
    endpoint.port = 443;
    endpoint.api_path = "/v1/messages";
    endpoint.provider = LLMProvider::Anthropic;
    
    ASSERT_EQ(endpoint.host, "api.anthropic.com");
    ASSERT_EQ(endpoint.provider, LLMProvider::Anthropic);
}

TEST(llm_endpoint_local) {
    LLMEndpoint endpoint;
    endpoint.host = "localhost";
    endpoint.port = 8080;
    endpoint.provider = LLMProvider::Local;
    endpoint.use_tls = false;
    
    ASSERT_EQ(endpoint.host, "localhost");
    ASSERT_EQ(endpoint.port, 8080);
    ASSERT_EQ(endpoint.provider, LLMProvider::Local);
    ASSERT_FALSE(endpoint.use_tls);
}

// ═══════════════════════════════════════════════════════════════════════════════
// InferenceRequest Tests
// ═══════════════════════════════════════════════════════════════════════════════

TEST(inference_request_defaults) {
    InferenceRequest request;
    
    ASSERT_TRUE(request.model.empty());
    ASSERT_EQ(request.max_tokens, 1000);
    ASSERT_EQ(request.temperature, 0.7f);
    ASSERT_FALSE(request.stream);
    ASSERT_TRUE(request.messages.empty());
    ASSERT_TRUE(request.stop_sequences.empty());
}

TEST(inference_request_with_messages) {
    InferenceRequest request;
    request.model = "gpt-4o";
    request.max_tokens = 2000;
    request.temperature = 0.5f;
    request.stream = true;
    
    std::map<std::string, std::string> msg1;
    msg1["role"] = "system";
    msg1["content"] = "You are a helpful assistant.";
    request.messages.push_back(msg1);
    
    std::map<std::string, std::string> msg2;
    msg2["role"] = "user";
    msg2["content"] = "Hello!";
    request.messages.push_back(msg2);
    
    ASSERT_EQ(request.model, "gpt-4o");
    ASSERT_EQ(request.max_tokens, 2000);
    ASSERT_EQ(request.temperature, 0.5f);
    ASSERT_TRUE(request.stream);
    ASSERT_EQ(request.messages.size(), 2);
    ASSERT_EQ(request.messages[0]["role"], "system");
    ASSERT_EQ(request.messages[1]["content"], "Hello!");
}

// ═══════════════════════════════════════════════════════════════════════════════
// InferenceResponse Tests
// ═══════════════════════════════════════════════════════════════════════════════

TEST(inference_response_defaults) {
    InferenceResponse response;
    
    ASSERT_FALSE(response.success);
    ASSERT_TRUE(response.error.empty());
    ASSERT_TRUE(response.content.empty());
    ASSERT_EQ(response.input_tokens, 0);
    ASSERT_EQ(response.output_tokens, 0);
    ASSERT_EQ(response.latency.count(), static_cast<long long>(0));
}

// ═══════════════════════════════════════════════════════════════════════════════
// LLMTLSBridge Tests
// ═══════════════════════════════════════════════════════════════════════════════

TEST(bridge_initialization) {
    LLMTLSBridge bridge;
    
    ASSERT_FALSE(bridge.is_initialized());
    
    TLSConfig config;
    ASSERT_TRUE(bridge.initialize(config));
    ASSERT_TRUE(bridge.is_initialized());
    
    bridge.shutdown();
    ASSERT_FALSE(bridge.is_initialized());
}

TEST(bridge_stats) {
    LLMTLSBridge bridge;
    
    TLSConfig config;
    bridge.initialize(config);
    
    auto stats = bridge.get_stats();
    ASSERT_TRUE(stats.count("bytes_sent") > 0);
    ASSERT_TRUE(stats.count("bytes_received") > 0);
    ASSERT_TRUE(stats.count("request_count") > 0);
    
    ASSERT_EQ(bridge.get_bytes_sent(), 0);
    ASSERT_EQ(bridge.get_bytes_received(), 0);
    ASSERT_EQ(bridge.get_request_count(), 0);
}

TEST(bridge_traffic_capture) {
    LLMTLSBridge bridge;
    
    TLSConfig config;
    bridge.initialize(config);
    
    // Enable traffic capture
    bridge.enable_traffic_capture(true);
    
    // Get captured frames (should be empty initially)
    auto frames = bridge.get_captured_frames();
    ASSERT_TRUE(frames.empty());
    
    // Get traffic analysis
    auto analysis = bridge.get_traffic_analysis();
    ASSERT_EQ(analysis.total_frames, 0);
    
    // Disable traffic capture
    bridge.enable_traffic_capture(false);
}

TEST(bridge_protocol_analysis) {
    LLMTLSBridge bridge;
    
    TLSConfig config;
    bridge.initialize(config);
    
    // Analyze OpenAI protocol
    auto openai = bridge.analyze_openai_protocol();
    ASSERT_TRUE(openai.count("endpoint") > 0);
    ASSERT_TRUE(openai.count("protocol") > 0);
    ASSERT_TRUE(openai.count("auth") > 0);
    ASSERT_EQ(openai["endpoint"], "api.openai.com:443");
    ASSERT_EQ(openai["protocol"], "HTTPS/REST");
    
    // Analyze Anthropic protocol
    auto anthropic = bridge.analyze_anthropic_protocol();
    ASSERT_EQ(anthropic["endpoint"], "api.anthropic.com:443");
    
    // Analyze Google protocol
    auto google = bridge.analyze_google_protocol();
    ASSERT_EQ(google["endpoint"], "generativelanguage.googleapis.com:443");
}

TEST(bridge_certificate_management) {
    LLMTLSBridge bridge;
    
    TLSConfig config;
    bridge.initialize(config);
    
    // Test certificate pinning
    ASSERT_TRUE(bridge.pin_certificate("sha256://ABC123"));
    
    // Test CA certificate loading (will fail with non-existent path)
    ASSERT_FALSE(bridge.load_ca_certificates("/nonexistent/path"));
    
    // Test client certificate loading (will fail with non-existent paths)
    ASSERT_FALSE(bridge.load_client_certificate("/nonexistent/cert", "/nonexistent/key"));
}

TEST(bridge_build_request) {
    LLMTLSBridge bridge;
    
    TLSConfig config;
    bridge.initialize(config);
    
    InferenceRequest request;
    request.model = "gpt-4o";
    request.max_tokens = 100;
    request.temperature = 0.5f;
    
    std::map<std::string, std::string> msg;
    msg["role"] = "user";
    msg["content"] = "Hello";
    request.messages.push_back(msg);
    
    std::string json_request = bridge.build_request(request);
    
    // Verify JSON contains expected fields
    ASSERT_TRUE(json_request.find("\"model\":\"gpt-4o\"") != std::string::npos);
    ASSERT_TRUE(json_request.find("\"max_tokens\":100") != std::string::npos);
    ASSERT_TRUE(json_request.find("\"temperature\":0.5") != std::string::npos);
    ASSERT_TRUE(json_request.find("\"role\":\"user\"") != std::string::npos);
    ASSERT_TRUE(json_request.find("\"content\":\"Hello\"") != std::string::npos);
}

// ═══════════════════════════════════════════════════════════════════════════════
// SecureChannel Tests (without actual network connections)
// ═══════════════════════════════════════════════════════════════════════════════

TEST(secure_channel_creation) {
    SecureChannel channel;
    
    ASSERT_FALSE(channel.is_connected());
}

TEST(secure_channel_disconnect_without_connect) {
    SecureChannel channel;
    
    // Should not crash when disconnecting without connecting
    channel.disconnect();
    ASSERT_FALSE(channel.is_connected());
}

// ═══════════════════════════════════════════════════════════════════════════════
// ProtocolFrame Tests
// ═══════════════════════════════════════════════════════════════════════════════

TEST(protocol_frame_defaults) {
    ProtocolFrame frame;
    
    ASSERT_EQ(frame.type, 0);
    ASSERT_EQ(frame.flags, 0);
    ASSERT_EQ(frame.version, 0);
    ASSERT_EQ(frame.length, 0);
    ASSERT_TRUE(frame.payload.empty());
}

TEST(protocol_frame_custom) {
    ProtocolFrame frame;
    frame.type = 0x01;
    frame.flags = 0x02;
    frame.version = 0x0304;
    frame.length = 1024;
    frame.payload = {0x01, 0x02, 0x03};
    
    ASSERT_EQ(frame.type, 0x01);
    ASSERT_EQ(frame.flags, 0x02);
    ASSERT_EQ(frame.version, 0x0304);
    ASSERT_EQ(frame.length, 1024);
    ASSERT_EQ(frame.payload.size(), 3);
}

// ═══════════════════════════════════════════════════════════════════════════════
// TrafficAnalysis Tests
// ═══════════════════════════════════════════════════════════════════════════════

TEST(traffic_analysis_defaults) {
    TrafficAnalysis analysis;
    
    ASSERT_EQ(analysis.total_bytes_sent, 0);
    ASSERT_EQ(analysis.total_bytes_received, 0);
    ASSERT_EQ(analysis.request_count, 0);
    ASSERT_EQ(analysis.response_count, 0);
    ASSERT_EQ(analysis.avg_latency_ms, 0.0f);
    ASSERT_TRUE(analysis.captured_frames.empty());
}

// ═══════════════════════════════════════════════════════════════════════════════
// CertificateInfo Tests
// ═══════════════════════════════════════════════════════════════════════════════

TEST(certificate_info_defaults) {
    CertificateInfo cert;
    
    ASSERT_TRUE(cert.subject.empty());
    ASSERT_TRUE(cert.issuer.empty());
    ASSERT_TRUE(cert.serial_number.empty());
    ASSERT_TRUE(cert.fingerprint_sha256.empty());
    ASSERT_TRUE(cert.public_key_type.empty());
    ASSERT_EQ(cert.public_key_bits, 0);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Main Test Runner
// ═══════════════════════════════════════════════════════════════════════════════

int main() {
    std::cout << "\n";
    std::cout << "╔═══════════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║         TLS Bridge Smoke Tests - LLM Reverse Engineering                 ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    
    // Utility function tests
    std::cout << "=== Utility Functions ===\n";
    RUN_TEST(base64_encode_decode);
    RUN_TEST(hex_encode_decode);
    RUN_TEST(sha256_hash);
    RUN_TEST(tls_version_string_conversion);
    
    // Configuration tests
    std::cout << "\n=== Configuration Tests ===\n";
    RUN_TEST(tls_config_defaults);
    RUN_TEST(tls_config_custom);
    
    // Endpoint tests
    std::cout << "\n=== LLM Endpoint Tests ===\n";
    RUN_TEST(llm_endpoint_defaults);
    RUN_TEST(llm_endpoint_openai);
    RUN_TEST(llm_endpoint_anthropic);
    RUN_TEST(llm_endpoint_local);
    
    // Request/Response tests
    std::cout << "\n=== Request/Response Tests ===\n";
    RUN_TEST(inference_request_defaults);
    RUN_TEST(inference_request_with_messages);
    RUN_TEST(inference_response_defaults);
    
    // Bridge tests
    std::cout << "\n=== LLMTLSBridge Tests ===\n";
    RUN_TEST(bridge_initialization);
    RUN_TEST(bridge_stats);
    RUN_TEST(bridge_traffic_capture);
    RUN_TEST(bridge_protocol_analysis);
    RUN_TEST(bridge_certificate_management);
    RUN_TEST(bridge_build_request);
    
    // Channel tests
    std::cout << "\n=== SecureChannel Tests ===\n";
    RUN_TEST(secure_channel_creation);
    RUN_TEST(secure_channel_disconnect_without_connect);
    
    // Protocol tests
    std::cout << "\n=== Protocol Tests ===\n";
    RUN_TEST(protocol_frame_defaults);
    RUN_TEST(protocol_frame_custom);
    RUN_TEST(traffic_analysis_defaults);
    RUN_TEST(certificate_info_defaults);
    
    // Summary
    std::cout << "\n";
    std::cout << "╔═══════════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                         TEST RESULTS SUMMARY                              ║\n";
    std::cout << "╠═══════════════════════════════════════════════════════════════════════════╣\n";
    std::cout << "║  Tests Passed: " << std::setw(3) << tests_passed << "                                                      ║\n";
    std::cout << "║  Tests Failed: " << std::setw(3) << tests_failed << "                                                      ║\n";
    std::cout << "╠═══════════════════════════════════════════════════════════════════════════╣\n";
    
    if (tests_failed == 0) {
        std::cout << "║  STATUS: ALL TESTS PASSED ✓                                               ║\n";
    } else {
        std::cout << "║  STATUS: SOME TESTS FAILED ✗                                              ║\n";
    }
    
    std::cout << "╚═══════════════════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    
    return tests_failed > 0 ? 1 : 0;
}
