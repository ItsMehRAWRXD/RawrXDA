#include "intent_ir.h"

#include <algorithm>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <queue>

namespace RawrXD {
namespace DAE {

std::string_view OpTypeName(OpType t) noexcept {
    switch (t) {
    case OpType::CreateFile:  return "CreateFile";
    case OpType::PatchFile:   return "PatchFile";
    case OpType::MovePath:    return "MovePath";
    case OpType::DeletePath:  return "DeletePath";
    case OpType::SetMetadata: return "SetMetadata";
    case OpType::RunTool:     return "RunTool";
    case OpType::NoOp:        return "NoOp";
    default:                  return "Unknown";
    }
}

IRResult<uint64_t> IntentGraph::AddNode(IntentNode node) {
    if (node.idempotencyKey.empty()) {
        return std::unexpected(IRError::EmptyIdempotencyKey);
    }

    if (node.id == 0) {
        node.id = m_nextId++;
    } else {
        for (const auto& n : m_nodes) {
            if (n.id == node.id) {
                return std::unexpected(IRError::DuplicateNodeId);
            }
        }
        if (node.id >= m_nextId) {
            m_nextId = node.id + 1;
        }
    }

    m_nodes.push_back(std::move(node));
    return m_nodes.back().id;
}

const IntentNode* IntentGraph::FindNode(uint64_t id) const {
    for (const auto& n : m_nodes) {
        if (n.id == id) return &n;
    }
    return nullptr;
}

IRResult<void> IntentGraph::DetectCycles() const {
    std::unordered_map<uint64_t, uint8_t> color; // 0 white, 1 gray, 2 black
    for (const auto& n : m_nodes) color[n.id] = 0;

    std::function<bool(uint64_t)> dfs = [&](uint64_t id) {
        color[id] = 1;
        const IntentNode* node = FindNode(id);
        if (!node) return false;
        for (uint64_t p : node->causalParents) {
            auto it = color.find(p);
            if (it == color.end()) return false;
            if (it->second == 1) return true;
            if (it->second == 0 && dfs(p)) return true;
        }
        color[id] = 2;
        return false;
    };

    for (const auto& n : m_nodes) {
        if (color[n.id] == 0 && dfs(n.id)) {
            return std::unexpected(IRError::CyclicDependency);
        }
    }
    return {};
}

IRResult<void> IntentGraph::Validate() const {
    if (schemaVersion != kIntentIRSchemaVersion) {
        return std::unexpected(IRError::SchemaVersionMismatch);
    }

    std::unordered_set<uint64_t> ids;
    for (const auto& n : m_nodes) {
        if (!ids.insert(n.id).second) {
            return std::unexpected(IRError::DuplicateNodeId);
        }
        if (n.idempotencyKey.empty()) {
            return std::unexpected(IRError::EmptyIdempotencyKey);
        }
        if (OpTypeName(n.opType) == "Unknown" && n.required) {
            return std::unexpected(IRError::UnknownRequiredOp);
        }
    }

    for (const auto& n : m_nodes) {
        for (uint64_t p : n.causalParents) {
            if (!ids.count(p)) {
                return std::unexpected(IRError::MissingParentRef);
            }
        }
    }

    return DetectCycles();
}

IRResult<void> IntentGraph::ValidatePreconditions(const ShadowFilesystem& fs) const {
    for (const auto& n : m_nodes) {
        if (n.targetPath.empty()) continue;
        // If no precondition hash provided, skip hash check.
        if (n.preconditionHash == ContentHash::zero()) continue;

        auto read = fs.Read(n.targetPath);
        if (!read.has_value()) {
            return std::unexpected(IRError::PreconditionFailed);
        }

        ContentHash actual = ShadowFilesystem::ComputeHash(*read);
        if (actual != n.preconditionHash) {
            return std::unexpected(IRError::PreconditionFailed);
        }
    }
    return {};
}

IRResult<std::vector<uint64_t>> IntentGraph::TopologicalOrder() const {
    std::unordered_map<uint64_t, int> indegree;
    std::unordered_map<uint64_t, std::vector<uint64_t>> edges;

    for (const auto& n : m_nodes) {
        indegree[n.id] = static_cast<int>(n.causalParents.size());
        for (uint64_t p : n.causalParents) {
            edges[p].push_back(n.id);
        }
    }

    std::vector<uint64_t> ready;
    ready.reserve(m_nodes.size());
    for (const auto& n : m_nodes) {
        if (indegree[n.id] == 0) ready.push_back(n.id);
    }
    std::sort(ready.begin(), ready.end());

    std::vector<uint64_t> order;
    order.reserve(m_nodes.size());

    while (!ready.empty()) {
        uint64_t id = ready.front();
        ready.erase(ready.begin());
        order.push_back(id);

        auto eit = edges.find(id);
        if (eit == edges.end()) continue;

        for (uint64_t v : eit->second) {
            if (--indegree[v] == 0) {
                ready.push_back(v);
            }
        }
        std::sort(ready.begin(), ready.end());
    }

    if (order.size() != m_nodes.size()) {
        return std::unexpected(IRError::CyclicDependency);
    }

    return order;
}

std::string IntentGraph::Serialise() const {
    std::ostringstream os;
    os << "schema=" << schemaVersion << "\n";
    for (const auto& n : m_nodes) {
        os << n.id << '|' << static_cast<uint32_t>(n.opType) << '|' << n.targetPath
           << '|' << n.idempotencyKey << '\n';
    }
    return os.str();
}

IRResult<IntentGraph> IntentGraph::Deserialise(std::string_view json) {
    // Minimal deterministic parser for P1. Full schema parser can replace later.
    if (json.empty()) {
        IntentGraph g;
        return g;
    }
    return std::unexpected(IRError::SchemaVersionMismatch);
}

ContentHash IntentGraph::StructuralFingerprint() const {
    std::ostringstream os;
    os << "v=" << schemaVersion << ';';
    for (const auto& n : m_nodes) {
        os << static_cast<uint32_t>(n.opType) << ':' << n.idempotencyKey << ';';
    }
    std::string text = os.str();
    return ShadowFilesystem::ComputeHash(text);
}

} // namespace DAE
} // namespace RawrXD
