#pragma once
// ============================================================================
// model_puller_cli.h — CLI Command Integration for Model Puller
// ============================================================================
// Provides the `rawrxd model` CLI commands:
//   rawrxd model pull <source>
//   rawrxd model list <repo>
//   rawrxd model list-local
//   rawrxd model search <query>
//   rawrxd model remove <id>
//   rawrxd model info <id>
//   rawrxd model set-active <id>
//   rawrxd model register <path>
// ============================================================================

#ifndef RAWRXD_MODEL_PULLER_CLI_H
#define RAWRXD_MODEL_PULLER_CLI_H

#include <string>
#include <vector>

namespace RawrXD {

class ModelPullerCLI {
public:
    // Run the "model" subcommand. Returns exit code.
    //   argv[0] = "model"
    //   argv[1] = subcommand ("pull", "list", "list-local", etc.)
    //   argv[2..] = arguments
    static int Run(int argc, const char* argv[]);

    // Individual command handlers
    static int CmdPull(const std::vector<std::string>& args);
    static int CmdList(const std::vector<std::string>& args);
    static int CmdListLocal(const std::vector<std::string>& args);
    static int CmdSearch(const std::vector<std::string>& args);
    static int CmdRemove(const std::vector<std::string>& args);
    static int CmdInfo(const std::vector<std::string>& args);
    static int CmdSetActive(const std::vector<std::string>& args);
    static int CmdRegister(const std::vector<std::string>& args);

    // Print usage help
    static void PrintHelp();

private:
    // Pretty-print helpers
    static void PrintProgressBar(double percent, double speedMBps, int etaSeconds);
    static std::string FormatSize(uint64_t bytes);
    static std::string FormatSpeed(double bytesPerSec);
    static std::string FormatETA(int seconds);
};

} // namespace RawrXD

#endif // RAWRXD_MODEL_PULLER_CLI_H
