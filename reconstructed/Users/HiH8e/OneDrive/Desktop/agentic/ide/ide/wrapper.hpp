#pragma once

#include <memory>

namespace ide {

class IDE;

/**
 * Thin convenience wrapper that owns an IDE instance and exposes
 * "start"/"stop" style lifecycle methods so native callers (or
 * future GUI front-ends) can bootstrap the console IDE without
 * touching the details of the interactive loop.
 */
class IDEWrapper {
public:
    IDEWrapper();
    ~IDEWrapper();

    // Non-copyable, movable
    IDEWrapper(const IDEWrapper&) = delete;
    IDEWrapper& operator=(const IDEWrapper&) = delete;
    IDEWrapper(IDEWrapper&&) noexcept;
    IDEWrapper& operator=(IDEWrapper&&) noexcept;

    /**
     * Launch the wrapped IDE instance. Returns the exit code produced
     * by the interactive loop (0 on success, non-zero on failure).
     */
    int run();

private:
    std::unique_ptr<IDE> ide_;
};

} // namespace ide
