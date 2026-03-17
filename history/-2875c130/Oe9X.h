#pragma once

#include <cstdint>

namespace rawrxd::re {

struct PeToolsInfo {
    std::uint32_t api_version;
};

PeToolsInfo get_pe_tools_info() noexcept;

} // namespace rawrxd::re
