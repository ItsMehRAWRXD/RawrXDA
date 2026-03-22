#pragma once

#include <cstdint>
#include <expected>
#include <string>

namespace RawrXD::Runtime {

enum class Lane : std::uint8_t {
    Parse = 0,
    Execute = 1,
    Ui = 2,
    Render = 3,
    Memory = 4,
};

constexpr std::size_t kLaneCount = 5;
/// At most four of the five lanes may be active unless a fifth is explicitly opened as a loaded module.
constexpr int kMaxConcurrentLanes = 4;

using RuntimeResult = std::expected<void, std::string>;

inline std::string laneName(Lane l) {
    switch (l) {
        case Lane::Parse: return "Parse";
        case Lane::Execute: return "Execute";
        case Lane::Ui: return "Ui";
        case Lane::Render: return "Render";
        case Lane::Memory: return "Memory";
        default: return "?";
    }
}

}  // namespace RawrXD::Runtime
