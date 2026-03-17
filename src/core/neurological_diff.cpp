#include "neurological_diff.hpp"
#include <sstream>
#include <unordered_map>
#include <iomanip>

static void jsonEscape(std::ostringstream& os, const std::string& s) {
    for (char c : s) {
        if (c == '"') os << "\\\"";
        else if (c == '\\') os << "\\\\";
        else if (c >= 32 && c != 127) os << c;
        else os << "\\u" << std::hex << std::setfill('0') << std::setw(4) << (unsigned)(unsigned char)c << std::dec;
    }
}

namespace RawrXD {

std::vector<DiffEntry> DiffAnatomies(const ModelAnatomy& a, const ModelAnatomy& b, std::ostream* streamOut) {
    std::vector<DiffEntry> out;
    std::unordered_map<std::string, const TensorEntry*> mapA, mapB;
    for (const auto& t : a.tensors) mapA[t.name] = &t;
    for (const auto& t : b.tensors) mapB[t.name] = &t;

    auto emit = [&](DiffEntry e) {
        out.push_back(e);
        if (streamOut) {
            const char* k = "?";
            switch (e.kind) {
                case DiffKind::OnlyInA:       k = "only_in_A"; break;
                case DiffKind::OnlyInB:       k = "only_in_B"; break;
                case DiffKind::SizeMismatch:  k = "size_mismatch"; break;
                case DiffKind::TypeMismatch:  k = "type_mismatch"; break;
                case DiffKind::ShapeMismatch: k = "shape_mismatch"; break;
                default: break;
            }
            *streamOut << "[diff] " << k << " " << e.tensorName;
            if (e.valueA != e.valueB) *streamOut << " A=" << e.valueA << " B=" << e.valueB;
            *streamOut << std::endl;
        }
    };

    for (const auto& t : a.tensors) {
        auto it = mapB.find(t.name);
        if (it == mapB.end()) {
            emit({ DiffKind::OnlyInA, t.name, 0, 0, t.layerId });
            continue;
        }

        const TensorEntry& tb = *it->second;
        if (t.byteSize != tb.byteSize)
            emit({ DiffKind::SizeMismatch, t.name, t.byteSize, tb.byteSize, t.layerId });
        if (t.tensorType != tb.tensorType)
            emit({ DiffKind::TypeMismatch, t.name, t.tensorType, tb.tensorType, t.layerId });
        if (t.shape != tb.shape)
            emit({ DiffKind::ShapeMismatch, t.name, static_cast<uint64_t>(t.shape.size()), static_cast<uint64_t>(tb.shape.size()), t.layerId });
    }

    for (const auto& t : b.tensors) {
        if (mapA.find(t.name) == mapA.end())
            emit({ DiffKind::OnlyInB, t.name, 0, 0, t.layerId });
    }
    return out;
}

std::string ExportDiffToJson(const std::vector<DiffEntry>& diff, const std::string& nameA, const std::string& nameB, bool pretty) {
    std::ostringstream os;
    const char* kStr[] = { "none", "only_in_A", "only_in_B", "size_mismatch", "type_mismatch", "shape_mismatch" };
    os << "{\"model_a\":\""; jsonEscape(os, nameA);
    os << "\",\"model_b\":\""; jsonEscape(os, nameB);
    os << "\",\"count\":" << diff.size() << ",\"diffs\":[";
    for (size_t i = 0; i < diff.size(); ++i) {
        const auto& d = diff[i];
        int k = static_cast<int>(d.kind);
        if (k < 0 || k > 5) k = 0;
        if (i) os << ",";
        if (pretty) os << "\n  ";
        os << "{\"kind\":\"" << kStr[k] << "\",\"tensor\":\""; jsonEscape(os, d.tensorName);
        os << "\",\"val_a\":" << d.valueA << ",\"val_b\":" << d.valueB << ",\"layer\":" << d.layerId << "}";
    }
    os << "]}";
    return os.str();
}

} // namespace RawrXD

