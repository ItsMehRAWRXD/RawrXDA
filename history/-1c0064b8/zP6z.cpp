#include "model_anatomy.hpp"
// #include "../gguf_loader.h" // IGGUFLoader interface not available in current build
#include <sstream>
#include <algorithm>
#include <cstring>
#include <iomanip>

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

} // namespace RawrXD

