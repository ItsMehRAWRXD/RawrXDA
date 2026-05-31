#pragma once

#include <cstdint>
#include <expected>
#include <mutex>
#include <string>
#include <vector>

namespace RawrXD
{

struct EngineRevolverStep
{
    std::size_t index{0};
    std::string id;
    std::uint64_t generation{0};
};

class EngineRevolver
{
  public:
    static EngineRevolver& instance();

    std::expected<void, std::string> setRing(std::vector<std::string> ids);
    const std::vector<std::string>& ring() const { return m_ids; }

    std::expected<EngineRevolverStep, std::string> advance();
    EngineRevolverStep peek() const;
    void reset();

  private:
    EngineRevolver();
    mutable std::mutex m_mutex;
    std::vector<std::string> m_ids;
    std::size_t m_next{0};
    std::uint64_t m_generation{0};
};

}  // namespace RawrXD
