#pragma once

#include "model_anatomy.hpp"
#include <string>
#include <vector>

namespace RawrXD {

/// Kind of difference between two models.
enum class DiffKind {
    None,
    OnlyInA,      // Tensor only in model A
    OnlyInB,      // Tensor only in model B
    SizeMismatch, // Same name, different byte size
    TypeMismatch, // Same name, different type
    ShapeMismatch // Same name, different shape
};

/// One reported difference.
struct DiffEntry {
    DiffKind kind = DiffKind::None;
    std::string tensorName;
    uint64_t valueA = 0;  // size or type for A
    uint64_t valueB = 0;  // size or type for B
    int layerId = -1;
};

/// Compare two anatomies (e.g. base vs fine-tuned, or before/after hotpatch).
/// Results streamed to optional ostream; returns list of differences.
std::vector<DiffEntry> DiffAnatomies(
    const ModelAnatomy& a,
    const ModelAnatomy& b,
    std::ostream* streamOut = nullptr
);

/// Export diff to JSON.
std::string ExportDiffToJson(const std::vector<DiffEntry>& diff, const std::string& nameA, const std::string& nameB, bool pretty = true);

} // namespace RawrXD
