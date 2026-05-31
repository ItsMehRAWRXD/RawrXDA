// composer_parallel_edit.h - Multi-file parallel edit composer (Cursor Composer parity)
// Feature 4/15 (Cursor parity).
#pragma once

#include <atomic>
#include <functional>
#include <future>
#include <mutex>
#include <string>
#include <vector>

namespace rawrxd::parity {

struct FileEditSpec {
    std::string path;
    std::string original;        // original file content (may be empty for new files)
    std::string instruction;     // per-file instruction
    std::string language;
};

struct FileEditOutcome {
    std::string path;
    std::string new_content;
    std::string unified_diff;
    bool        ok{false};
    std::string error;
    std::uint64_t latency_ms{0};
};

using FileEditWorkerFn = std::function<FileEditOutcome(const FileEditSpec&)>;

class ComposerParallelEdit {
public:
    void set_worker(FileEditWorkerFn fn);
    void set_concurrency(std::uint32_t n);  // caps parallel futures

    // Runs edits concurrently and returns outcomes in input order.
    std::vector<FileEditOutcome> run(const std::vector<FileEditSpec>& specs);

    // Cancellation: signal running batches to abort as soon as possible.
    void cancel();
    bool cancelled() const;
    void reset_cancel();

private:
    mutable std::mutex mu_;
    FileEditWorkerFn  worker_;
    std::uint32_t     concurrency_{4};
    std::atomic<bool> cancel_{false};
};

} // namespace rawrxd::parity
