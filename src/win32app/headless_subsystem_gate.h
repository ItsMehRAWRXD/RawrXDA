#pragma once
// ============================================================================
// HeadlessSubsystemGate — Two-phase init to prevent callback-during-load crashes
// Phase 1 (PreLoad):  Subsystems start threads / alloc buffers. NO model access.
// Phase 2 (PostLoad): Subsystems receive valid model handle. Safe to dereference.
// ============================================================================

#include <vector>
#include <functional>
#include <atomic>

class HeadlessSubsystemGate {
public:
    static HeadlessSubsystemGate& Instance() {
        static HeadlessSubsystemGate g;
        return g;
    }

    using PreLoadInit  = std::function<bool()>;
    using PostLoadInit = std::function<bool()>;

    void RegisterPreLoad(PreLoadInit fn)  { pre_.push_back(std::move(fn)); }
    void RegisterPostLoad(PostLoadInit fn){ post_.push_back(std::move(fn)); }

    bool RunPreLoad() {
        for (auto& fn : pre_) {
            if (!fn()) return false;
        }
        return true;
    }

    bool RunPostLoad() {
        for (auto& fn : post_) {
            if (!fn()) return false;
        }
        return true;
    }

    void Clear() { pre_.clear(); post_.clear(); }

private:
    std::vector<PreLoadInit>  pre_;
    std::vector<PostLoadInit> post_;
};
