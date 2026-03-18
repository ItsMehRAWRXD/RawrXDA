#include "Win32IDE.h"
#include "autonomous_communicator.hpp"
#include <windows.h>

// Handler for autonomous communicator feature
void HandleAutonomousCommunicator(void* idePtr) {
    Win32IDE* ide = static_cast<Win32IDE*>(idePtr);
    if (!ide) return;

    // Initialize the autonomous communicator if not already active
    auto& comm = RawrXD::Autonomy::AutonomousCommunicator::instance();
    if (!comm.isActive()) {
        auto result = comm.initialize();
        if (!result.success) {
            MessageBoxA(NULL, ("Failed to initialize Autonomous Communicator: " + std::string(result.detail)).c_str(),
                       "Autonomous Communicator", MB_ICONERROR | MB_OK);
            return;
        }
    }

    // Generate a status report as a demo
    auto report = comm.generateReport(RawrXD::Autonomy::ReportType::Standup);
    std::string markdown = comm.reportToMarkdown(report);

    // Show the report in a message box (for demo purposes)
    MessageBoxA(NULL, markdown.c_str(), "Autonomous Communicator - Status Report",
               MB_ICONINFORMATION | MB_OK);

    // Record a reasoning step
    comm.recordReasoning("User initiated autonomous communication",
                        "Demonstrating autonomous communicator integration",
                        "Could have shown in IDE panel instead",
                        0.9f);
}