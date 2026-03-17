#pragma once

#include <optional>
#include <exception>
#include <QLoggingCategory>
#include "Logging.h"

namespace rawrxd::safety {

template <typename F>
auto captureErrors(F&& fn, const char* operation) -> std::optional<decltype(fn())> {
    QLoggingCategory cat("rawrxd.errors");
    try {
        RAWRXD_SCOPED_TIMER(operation, cat);
        return std::optional<decltype(fn())>(fn());
    } catch (const std::exception& ex) {
        qCCritical(cat) << "Unhandled exception" << operation << "message" << ex.what();
        return std::nullopt;
    } catch (...) {
        qCCritical(cat) << "Unhandled non-std exception" << operation;
        return std::nullopt;
    }
}

} // namespace rawrxd::safety
