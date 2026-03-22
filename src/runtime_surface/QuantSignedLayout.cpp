// QuantSignedLayout — implementation TU (parity tests / tables live here as the format grows).

#include "rawrxd/runtime/QuantSignedLayout.hpp"

#include "../logging/Logger.h"

#include <atomic>

namespace RawrXD::Runtime::Quant
{

void LogQuantLayoutLegendOnce()
{
    static std::atomic<bool> s_logged{false};
    if (!s_logged.exchange(true))
    {
        RawrXD::Logging::Logger::instance().info(
            "[RawrXD::Quant] Layout Legend: Q4S (Signed 4-bit): UI '-4bit', Wire: val+8, Range [-8,7]. "
            "Q6S (Signed 6-bit): UI '-6bit', Wire: val+32, Range [-32,31]. "
            "Q4U/Q6U: Standard Unsigned Mapping.",
            "RuntimeSurface");
    }
}

}  // namespace RawrXD::Runtime::Quant
