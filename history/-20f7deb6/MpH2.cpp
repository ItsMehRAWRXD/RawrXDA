/**
 * @file RawrXD_QtCompat.cpp
 * @brief Qt compatibility shims implementation
 */

#include "RawrXD_QtCompat.hpp"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <random>
#include <thread>

namespace RawrXD::QtCompat {

QUuid QUuid::createUuid() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);

    QUuid uuid;
    std::ostringstream oss;
    
    for (int i = 0; i < 8; i++) {
        oss << std::hex << dis(gen);
    }
    oss << "-";
    for (int i = 0; i < 4; i++) {
        oss << std::hex << dis(gen);
    }
    oss << "-";
    oss << "4";  // version 4
    for (int i = 0; i < 3; i++) {
        oss << std::hex << dis(gen);
    }
    oss << "-";
    oss << std::hex << (dis(gen) | 0x8);  // variant
    for (int i = 0; i < 3; i++) {
        oss << std::hex << dis(gen);
    }
    oss << "-";
    for (int i = 0; i < 12; i++) {
        oss << std::hex << dis(gen);
    }

    uuid.uuid_str_ = oss.str();
    return uuid;
}

std::string QUuid::toString() const {
    return uuid_str_;
}

QDateTime QDateTime::currentDateTime() {
    QDateTime dt;
    dt.timestamp_ = std::chrono::system_clock::now().time_since_epoch().count();
    return dt;
}

long long QDateTime::toMSecsSinceEpoch() const {
    return timestamp_;
}

void QTimer::start() {
    if (interval_ms_ <= 0) return;

    std::thread([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms_));
        if (on_timeout) {
            on_timeout();
        }
    }).detach();
}

void QTimer::stop() {
    // Simple implementation - in production would need proper thread cancellation
}

} // namespace RawrXD::QtCompat
