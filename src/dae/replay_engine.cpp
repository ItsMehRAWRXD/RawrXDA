#include "replay_engine.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace RawrXD {
namespace DAE {

namespace {

bool g_asmHotpathEnabled = false;

ContentHash BlendHash(const ContentHash& a, const ContentHash& b) {
    uint8_t out[32] = {};
    for (int i = 0; i < 32; ++i) {
        out[i] = static_cast<uint8_t>((a.bytes[i] ^ b.bytes[i]) << 1 | (a.bytes[i] ^ b.bytes[i]) >> 7);
    }
    std::string v(reinterpret_cast<const char*>(out), 32);
    return ShadowFilesystem::ComputeHash(v);
}

ContentHash StateDigest(const ShadowFilesystem& fs) {
    auto entries = fs.DirtyEntries();
    std::sort(entries.begin(), entries.end(), [](const FileEntry& l, const FileEntry& r) {
        return l.path < r.path;
    });

    std::ostringstream os;
    for (const auto& e : entries) {
        os << e.path << '|' << (e.isDeleted ? 'D' : 'A') << '|'
           << e.sizeBytes << '|';
        for (uint8_t b : e.overlayHash.bytes) {
            os << static_cast<char>(b);
        }
    }
    return ShadowFilesystem::ComputeHash(os.str());
}

ContentHash ActionDigest(const IntentNode& node) {
    std::ostringstream os;
    os << static_cast<uint32_t>(node.opType) << '|'
       << node.targetPath << '|'
       << node.idempotencyKey << '|'
       << node.payload;
    return ShadowFilesystem::ComputeHash(os.str());
}

std::vector<TraceEvent> LoadReferenceTrace(std::string_view path) {
    std::vector<TraceEvent> out;
    std::ifstream in(std::string(path));
    if (!in.is_open()) return out;

    std::string line;
    while (std::getline(in, line)) {
        std::vector<std::string> p;
        std::stringstream ss(line);
        std::string item;
        while (std::getline(ss, item, '\t')) p.push_back(item);
        if (p.size() < 7) continue;

        TraceEvent e;
        e.seq = static_cast<uint64_t>(std::strtoull(p[0].c_str(), nullptr, 10));
        e.timestampUs = static_cast<uint64_t>(std::strtoull(p[1].c_str(), nullptr, 10));
        e.opTypeName = p[2];
        e.targetPath = p[3];
        // We only need op + path + seq + hashes for divergence checks.
        out.push_back(std::move(e));
    }
    return out;
}

} // namespace

ReplayEngine::ReplayEngine(ShadowFilesystem& shadowFs,
                           TraceLog& traceLog)
    : m_shadowFs(shadowFs), m_traceLog(traceLog) {}

void ReplayEngine::SetDivergenceCallback(DivergenceCb cb) {
    m_divergenceCb = std::move(cb);
}

void ReplayEngine::SetAsmHotpathEnabled(bool enabled) noexcept {
    g_asmHotpathEnabled = enabled;
}

bool ReplayEngine::IsAsmHotpathEnabled() noexcept {
    return g_asmHotpathEnabled;
}

uint32_t ReplayEngine::ValidateTransitionCpp(const TransitionContext& ctx) noexcept {
    if (ctx.opType > 10u && ctx.opType != 0xFFFFu) {
        return 2u; // InvalidOpType
    }

    if (ctx.seq > 0u) {
        bool allZero = true;
        for (uint8_t b : ctx.stateHash) {
            if (b != 0) { allZero = false; break; }
        }
        if (allZero) {
            return 3u; // UninitialisedStateHash
        }
    }

    return 0u;
}

ReplayResult<void> ReplayEngine::ApplyNode(const IntentNode& node,
                                           const ReplayConfig& config,
                                           uint64_t seq,
                                           const ContentHash& prevHash,
                                           ReplaySummary& summary) {
    (void)prevHash;

    if (node.opType == OpType::NoOp) {
        summary.nodessSkipped++;
        return {};
    }

    if (config.mode == ReplayMode::Audit) {
        summary.nodessSkipped++;
        return {};
    }

    if (node.opType == OpType::RunTool) {
        ToolInvocation inv;
        inv.toolName = node.targetPath;
        inv.idempotencyKey = node.idempotencyKey;
        inv.argsJson = node.payload;
        inv.dryRun = (config.mode == ReplayMode::Speculate);

        auto tool = ToolRegistry::Instance().Invoke(inv, config.toolPolicy);
        if (!tool.has_value()) {
            return std::unexpected(ReplayError::ToolFailed);
        }
        summary.toolsInvoked++;
        return {};
    }

    if (config.mode == ReplayMode::Speculate) {
        if (node.opType == OpType::CreateFile || node.opType == OpType::PatchFile || node.opType == OpType::SetMetadata) {
            auto rc = m_shadowFs.Write(node.targetPath, node.payload);
            if (!rc.has_value()) return std::unexpected(ReplayError::Aborted);
            return {};
        }
        if (node.opType == OpType::DeletePath) {
            auto rc = m_shadowFs.Delete(node.targetPath);
            if (!rc.has_value()) return std::unexpected(ReplayError::Aborted);
            return {};
        }
        if (node.opType == OpType::MovePath) {
            // payload format: "from|to"
            auto pos = node.payload.find('|');
            if (pos == std::string::npos) return std::unexpected(ReplayError::Aborted);
            auto rc = m_shadowFs.Move(node.payload.substr(0, pos), node.payload.substr(pos + 1));
            if (!rc.has_value()) return std::unexpected(ReplayError::Aborted);
            return {};
        }
        return {};
    }

    // Execute mode
    if (node.opType == OpType::CreateFile || node.opType == OpType::PatchFile || node.opType == OpType::SetMetadata) {
        auto rc = m_shadowFs.Write(node.targetPath, node.payload);
        if (!rc.has_value()) return std::unexpected(ReplayError::Aborted);
        return {};
    }
    if (node.opType == OpType::DeletePath) {
        auto rc = m_shadowFs.Delete(node.targetPath);
        if (!rc.has_value()) return std::unexpected(ReplayError::Aborted);
        return {};
    }
    if (node.opType == OpType::MovePath) {
        auto pos = node.payload.find('|');
        if (pos == std::string::npos) return std::unexpected(ReplayError::Aborted);
        auto rc = m_shadowFs.Move(node.payload.substr(0, pos), node.payload.substr(pos + 1));
        if (!rc.has_value()) return std::unexpected(ReplayError::Aborted);
        return {};
    }

    return {};
}

ReplayResult<ReplaySummary> ReplayEngine::Run(const IntentGraph& graph,
                                              const ReplayConfig& config) {
    ReplaySummary summary{};

    auto v = graph.Validate();
    if (!v.has_value()) {
        return std::unexpected(ReplayError::GraphInvalid);
    }

    auto p = graph.ValidatePreconditions(m_shadowFs);
    if (!p.has_value()) {
        return std::unexpected(ReplayError::PreconditionFailed);
    }

    auto order = graph.TopologicalOrder();
    if (!order.has_value()) {
        return std::unexpected(ReplayError::GraphInvalid);
    }

    std::vector<TraceEvent> reference;
    if (!config.referenceTracePath.empty()) {
        reference = LoadReferenceTrace(config.referenceTracePath);
    }

    auto started = std::chrono::steady_clock::now();
    ContentHash prevHash = StateDigest(m_shadowFs);

    uint64_t seq = 0;
    for (uint64_t nodeId : *order) {
        const IntentNode* node = graph.FindNode(nodeId);
        if (!node) return std::unexpected(ReplayError::GraphInvalid);

        TransitionContext ctx{};
        ctx.seq = seq;
        std::memcpy(ctx.stateHash, prevHash.bytes, 32);

        ContentHash act = ActionDigest(*node);
        std::memcpy(ctx.actionDigest, act.bytes, 32);
        ctx.opType = static_cast<uint32_t>(node->opType);
        ctx.reserved = 0;

        uint32_t ok = ValidateTransitionCpp(ctx);
        if (ok != 0) {
            return std::unexpected(ReplayError::TransitionInvalid);
        }

        auto applied = ApplyNode(*node, config, seq, prevHash, summary);
        if (!applied.has_value()) {
            return std::unexpected(applied.error());
        }

        ContentHash afterHash = StateDigest(m_shadowFs);

        TraceEvent ev;
        ev.seq = seq + 1;
        ev.timestampUs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - started).count());
        ev.opTypeName = std::string(OpTypeName(node->opType));
        ev.targetPath = node->targetPath;
        ev.stateBeforeHash = prevHash;
        ev.actionDigest = act;
        ev.stateAfterHash = afterHash;
        ev.succeeded = true;

        if (m_traceLog.IsOpen()) {
            auto append = m_traceLog.Append(ev);
            if (!append.has_value()) {
                return std::unexpected(ReplayError::LogWriteFailed);
            }
        }

        // Divergence check against reference trace (if provided)
        if (!reference.empty() && seq < reference.size()) {
            if (reference[seq].opTypeName != ev.opTypeName || reference[seq].targetPath != ev.targetPath) {
                DivergenceEvent d{};
                d.seq = ev.seq;
                d.opTypeName = ev.opTypeName;
                d.targetPath = ev.targetPath;
                d.expectedHash = prevHash;
                d.actualHash = afterHash;
                d.context = "operation mismatch vs reference trace";
                summary.divergences.push_back(d);
                if (m_divergenceCb) m_divergenceCb(d);
                if (config.abortOnDivergence) {
                    m_shadowFs.Reset();
                    return std::unexpected(ReplayError::Diverged);
                }
            }
        }

        prevHash = afterHash;
        ++seq;
        summary.nodesExecuted++;
    }

    summary.finalStateHash = prevHash;
    summary.allSucceeded = true;

    if (config.mode == ReplayMode::Execute || config.mode == ReplayMode::Speculate) {
        m_shadowFs.SetReplayValidated(true);
    }

    return summary;
}

} // namespace DAE
} // namespace RawrXD
