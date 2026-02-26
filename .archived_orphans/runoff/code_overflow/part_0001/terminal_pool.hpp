#pragma once

/**
 * @file terminal_pool.hpp
 * @brief Stub: pool of terminal tabs (used by bounded_autonomous_executor).
 */

class TerminalPool {
public:
    explicit TerminalPool(int count, void* parent = nullptr);
private:
    int m_count = 0;
    void* m_parent = nullptr;
};
