#pragma once
#include <string>

namespace rawrxd {
namespace wal_gutter {

inline std::string getProgrammaticDepth(const std::string& ctx) {
    (void)ctx;
    return "";
}

inline int programmaticMutationDepth() {
    return 0;
}

} // namespace wal_gutter
} // namespace rawrxd
