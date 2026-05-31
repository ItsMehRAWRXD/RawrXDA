// text_edit_utils.cpp - Text edit computation utilities
#include "../include/collaboration/live_share.h"
#include "core/diff_engine.h"
#include <algorithm>
#include <vector>

using namespace RawrXD::Collaboration;
using namespace RawrXD::Core::Diff;

// Production-grade diff using Myers' algorithm
std::vector<TextEdit> computeEdits(const std::string& from, const std::string& to) {
    std::vector<TextEdit> edits;
    
    if (from == to) {
        return edits; // No changes needed
    }
    
    DiffEngine engine;
    auto result = engine.diffStrings(from, to);
    
    size_t pos = 0;
    for (const auto& chunk : result.chunks) {
        switch (chunk.op) {
            case DiffOp::Equal:
                pos += chunk.oldCount; // Advance by line count (approximate char pos)
                break;
            case DiffOp::Delete:
                edits.push_back({pos, chunk.oldCount, ""});
                break;
            case DiffOp::Insert:
                edits.push_back({pos, 0, joinLines(chunk.newLines)});
                break;
            case DiffOp::Replace:
                edits.push_back({pos, chunk.oldCount, joinLines(chunk.newLines)});
                pos += chunk.oldCount;
                break;
        }
    }
    
    return edits;
}
