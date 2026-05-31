#include "gguf_factory/gguf_writer_minimal.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>


using RawrXD::GGUFFactory::GGUFWriterMinimal;

static int g_testsPassed = 0;
static int g_testsFailed = 0;

static bool expect_(bool cond, const char* msg)
{
    if (!cond)
    {
        std::printf("  FAIL: %s\n", msg ? msg : "(null)");
        ++g_testsFailed;
        return false;
    }
    std::printf("  PASS: %s\n", msg ? msg : "ok");
    ++g_testsPassed;
    return true;
}

static bool readU32_(FILE* f, uint32_t& out)
{
    return std::fread(&out, sizeof(out), 1, f) == 1;
}
static bool readU64_(FILE* f, uint64_t& out)
{
    return std::fread(&out, sizeof(out), 1, f) == 1;
}
static bool readI32_(FILE* f, int32_t& out)
{
    return std::fread(&out, sizeof(out), 1, f) == 1;
}
static bool readF32_(FILE* f, float& out)
{
    return std::fread(&out, sizeof(out), 1, f) == 1;
}
static bool readU8_(FILE* f, uint8_t& out)
{
    return std::fread(&out, sizeof(out), 1, f) == 1;
}
static bool readBytes_(FILE* f, void* dst, size_t n)
{
    return n == 0 || std::fread(dst, 1, n, f) == n;
}
static bool readString_(FILE* f, std::string& out)
{
    uint64_t len = 0;
    if (!readU64_(f, len))
        return false;
    out.assign((size_t)len, '\0');
    return readBytes_(f, out.empty() ? nullptr : &out[0], (size_t)len);
}

static bool readHeader_(FILE* f, uint32_t& magic, uint32_t& version, uint64_t& tensorCount, uint64_t& kvCount)
{
    return readU32_(f, magic) && readU32_(f, version) && readU64_(f, tensorCount) && readU64_(f, kvCount);
}

struct GGUFReadResult_
{
    std::unordered_map<std::string, std::string> kv;
    std::unordered_map<std::string, uint32_t> types;
};

static GGUFReadResult_ readKv_(FILE* f, uint64_t kvCount)
{
    GGUFReadResult_ rr;
    rr.kv.reserve((size_t)kvCount);
    rr.types.reserve((size_t)kvCount);

    for (uint64_t i = 0; i < kvCount; ++i)
    {
        std::string key;
        if (!readString_(f, key))
            break;

        uint32_t type = 0;
        if (!readU32_(f, type))
            break;

        std::string value;
        switch (type)
        {
            case 4:  // UINT32
            {
                uint32_t v = 0;
                if (!readU32_(f, v))
                    return rr;
                value = std::to_string(v);
            }
            break;
            case 5:  // INT32
            {
                int32_t v = 0;
                if (!readI32_(f, v))
                    return rr;
                value = std::to_string(v);
            }
            break;
            case 6:  // FLOAT32
            {
                float v = 0.0f;
                if (!readF32_(f, v))
                    return rr;
                // Canonicalize floats with max precision; tests use numeric compare anyway.
                char buf[64];
                std::snprintf(buf, sizeof(buf), "%.9g", v);
                value.assign(buf);
            }
            break;
            case 7:  // BOOL
            {
                uint8_t v = 0;
                if (!readU8_(f, v))
                    return rr;
                value = v ? "true" : "false";
            }
            break;
            case 8:  // STRING
            {
                if (!readString_(f, value))
                    return rr;
            }
            break;
            default:
                // Unknown type: bail (minimal reader).
                return rr;
        }

        rr.types.emplace(key, type);
        rr.kv.emplace(std::move(key), std::move(value));
    }

    return rr;
}

static bool verifyKvEq_(const std::unordered_map<std::string, std::string>& kv, const char* key, const char* expected)
{
    const std::string k = key ? key : "";
    auto it = kv.find(k);
    if (it == kv.end())
        return expect_(false, (std::string("missing key: ") + k).c_str());
    const std::string exp = expected ? expected : "";
    if (it->second != exp)
    {
        std::printf("  FAIL: %s = `%s` (expected `%s`)\n", k.c_str(), it->second.c_str(), exp.c_str());
        ++g_testsFailed;
        return false;
    }
    std::printf("  PASS: %s = `%s`\n", k.c_str(), exp.c_str());
    ++g_testsPassed;
    return true;
}

static bool verifyKvBool_(const std::unordered_map<std::string, std::string>& kv, const char* key, bool expected)
{
    return verifyKvEq_(kv, key, expected ? "true" : "false");
}

static bool verifyKvFloatNear_(const std::unordered_map<std::string, std::string>& kv, const char* key, float expected,
                               float eps)
{
    const std::string k = key ? key : "";
    auto it = kv.find(k);
    if (it == kv.end())
        return expect_(false, (std::string("missing key: ") + k).c_str());
    const float got = std::strtof(it->second.c_str(), nullptr);
    const float diff = std::fabs(got - expected);
    if (!(diff <= eps))
    {
        std::printf("  FAIL: %s = %g (expected %g ± %g)\n", k.c_str(), got, expected, eps);
        ++g_testsFailed;
        return false;
    }
    std::printf("  PASS: %s ~= %g\n", k.c_str(), expected);
    ++g_testsPassed;
    return true;
}

static bool testBasicProfile_()
{
    std::printf("\nTest: Basic profile with behavioral toggles\n");
    const char* path = "test_profile.gguf";

    // Write
    {
        GGUFWriterMinimal writer;
        writer.addString("general.architecture", "llama");
        writer.addInt32("llama.embedding_length", 768);
        writer.addInt32("llama.feed_forward_length", 3072);
        writer.addInt32("llama.head_count", 12);
        writer.addInt32("llama.block_count", 12);
        writer.addInt32("llama.context_length", 2048);
        writer.addBool("sovereign.deepthinking", true);
        writer.addBool("sovereign.deepresearch", false);
        writer.addBool("sovereign.max_mode", true);

        if (!writer.writeMetadataOnly(path))
        {
            std::printf("  FAIL: write failed: %s\n", writer.lastError().c_str());
            ++g_testsFailed;
            return false;
        }
        ++g_testsPassed;
        std::printf("  PASS: wrote `%s`\n", path);
    }

    // Read + verify
    FILE* f = std::fopen(path, "rb");
    if (!f)
        return expect_(false, "cannot open file for reading");

    uint32_t magic = 0, version = 0;
    uint64_t tensorCount = 0, kvCount = 0;
    if (!readHeader_(f, magic, version, tensorCount, kvCount))
    {
        std::fclose(f);
        return expect_(false, "cannot read header");
    }

    expect_(magic == 0x46554747u, "magic == GGUF");
    expect_(version == 3u, "version == 3");
    expect_(tensorCount == 0u, "tensor_count == 0");
    expect_(kvCount == 9u, "kv_count == 9");

    const auto rr = readKv_(f, kvCount);
    std::fclose(f);

    verifyKvEq_(rr.kv, "general.architecture", "llama");
    verifyKvEq_(rr.kv, "llama.embedding_length", "768");
    verifyKvEq_(rr.kv, "llama.feed_forward_length", "3072");
    verifyKvEq_(rr.kv, "llama.head_count", "12");
    verifyKvEq_(rr.kv, "llama.block_count", "12");
    verifyKvEq_(rr.kv, "llama.context_length", "2048");
    verifyKvBool_(rr.kv, "sovereign.deepthinking", true);
    verifyKvBool_(rr.kv, "sovereign.deepresearch", false);
    verifyKvBool_(rr.kv, "sovereign.max_mode", true);

    return true;
}

static bool testAllTypes_()
{
    std::printf("\nTest: All supported value types\n");
    const char* path = "test_types.gguf";

    {
        GGUFWriterMinimal writer;
        writer.addString("test.string", "hello_world");
        writer.addInt32("test.int32", -12345);
        writer.addUInt32("test.uint32", 3735928559u);  // 0xDEADBEEF
        writer.addFloat32("test.float32", 3.14159f);
        writer.addBool("test.bool_true", true);
        writer.addBool("test.bool_false", false);
        if (!writer.writeMetadataOnly(path))
        {
            std::printf("  FAIL: write failed: %s\n", writer.lastError().c_str());
            ++g_testsFailed;
            return false;
        }
        ++g_testsPassed;
        std::printf("  PASS: wrote `%s`\n", path);
    }

    FILE* f = std::fopen(path, "rb");
    if (!f)
        return expect_(false, "cannot open file for reading");

    uint32_t magic = 0, version = 0;
    uint64_t tensorCount = 0, kvCount = 0;
    if (!readHeader_(f, magic, version, tensorCount, kvCount))
    {
        std::fclose(f);
        return expect_(false, "cannot read header");
    }
    expect_(kvCount == 6u, "kv_count == 6");
    const auto rr = readKv_(f, kvCount);
    std::fclose(f);

    verifyKvEq_(rr.kv, "test.string", "hello_world");
    verifyKvEq_(rr.kv, "test.int32", "-12345");
    verifyKvEq_(rr.kv, "test.uint32", "3735928559");
    verifyKvFloatNear_(rr.kv, "test.float32", 3.14159f, 1e-6f);
    verifyKvBool_(rr.kv, "test.bool_true", true);
    verifyKvBool_(rr.kv, "test.bool_false", false);

    return true;
}

static bool testManyKv_()
{
    std::printf("\nTest: Many KV pairs\n");
    const char* path = "test_many_kv.gguf";
    const int numPairs = 100;

    {
        GGUFWriterMinimal writer;
        writer.addString("general.architecture", "llama");
        for (int i = 0; i < numPairs; ++i)
        {
            char k[64];
            char v[64];
            std::snprintf(k, sizeof(k), "test.key.%03d", i);
            std::snprintf(v, sizeof(v), "value_%03d", i);
            writer.addString(k, v);
        }
        if (!writer.writeMetadataOnly(path))
        {
            std::printf("  FAIL: write failed: %s\n", writer.lastError().c_str());
            ++g_testsFailed;
            return false;
        }
        ++g_testsPassed;
        std::printf("  PASS: wrote `%s`\n", path);
    }

    FILE* f = std::fopen(path, "rb");
    if (!f)
        return expect_(false, "cannot open file for reading");

    uint32_t magic = 0, version = 0;
    uint64_t tensorCount = 0, kvCount = 0;
    if (!readHeader_(f, magic, version, tensorCount, kvCount))
    {
        std::fclose(f);
        return expect_(false, "cannot read header");
    }
    expect_(kvCount == 101u, "kv_count == 101");
    const auto rr = readKv_(f, kvCount);
    std::fclose(f);

    verifyKvEq_(rr.kv, "general.architecture", "llama");
    verifyKvEq_(rr.kv, "test.key.000", "value_000");
    verifyKvEq_(rr.kv, "test.key.050", "value_050");
    verifyKvEq_(rr.kv, "test.key.099", "value_099");

    int found = 0;
    for (int i = 0; i < numPairs; ++i)
    {
        char k[64];
        std::snprintf(k, sizeof(k), "test.key.%03d", i);
        if (rr.kv.find(k) != rr.kv.end())
            ++found;
    }
    expect_(found == numPairs, "all 100 test keys present");
    return true;
}

static bool testUnicode_()
{
    std::printf("\nTest: Unicode string handling\n");
    const char* path = "test_unicode.gguf";

    const char* jp = "日本語テスト";
    const char* emoji = "🚀🎉💡";
    const char* mixed = "Hello 世界 🌍";
    const char* arabic = "مرحبا بالعالم";
    const char* russian = "Привет мир";

    {
        GGUFWriterMinimal writer;
        writer.addString("test.japanese", jp);
        writer.addString("test.emoji", emoji);
        writer.addString("test.mixed", mixed);
        writer.addString("test.arabic", arabic);
        writer.addString("test.russian", russian);
        if (!writer.writeMetadataOnly(path))
        {
            std::printf("  FAIL: write failed: %s\n", writer.lastError().c_str());
            ++g_testsFailed;
            return false;
        }
        ++g_testsPassed;
        std::printf("  PASS: wrote `%s`\n", path);
    }

    FILE* f = std::fopen(path, "rb");
    if (!f)
        return expect_(false, "cannot open file for reading");

    uint32_t magic = 0, version = 0;
    uint64_t tensorCount = 0, kvCount = 0;
    if (!readHeader_(f, magic, version, tensorCount, kvCount))
    {
        std::fclose(f);
        return expect_(false, "cannot read header");
    }
    expect_(kvCount == 5u, "kv_count == 5");
    const auto rr = readKv_(f, kvCount);
    std::fclose(f);

    verifyKvEq_(rr.kv, "test.japanese", jp);
    verifyKvEq_(rr.kv, "test.emoji", emoji);
    verifyKvEq_(rr.kv, "test.mixed", mixed);
    verifyKvEq_(rr.kv, "test.arabic", arabic);
    verifyKvEq_(rr.kv, "test.russian", russian);
    return true;
}

static bool testEdgeCases_()
{
    std::printf("\nTest: Edge cases\n");
    const char* path = "test_edge.gguf";

    {
        GGUFWriterMinimal writer;
        writer.addString("test.empty", "");
        writer.addString("test.long", std::string(10000, 'x'));
        writer.addString("test.newline", "line1\nline2");
        writer.addString("test.tab", "col1\tcol2");
        writer.addString("test.special", "a\\b/c:d*e?f\"g");
        writer.addInt32("test.negative", -999999);
        writer.addUInt32("test.large_uint", 4294967295u);
        writer.addFloat32("test.float_zero", 0.0f);
        writer.addFloat32("test.float_neg", -1.5f);
        if (!writer.writeMetadataOnly(path))
        {
            std::printf("  FAIL: write failed: %s\n", writer.lastError().c_str());
            ++g_testsFailed;
            return false;
        }
        ++g_testsPassed;
        std::printf("  PASS: wrote `%s`\n", path);
    }

    FILE* f = std::fopen(path, "rb");
    if (!f)
        return expect_(false, "cannot open file for reading");

    uint32_t magic = 0, version = 0;
    uint64_t tensorCount = 0, kvCount = 0;
    if (!readHeader_(f, magic, version, tensorCount, kvCount))
    {
        std::fclose(f);
        return expect_(false, "cannot read header");
    }
    expect_(kvCount == 9u, "kv_count == 9");
    const auto rr = readKv_(f, kvCount);
    std::fclose(f);

    verifyKvEq_(rr.kv, "test.empty", "");
    verifyKvEq_(rr.kv, "test.negative", "-999999");
    verifyKvEq_(rr.kv, "test.large_uint", "4294967295");
    verifyKvFloatNear_(rr.kv, "test.float_zero", 0.0f, 0.0f);
    verifyKvFloatNear_(rr.kv, "test.float_neg", -1.5f, 1e-6f);

    auto it = rr.kv.find("test.long");
    expect_(it != rr.kv.end(), "test.long exists");
    if (it != rr.kv.end())
        expect_(it->second.size() == 10000u, "long string length preserved");
    return true;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    std::printf("=== GGUF Profile Writer Test Suite ===\n");

    bool ok = true;
    ok &= testBasicProfile_();
    ok &= testAllTypes_();
    ok &= testManyKv_();
    ok &= testUnicode_();
    ok &= testEdgeCases_();

    std::printf("\n=== Test Summary ===\n");
    std::printf("Passed: %d\n", g_testsPassed);
    std::printf("Failed: %d\n", g_testsFailed);

    if (ok && g_testsFailed == 0)
    {
        std::printf("\n=== ALL TESTS PASSED ===\n");
        return 0;
    }
    std::printf("\n=== SOME TESTS FAILED ===\n");
    return 1;
}
