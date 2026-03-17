// GGUF detailed parser - extracts header, key-value metadata, and tensor index
#include <jni.h>
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <android/log.h>

#define LOG_TAG "GGUFParser"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

struct GGUFMetadata {
    std::map<std::string, std::string> kv;
    uint32_t tensorCount = 0;
};

// Very simplified GGUF parsing (reference: gguf format spec). Not full tensor decoding.
// Layout (little endian):
// magic(4) = 'GGUF' | version(u32) | tensor_count(u32) | kv_count(u32) | kv entries
// kv entry: key_len(u32) | key(bytes) | value_type(u8) | value depending on type
// This parser will capture string & int types, ignore others.

static bool read_u32(const uint8_t* data, size_t len, size_t& offset, uint32_t& out) {
    if (offset + 4 > len) return false;
    out = (uint32_t)data[offset] |
          ((uint32_t)data[offset+1] << 8) |
          ((uint32_t)data[offset+2] << 16) |
          ((uint32_t)data[offset+3] << 24);
    offset += 4;
    return true;
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_rawr_mobilecopilot_core_ModelInferenceEngine_nativeParseGgufDetailed(
    JNIEnv* env,
    jobject,
    jbyteArray ggufBytes) {
    jsize len = env->GetArrayLength(ggufBytes);
    if (len < 24) return 0; // minimal size
    std::vector<uint8_t> buffer(len);
    env->GetByteArrayRegion(ggufBytes, 0, len, reinterpret_cast<jbyte*>(buffer.data()));
    size_t off = 0;
    if (!(buffer[0]=='G' && buffer[1]=='G' && buffer[2]=='U' && buffer[3]=='F')) return 0;
    off += 4; // magic
    uint32_t version=0, tensorCount=0, kvCount=0;
    if (!read_u32(buffer.data(), buffer.size(), off, version)) return 0;
    if (!read_u32(buffer.data(), buffer.size(), off, tensorCount)) return 0;
    if (!read_u32(buffer.data(), buffer.size(), off, kvCount)) return 0;
    auto* meta = new GGUFMetadata();
    meta->tensorCount = tensorCount;

    for (uint32_t i=0; i<kvCount; ++i) {
        uint32_t keyLen=0; if (!read_u32(buffer.data(), buffer.size(), off, keyLen)) break;
        if (off + keyLen > buffer.size()) break;
        std::string key(reinterpret_cast<char*>(buffer.data()+off), keyLen);
        off += keyLen;
        if (off >= buffer.size()) break;
        uint8_t valueType = buffer[off++];
        switch (valueType) {
            case 0: { // string
                uint32_t vlen=0; if (!read_u32(buffer.data(), buffer.size(), off, vlen)) break;
                if (off + vlen > buffer.size()) { off = buffer.size(); break; }
                std::string value(reinterpret_cast<char*>(buffer.data()+off), vlen);
                off += vlen;
                meta->kv[key] = value;
                break;
            }
            case 1: { // uint32
                uint32_t val=0; if (!read_u32(buffer.data(), buffer.size(), off, val)) break;
                meta->kv[key] = std::to_string(val);
                break;
            }
            default: {
                // Skip unsupported types conservatively
                meta->kv[key] = "(unsupported_type)";
                break;
            }
        }
        if (off >= buffer.size()) break;
    }
    // store basic fields
    meta->kv["gguf.version"] = std::to_string(version);
    meta->kv["gguf.tensor_count"] = std::to_string(tensorCount);
    meta->kv["gguf.kv_count"] = std::to_string(kvCount);
    return reinterpret_cast<jlong>(meta);
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_com_rawr_mobilecopilot_core_ModelInferenceEngine_nativeListGgufMeta(
    JNIEnv* env,
    jobject,
    jlong ptr) {
    auto* meta = reinterpret_cast<GGUFMetadata*>(ptr);
    if (!meta) return nullptr;
    jclass stringClass = env->FindClass("java/lang/String");
    jobjectArray arr = env->NewObjectArray(meta->kv.size(), stringClass, nullptr);
    int idx = 0;
    for (auto& kv : meta->kv) {
        std::string line = kv.first + "=" + kv.second;
        jstring jline = env->NewStringUTF(line.c_str());
        env->SetObjectArrayElement(arr, idx++, jline);
    }
    return arr;
}

extern "C" JNIEXPORT void JNICALL
Java_com_rawr_mobilecopilot_core_ModelInferenceEngine_nativeReleaseGgufDetailed(
    JNIEnv* env,
    jobject,
    jlong ptr) {
    auto* meta = reinterpret_cast<GGUFMetadata*>(ptr);
    delete meta;
}