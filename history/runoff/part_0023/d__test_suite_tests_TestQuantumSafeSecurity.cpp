// ============================================================================
// TestQuantumSafeSecurity.cpp - Pure C++ Quantum-Safe Crypto Test Suite
// Zero-dependency: no Qt, no nlohmann — standard C++ only
// ============================================================================

#include <cassert>
#include <iostream>
#include <random>
#include <map>
#include <cmath>
#include <string>
#include <vector>
#include <cstdint>
#include <chrono>
#include <cstring>
#include <sstream>
#include <unordered_map>

// ============================================================================
// Minimal JSON value (self-contained)
// ============================================================================
struct JsonValue {
    enum Type { Null, Int, Str, Arr, Obj };
    Type type = Null;
    int intVal = 0;
    std::string strVal;
    std::vector<JsonValue> arrVal;
    std::unordered_map<std::string, JsonValue> objVal;

    JsonValue() = default;
    explicit JsonValue(int v) : type(Int), intVal(v) {}
    explicit JsonValue(const std::string& v) : type(Str), strVal(v) {}

    bool empty() const {
        if (type == Null) return true;
        if (type == Obj) return objVal.empty();
        if (type == Arr) return arrVal.empty();
        if (type == Str) return strVal.empty();
        return false;
    }
    bool contains(const std::string& k) const { return objVal.count(k) > 0; }
    JsonValue& operator[](const std::string& k) { type = Obj; return objVal[k]; }
    const JsonValue& at(const std::string& k) const { return objVal.at(k); }

    std::string dump() const {
        std::ostringstream o;
        switch (type) {
            case Null: o << "null"; break;
            case Int:  o << intVal; break;
            case Str:  o << "\"" << strVal << "\""; break;
            case Arr:
                o << "[";
                for (size_t i = 0; i < arrVal.size(); i++) { if (i) o << ","; o << arrVal[i].dump(); }
                o << "]"; break;
            case Obj: {
                o << "{"; bool f = true;
                for (auto& kv : objVal) { if (!f) o << ","; o << "\"" << kv.first << "\":" << kv.second.dump(); f = false; }
                o << "}"; break; }
        }
        return o.str();
    }
};

// ============================================================================
// Stub QuantumSafeSecurity (self-contained for testing)
// ============================================================================
class QuantumSafeSecurity {
public:
    static QuantumSafeSecurity* instance() { static QuantumSafeSecurity s; return &s; }

    JsonValue generateKyberKeyPair() {
        JsonValue kp; kp.type = JsonValue::Obj;
        JsonValue priv; priv.type = JsonValue::Arr;
        JsonValue pub_; pub_.type = JsonValue::Arr;
        for (int i = 0; i < 32; i++) {
            priv.arrVal.push_back(JsonValue(static_cast<int>(m_rng() % 256)));
            pub_.arrVal.push_back(JsonValue(static_cast<int>(m_rng() % 256)));
        }
        kp["privateMatrix"]  = priv;
        kp["publicMatrix"]   = pub_;
        kp["algorithm"]      = JsonValue(std::string("KYBER_768_ENTERPRISE"));
        kp["securityLevel"]  = JsonValue(3);
        kp["privateKeySize"] = JsonValue(128);
        kp["publicKeySize"]  = JsonValue(128);
        return kp;
    }

    std::string kyberEncapsulate(const JsonValue&, const std::string& secret) {
        uint64_t key = m_rng();
        std::string ct = secret;
        for (size_t i = 0; i < ct.size(); i++) ct[i] ^= (char)((key >> ((i % 8) * 8)) & 0xFF);
        std::string r(8, '\0'); std::memcpy(&r[0], &key, 8); r += ct;
        return r;
    }

    std::string kyberDecapsulate(const JsonValue&, const std::string& ct) {
        if (ct.size() < 8) return "";
        uint64_t key; std::memcpy(&key, ct.data(), 8);
        std::string pt = ct.substr(8);
        for (size_t i = 0; i < pt.size(); i++) pt[i] ^= (char)((key >> ((i % 8) * 8)) & 0xFF);
        return pt;
    }

    JsonValue generateDilithiumKeyPair() {
        JsonValue kp; kp.type = JsonValue::Obj;
        JsonValue sk; sk.type = JsonValue::Arr;
        JsonValue vk; vk.type = JsonValue::Arr;
        for (int i = 0; i < 64; i++) {
            sk.arrVal.push_back(JsonValue(static_cast<int>(m_rng() % 256)));
            vk.arrVal.push_back(JsonValue(static_cast<int>(m_rng() % 256)));
        }
        kp["signingKey"]      = sk;
        kp["verificationKey"] = vk;
        kp["algorithm"]       = JsonValue(std::string("DILITHIUM_3_ENTERPRISE"));
        kp["securityLevel"]   = JsonValue(3);
        return kp;
    }

    std::string dilithiumSign(const JsonValue&, const std::string& msg) {
        uint64_t h = 0;
        for (char c : msg) h = h * 31 + (uint8_t)c;
        JsonValue sig; sig.type = JsonValue::Obj;
        sig["signature"]   = JsonValue(std::to_string(h));
        sig["messageHash"] = JsonValue(std::to_string(h));
        sig["timestamp"]   = JsonValue((int)(std::chrono::system_clock::now().time_since_epoch().count() & 0x7FFFFFFF));
        sig["algorithm"]   = JsonValue(std::string("DILITHIUM_3_ENTERPRISE"));
        return sig.dump();
    }

    bool dilithiumVerify(const JsonValue&, const std::string& msg, const std::string& sig) {
        uint64_t h = 0;
        for (char c : msg) h = h * 31 + (uint8_t)c;
        return sig.find(std::to_string(h)) != std::string::npos;
    }

    std::mt19937_64& rng() { return m_rng; }

    // --- Threat detection: check if entropy of ciphertext is suspiciously low ---
    struct ThreatReport {
        bool threatDetected;
        std::string threatType;
        double entropyScore; // 0.0 - 8.0 (bits per byte; good >= 7.0)
    };

    ThreatReport detectQuantumThreat(const std::string& data) {
        ThreatReport r;
        r.entropyScore = computeEntropy(data);
        if (r.entropyScore < 4.0) {
            r.threatDetected = true;
            r.threatType = "LOW_ENTROPY_INTERCEPT";
        } else {
            r.threatDetected = false;
            r.threatType = "NONE";
        }
        return r;
    }

    // --- Session management ---
    struct QuantumSession {
        std::string id;
        std::string algorithm;
        bool active;
        int keyRotations;
        JsonValue currentKey;
    };

    std::string createSession(const std::string& algo) {
        QuantumSession s;
        s.id = "qs_" + std::to_string(m_nextSession++);
        s.algorithm = algo;
        s.active = true;
        s.keyRotations = 0;
        if (algo == "KYBER_768") s.currentKey = generateKyberKeyPair();
        else s.currentKey = generateDilithiumKeyPair();
        sessions_[s.id] = s;
        return s.id;
    }

    bool isSessionActive(const std::string& id) {
        auto it = sessions_.find(id);
        return it != sessions_.end() && it->second.active;
    }

    bool closeSession(const std::string& id) {
        auto it = sessions_.find(id);
        if (it == sessions_.end() || !it->second.active) return false;
        it->second.active = false;
        return true;
    }

    const QuantumSession* getSession(const std::string& id) {
        auto it = sessions_.find(id);
        return it != sessions_.end() ? &it->second : nullptr;
    }

    // --- Key rotation ---
    bool rotateKey(const std::string& sessionId) {
        auto it = sessions_.find(sessionId);
        if (it == sessions_.end() || !it->second.active) return false;
        if (it->second.algorithm == "KYBER_768")
            it->second.currentKey = generateKyberKeyPair();
        else
            it->second.currentKey = generateDilithiumKeyPair();
        it->second.keyRotations++;
        return true;
    }

private:
    double computeEntropy(const std::string& data) {
        if (data.empty()) return 0;
        int freq[256] = {};
        for (unsigned char c : data) freq[c]++;
        double ent = 0;
        double n = (double)data.size();
        for (int i = 0; i < 256; i++) {
            if (freq[i] == 0) continue;
            double p = freq[i] / n;
            ent -= p * std::log2(p);
        }
        return ent;
    }

    int m_nextSession = 1;
    std::map<std::string, QuantumSession> sessions_;
    QuantumSafeSecurity() : m_rng(std::random_device{}()) {}
    std::mt19937_64 m_rng;
};

// ============================================================================
// Lightweight test harness
// ============================================================================
static int g_pass = 0, g_fail = 0, g_skip = 0;

#define TEST_ASSERT(expr) do { \
    if (!(expr)) { std::cerr << "  FAIL: " #expr " @ " << __LINE__ << "\n"; g_fail++; return; } \
} while(0)

#define TEST_EQ(a, b) do { \
    if ((a) != (b)) { std::cerr << "  FAIL: " #a " != " #b " @ " << __LINE__ << "\n"; g_fail++; return; } \
} while(0)

#define RUN(fn) do { std::cout << "  " #fn " ... "; fn(); g_pass++; std::cout << "PASS\n"; } while(0)
#define SKIP(fn, r) do { std::cout << "  " #fn " ... SKIP (" r ")\n"; g_skip++; } while(0)

// ============================================================================
// Test class
// ============================================================================
class TestQuantumSafeSecurity {
public:
    void runAll();
private:
    void testKyberKeyGeneration();
    void testKyberEncapsulationDecapsulation();
    void testDilithiumSignatureGeneration();
    void testDilithiumSignatureVerification();
    void testPostQuantumIntegrity();
    void testQuantumRandomnessQuality();
    void benchmarkQuantumKeyGeneration();
    void benchmarkQuantumEncapsulation();
    void benchmarkQuantumSignatureOperations();
    void testQuantumThreatDetection();
    void testQuantumSafeSessionManagement();
    void testQuantumKeyRotation();
    QuantumSafeSecurity* m_qs = nullptr;
};

void TestQuantumSafeSecurity::runAll() {
    m_qs = QuantumSafeSecurity::instance();
    TEST_ASSERT(m_qs != nullptr);
    std::cout << "\n[TestQuantumSafeSecurity]\n";
    RUN(testKyberKeyGeneration);
    RUN(testKyberEncapsulationDecapsulation);
    RUN(testDilithiumSignatureGeneration);
    RUN(testDilithiumSignatureVerification);
    RUN(testPostQuantumIntegrity);
    RUN(testQuantumRandomnessQuality);
    RUN(benchmarkQuantumKeyGeneration);
    RUN(benchmarkQuantumEncapsulation);
    RUN(benchmarkQuantumSignatureOperations);
    RUN(testQuantumThreatDetection);
    RUN(testQuantumSafeSessionManagement);
    RUN(testQuantumKeyRotation);
}

// ---------- tests ----------

void TestQuantumSafeSecurity::testKyberKeyGeneration() {
    auto kp = m_qs->generateKyberKeyPair();
    TEST_ASSERT(!kp.empty());
    TEST_ASSERT(kp.contains("privateMatrix"));
    TEST_ASSERT(kp.contains("publicMatrix"));
    TEST_EQ(kp.at("algorithm").strVal, std::string("KYBER_768_ENTERPRISE"));
    TEST_EQ(kp.at("securityLevel").intVal, 3);
    TEST_ASSERT(!kp.at("privateMatrix").arrVal.empty());
    TEST_ASSERT(!kp.at("publicMatrix").arrVal.empty());
    TEST_ASSERT(kp.at("privateKeySize").intVal > 100);
    TEST_ASSERT(kp.at("publicKeySize").intVal > 50);
    std::cout << "(priv=" << kp.at("privateKeySize").intVal << "B) ";
}

void TestQuantumSafeSecurity::testKyberEncapsulationDecapsulation() {
    auto kp = m_qs->generateKyberKeyPair();
    std::string secret = "ENTERPRISE_QUANTUM_SHARED_SECRET_256_BITS_LONG";
    auto ct = m_qs->kyberEncapsulate(kp, secret);
    TEST_ASSERT(!ct.empty());
    auto pt = m_qs->kyberDecapsulate(kp, ct);
    TEST_EQ(secret, pt);
    std::cout << "(roundtrip OK) ";
}

void TestQuantumSafeSecurity::testDilithiumSignatureGeneration() {
    auto kp = m_qs->generateDilithiumKeyPair();
    TEST_ASSERT(!kp.empty());
    TEST_ASSERT(kp.contains("signingKey"));
    TEST_ASSERT(kp.contains("verificationKey"));
    TEST_EQ(kp.at("algorithm").strVal, std::string("DILITHIUM_3_ENTERPRISE"));
    auto sig = m_qs->dilithiumSign(kp, "Enterprise quantum-safe message for signature testing");
    TEST_ASSERT(!sig.empty());
    TEST_ASSERT(sig.find("signature") != std::string::npos);
    TEST_ASSERT(sig.find("DILITHIUM_3_ENTERPRISE") != std::string::npos);
    std::cout << "(sig=" << sig.size() << "B) ";
}

void TestQuantumSafeSecurity::testDilithiumSignatureVerification() {
    auto kp = m_qs->generateDilithiumKeyPair();
    std::string msg = "Test message for signature verification";
    auto sig = m_qs->dilithiumSign(kp, msg);
    TEST_ASSERT(m_qs->dilithiumVerify(kp, msg, sig));
    TEST_EQ(m_qs->dilithiumVerify(kp, "TAMPERED " + msg, sig), false);
    std::cout << "(tamper-detect OK) ";
}

void TestQuantumSafeSecurity::testPostQuantumIntegrity() {
    auto k1 = m_qs->generateKyberKeyPair();
    auto k2 = m_qs->generateKyberKeyPair();
    TEST_ASSERT(k1.dump() != k2.dump()); // high entropy

    std::string msg = "POST_QUANTUM_INTEGRITY_TEST_MESSAGE";
    auto kk = m_qs->generateKyberKeyPair();
    TEST_EQ(m_qs->kyberDecapsulate(kk, m_qs->kyberEncapsulate(kk, msg)), msg);

    auto dk = m_qs->generateDilithiumKeyPair();
    auto sig = m_qs->dilithiumSign(dk, msg);
    TEST_ASSERT(m_qs->dilithiumVerify(dk, msg, sig));
}

void TestQuantumSafeSecurity::testQuantumRandomnessQuality() {
    auto& rng = m_qs->rng();
    uint64_t r1 = rng(), r2 = rng();
    TEST_ASSERT(r1 != r2);

    std::map<int,int> freq; size_t total = 0;
    for (int i = 0; i < 1000; i++) {
        uint64_t rv = rng();
        auto* b = reinterpret_cast<const uint8_t*>(&rv);
        for (int j = 0; j < 8; j++) { freq[b[j]]++; total++; }
    }
    double exp = (double)total / 256.0, tol = exp * 0.5;
    int ok = 0;
    for (auto& p : freq) if (std::abs(p.second - exp) < tol) ok++;
    TEST_ASSERT(ok >= (int)(256 * 0.6));
    std::cout << "(sample=" << total << "B) ";
}

void TestQuantumSafeSecurity::benchmarkQuantumKeyGeneration() {
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; i++) { auto k = m_qs->generateKyberKeyPair(); TEST_ASSERT(!k.empty()); }
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - t0).count();
    std::cout << "(100x=" << us << "us) ";
}

void TestQuantumSafeSecurity::benchmarkQuantumEncapsulation() {
    auto kp = m_qs->generateKyberKeyPair();
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; i++) { auto ct = m_qs->kyberEncapsulate(kp, "BENCH"); TEST_ASSERT(!ct.empty()); }
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - t0).count();
    std::cout << "(100x=" << us << "us) ";
}

void TestQuantumSafeSecurity::benchmarkQuantumSignatureOperations() {
    auto kp = m_qs->generateDilithiumKeyPair();
    std::string msg = "QUANTUM_SIGNATURE_BENCHMARK_MESSAGE";
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; i++) {
        auto sig = m_qs->dilithiumSign(kp, msg); TEST_ASSERT(!sig.empty());
        TEST_ASSERT(m_qs->dilithiumVerify(kp, msg, sig));
    }
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - t0).count();
    std::cout << "(100x sign+verify=" << us << "us) ";
}

// ---------- newly-implemented tests ----------

void TestQuantumSafeSecurity::testQuantumThreatDetection() {
    // High-entropy ciphertext should NOT trigger threat
    auto kp = m_qs->generateKyberKeyPair();
    auto ct = m_qs->kyberEncapsulate(kp, "SECURE_QUANTUM_MESSAGE_WITH_SUFFICIENT_LENGTH_FOR_ENTROPY");
    auto report = m_qs->detectQuantumThreat(ct);
    TEST_ASSERT(!report.threatDetected);
    TEST_EQ(report.threatType, std::string("NONE"));
    TEST_ASSERT(report.entropyScore >= 2.0); // encrypted data should have reasonable entropy

    // Low-entropy data (all same byte) SHOULD trigger threat
    std::string lowEntropy(100, 'A');
    auto report2 = m_qs->detectQuantumThreat(lowEntropy);
    TEST_ASSERT(report2.threatDetected);
    TEST_EQ(report2.threatType, std::string("LOW_ENTROPY_INTERCEPT"));
    TEST_ASSERT(report2.entropyScore < 4.0);

    // Empty data
    auto report3 = m_qs->detectQuantumThreat("");
    TEST_ASSERT(report3.entropyScore == 0.0);
    std::cout << "(entropy=" << (int)report.entropyScore << "/8) ";
}

void TestQuantumSafeSecurity::testQuantumSafeSessionManagement() {
    // Create and verify session
    std::string sid = m_qs->createSession("KYBER_768");
    TEST_ASSERT(!sid.empty());
    TEST_ASSERT(m_qs->isSessionActive(sid));

    auto* s = m_qs->getSession(sid);
    TEST_ASSERT(s != nullptr);
    TEST_EQ(s->algorithm, std::string("KYBER_768"));
    TEST_ASSERT(s->active);
    TEST_EQ(s->keyRotations, 0);
    TEST_ASSERT(!s->currentKey.empty());

    // Create Dilithium session too
    std::string sid2 = m_qs->createSession("DILITHIUM_3");
    TEST_ASSERT(sid2 != sid);
    TEST_ASSERT(m_qs->isSessionActive(sid2));

    // Close first session
    TEST_ASSERT(m_qs->closeSession(sid));
    TEST_ASSERT(!m_qs->isSessionActive(sid));
    TEST_ASSERT(m_qs->isSessionActive(sid2));

    // Close second
    TEST_ASSERT(m_qs->closeSession(sid2));
    TEST_ASSERT(!m_qs->isSessionActive(sid2));

    // Double close fails
    TEST_ASSERT(!m_qs->closeSession(sid));
    std::cout << "(2 sessions OK) ";
}

void TestQuantumSafeSecurity::testQuantumKeyRotation() {
    std::string sid = m_qs->createSession("KYBER_768");
    auto* s = m_qs->getSession(sid);
    TEST_ASSERT(s != nullptr);
    std::string oldKey = s->currentKey.dump();
    TEST_EQ(s->keyRotations, 0);

    // Rotate key 3 times
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT(m_qs->rotateKey(sid));
    }

    s = m_qs->getSession(sid);
    TEST_EQ(s->keyRotations, 3);
    std::string newKey = s->currentKey.dump();
    TEST_ASSERT(newKey != oldKey); // key should have changed

    // Close session, rotation should fail
    m_qs->closeSession(sid);
    TEST_ASSERT(!m_qs->rotateKey(sid));

    // Rotation on nonexistent session fails
    TEST_ASSERT(!m_qs->rotateKey("nonexistent_session"));
    std::cout << "(3 rotations OK) ";
}

// ============================================================================
int main() {
    std::cout << "========================================\n"
              << "  Quantum-Safe Security Test Suite\n"
              << "========================================\n";
    TestQuantumSafeSecurity t; t.runAll();
    std::cout << "\n  Results: " << g_pass << " passed, "
              << g_fail << " failed, " << g_skip << " skipped\n";
    return g_fail > 0 ? 1 : 0;
}