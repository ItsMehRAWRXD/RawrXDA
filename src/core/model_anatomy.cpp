#include "model_anatomy.hpp"

#include "BinaryStream.hpp"
#include "../llm_adapter/gguf_k_quants.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>

namespace RawrXD {

static const char* kCategoryNames[] = {
    "embedding", "attention_q", "attention_k", "attention_v", "attention_o",
    "ffn_gate", "ffn_up", "ffn_down", "norm", "output", "misc"
};

void ClassifyTensor(const std::string& name, int& category, int& layerId) {
    category = 10; // misc
    layerId = -1;

    auto contains = [&name](const char* sub) {
        return name.find(sub) != std::string::npos;
    };
    auto extractLayer = [&name]() -> int {
        // blk.N. or layers.N. or model.layers.N.
        size_t i = name.find("blk.");
        if (i != std::string::npos) {
            i += 4;
            int n = 0;
            while (i < name.size() && name[i] >= '0' && name[i] <= '9')
                n = n * 10 + (name[i++] - '0');
            return n;
        }
        i = name.find("layers.");
        if (i != std::string::npos) {
            i += 7;
            int n = 0;
            while (i < name.size() && name[i] >= '0' && name[i] <= '9')
                n = n * 10 + (name[i++] - '0');
            return n;
        }
        return -1;
    };

    if (contains("embed") || contains("tok_emb")) { category = 0; return; }
    if (contains("attn_q") || contains("self_attn.q")) { category = 1; layerId = extractLayer(); return; }
    if (contains("attn_k") || contains("self_attn.k")) { category = 2; layerId = extractLayer(); return; }
    if (contains("attn_v") || contains("self_attn.v")) { category = 3; layerId = extractLayer(); return; }
    if (contains("attn_output") || contains("self_attn.o")) { category = 4; layerId = extractLayer(); return; }
    if (contains("ffn_gate") || contains("mlp.gate")) { category = 5; layerId = extractLayer(); return; }
    if (contains("ffn_up") || contains("mlp.up")) { category = 6; layerId = extractLayer(); return; }
    if (contains("ffn_down") || contains("mlp.down")) { category = 7; layerId = extractLayer(); return; }
    if (contains("norm") || contains("ln_") || contains("rms")) { category = 8; layerId = extractLayer(); return; }
    if (contains("output") || contains("lm_head") || contains("embed_tokens")) { category = 9; return; }
}

bool BuildAnatomyFromLoader(void* loader, ModelAnatomy& out, std::ostream* streamOut) {
    // IGGUFLoader interface not available in current build - stub
    (void)loader; (void)out; (void)streamOut;
    return false;
}

static void jsonEscape(std::ostringstream& os, const std::string& s) {
    for (char c : s) {
        if (c == '"') os << "\\\"";
        else if (c == '\\') os << "\\\\";
        else if (c >= 32 && c != 127) os << c;
        else os << "\\u" << std::hex << std::setfill('0') << std::setw(4) << (unsigned)(unsigned char)c << std::dec;
    }
}

std::string ExportAnatomyToJson(const ModelAnatomy& a, bool pretty) {
    std::ostringstream os;
    os << "{\"model\":\"";
    jsonEscape(os, a.modelName);
    os << "\",\"gguf_version\":" << a.ggufVersion
       << ",\"tensor_count\":" << a.tensorCount << ",\"total_params\":" << a.totalParams
       << ",\"tensors\":[";
    for (size_t i = 0; i < a.tensors.size(); ++i) {
        const auto& t = a.tensors[i];
        if (i) os << ",";
        if (pretty) os << "\n  ";
        os << "{\"name\":\""; jsonEscape(os, t.name); os << "\",\"type\":" << t.tensorType
           << ",\"cat\":" << t.category << ",\"layer\":" << t.layerId
           << ",\"offset\":" << t.byteOffset << ",\"size\":" << t.byteSize
           << ",\"elements\":" << t.elementCount << "}";
    }
    os << "]}";
    return os.str();
}

namespace
{

using RawrXD::BinaryStream;
using RawrXD::StreamStatus;

void skipGgufValue(BinaryStream& ds, uint32_t type)
{
    switch (type)
    {
        case 0: {
            uint8_t v;
            ds >> v;
            break;
        }
        case 1: {
            int8_t v;
            ds >> v;
            break;
        }
        case 2: {
            uint16_t v;
            ds >> v;
            break;
        }
        case 3: {
            int16_t v;
            ds >> v;
            break;
        }
        case 4: {
            uint32_t v;
            ds >> v;
            break;
        }
        case 5: {
            int32_t v;
            ds >> v;
            break;
        }
        case 6: {
            float v;
            ds >> v;
            break;
        }
        case 7: {
            bool v;
            ds >> v;
            break;
        }
        case 8: {
            uint64_t len;
            ds >> len;
            ds.skipRawData(static_cast<int>(len));
            break;
        }
        case 9: {
            uint32_t elemType;
            ds >> elemType;
            uint64_t len;
            ds >> len;
            for (uint64_t i = 0; i < len; ++i)
            {
                skipGgufValue(ds, elemType);
            }
            break;
        }
        default:
            ds.setStatus(StreamStatus::ReadCorruptData);
            break;
    }
}

std::string readGgufKey(BinaryStream& ds)
{
    uint64_t keyLen = 0;
    ds >> keyLen;
    if (ds.status() != StreamStatus::Ok || keyLen > 4096u)
    {
        return {};
    }
    std::string key(static_cast<size_t>(keyLen), '\0');
    if (keyLen > 0)
    {
        const int n = static_cast<int>(keyLen);
        if (ds.readRawData(key.data(), n) != n)
        {
            return {};
        }
    }
    return key;
}

}  // namespace

bool BuildAnatomyFromGgufPath(const std::string& filePath, ModelAnatomy& out, std::ostream* streamOut,
                              std::string* errorOut)
{
    out = ModelAnatomy{};
    out.modelPath = filePath;

    if (errorOut)
    {
        errorOut->clear();
    }

    RawrXD::NativeFile file(filePath);
    if (!file.exists())
    {
        if (errorOut)
        {
            *errorOut = "file does not exist";
        }
        return false;
    }
    if (!file.open())
    {
        if (errorOut)
        {
            *errorOut = "failed to open file";
        }
        return false;
    }

    const int64_t fileSize64 = file.size();
    if (fileSize64 < 32)
    {
        if (errorOut)
        {
            *errorOut = "file too small";
        }
        return false;
    }

    BinaryStream ds(file.getStream());
    ds.setByteOrderLittleEndian();

    uint32_t magic = 0;
    uint32_t version = 0;
    uint64_t tensorCount = 0;
    uint64_t kvCount = 0;
    ds >> magic >> version >> tensorCount >> kvCount;
    if (ds.status() != StreamStatus::Ok || magic != 0x46554747u)
    {
        if (errorOut)
        {
            *errorOut = "not a GGUF file (bad magic)";
        }
        return false;
    }
    if (version < 2)
    {
        if (errorOut)
        {
            *errorOut = "GGUF version < 2 not supported";
        }
        return false;
    }

    out.ggufVersion = version;

    uint32_t tensorAlign = 32;
    for (uint64_t i = 0; i < kvCount; ++i)
    {
        const std::string key = readGgufKey(ds);
        if (ds.status() != StreamStatus::Ok)
        {
            if (errorOut)
            {
                *errorOut = "truncated KV key";
            }
            return false;
        }
        uint32_t valueType = 0;
        ds >> valueType;
        if (ds.status() != StreamStatus::Ok)
        {
            if (errorOut)
            {
                *errorOut = "truncated KV value type";
            }
            return false;
        }
        if (key == "general.alignment")
        {
            if (valueType == 4)
            {
                uint32_t v = 0;
                ds >> v;
                if (v > 0)
                {
                    tensorAlign = v;
                }
            }
            else if (valueType == 5)
            {
                int32_t v = 0;
                ds >> v;
                if (v > 0)
                {
                    tensorAlign = static_cast<uint32_t>(v);
                }
            }
            else
            {
                skipGgufValue(ds, valueType);
            }
        }
        else
        {
            skipGgufValue(ds, valueType);
        }
        if (ds.status() != StreamStatus::Ok)
        {
            if (errorOut)
            {
                *errorOut = "corrupt KV section";
            }
            return false;
        }
    }

    out.tensors.clear();
    out.tensors.reserve(static_cast<size_t>((std::min)(tensorCount, uint64_t{1} << 20)));

    for (uint64_t i = 0; i < tensorCount; ++i)
    {
        TensorEntry te;
        uint64_t nameLen = 0;
        ds >> nameLen;
        constexpr uint64_t kMaxName = 1u << 20;
        if (nameLen > kMaxName || nameLen > static_cast<uint64_t>(std::numeric_limits<int>::max()))
        {
            if (errorOut)
            {
                *errorOut = "absurd tensor name length";
            }
            return false;
        }
        std::vector<uint8_t> nameBa = file.read(static_cast<int>(nameLen));
        te.name.assign(reinterpret_cast<const char*>(nameBa.data()), nameBa.size());
        uint32_t nDims = 0;
        ds >> nDims;
        te.shape.resize(nDims);
        uint64_t nElements = 1;
        for (uint32_t d = 0; d < nDims; ++d)
        {
            uint64_t dim = 0;
            ds >> dim;
            te.shape[d] = dim;
            nElements *= dim;
        }
        uint32_t typeRaw = 0;
        ds >> typeRaw;
        te.tensorType = typeRaw;
        uint64_t relOff = 0;
        ds >> relOff;
        te.byteOffset = relOff;
        te.elementCount = nElements;
        ClassifyTensor(te.name, te.category, te.layerId);
        size_t pay = 0;
        if (!RawrXD::GgufTensorBytes::payloadBytes(typeRaw, static_cast<size_t>(nElements), pay))
        {
            te.byteSize = 0;
        }
        else
        {
            te.byteSize = static_cast<uint64_t>(pay);
        }
        out.tensors.push_back(std::move(te));
        if (ds.status() != StreamStatus::Ok)
        {
            if (errorOut)
            {
                *errorOut = "truncated tensor info table";
            }
            return false;
        }
    }

    const std::streampos endMetaPos = file.getStream().tellg();
    if (endMetaPos == std::streampos(-1))
    {
        if (errorOut)
        {
            *errorOut = "tellg failed after tensor headers";
        }
        return false;
    }
    const uint64_t endMeta = static_cast<uint64_t>(endMetaPos);
    const uint64_t align = tensorAlign == 0 ? 32u : static_cast<uint64_t>(tensorAlign);
    const uint64_t tensorDataBase = (endMeta + align - 1u) / align * align;

    const uint64_t fileSizeU = static_cast<uint64_t>(fileSize64);
    out.totalParams = 0;
    for (auto& te : out.tensors)
    {
        const uint64_t rel = te.byteOffset;
        if (rel > UINT64_MAX - tensorDataBase)
        {
            if (errorOut)
            {
                *errorOut = "tensor offset overflow";
            }
            return false;
        }
        const uint64_t absOff = tensorDataBase + rel;
        if (absOff > fileSizeU)
        {
            if (errorOut)
            {
                *errorOut = "tensor absolute offset past EOF";
            }
            return false;
        }
        te.byteOffset = absOff;
        out.totalParams += te.elementCount;

        if (streamOut)
        {
            *streamOut << "[tensor] " << te.name << " type=" << te.tensorType << " cat=" << te.category
                       << " layer=" << te.layerId << " elems=" << te.elementCount << " bytes=" << te.byteSize
                       << " off=" << te.byteOffset << std::endl;
        }
    }

    out.tensorCount = static_cast<uint64_t>(out.tensors.size());
    out.modelName = filePath;
    return true;
}

}  // namespace RawrXD

