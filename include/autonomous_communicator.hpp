// autonomous_communicator.hpp — Stub for build compatibility
#pragma once
#include <string>
#include <vector>

namespace RawrXD {
namespace Autonomy {

enum class ReportType {
    Standup = 0,
    Summary = 1,
    Detailed = 2
};

struct ReportResult {
    bool success = false;
    std::string detail;
    std::vector<std::string> items;
};

struct InitResult {
    bool success = true;
    std::string detail;
};

class AutonomousCommunicator {
public:
    static AutonomousCommunicator& instance() {
        static AutonomousCommunicator inst;
        return inst;
    }

    bool isActive() const { return active_; }
    InitResult initialize() { active_ = true; return {true, ""}; }
    void shutdown() { active_ = false; }

    ReportResult generateReport(ReportType /*type*/) {
        return {true, "Stub report", {"Item 1", "Item 2"}};
    }

    std::string reportToMarkdown(const ReportResult& /*report*/) {
        return "# Autonomous Communicator Report\n\n(stub)\n";
    }

    void recordReasoning(const std::string& /*reason*/, const std::string& /*context*/ = "", const std::string& /*detail*/ = "", float /*confidence*/ = 0.0f) {}

private:
    bool active_ = false;
};

} // namespace Autonomy
} // namespace RawrXD
