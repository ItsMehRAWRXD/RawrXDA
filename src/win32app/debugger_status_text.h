#pragma once

#include <cstddef>
#include <string>

namespace rawrxd::win32app {

constexpr std::size_t kDebuggerStatusTextLimit = 256;

inline std::string formatDebuggerStatusText(const std::string& message) {
    if (message.size() <= kDebuggerStatusTextLimit) {
        return message;
    }

    std::string statusMessage = message;
    statusMessage.resize(kDebuggerStatusTextLimit - 3);
    statusMessage += "...";
    return statusMessage;
}

}  // namespace rawrxd::win32app
