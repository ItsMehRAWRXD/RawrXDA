// RAWRXD Unified Command Router
// Single dispatch surface for GhostText, LSP, Titan, and autonomous systems

#pragma once

#include <cstdint>

// Command opcodes - unified execution surface
enum RX_CMD : uint16_t {
    RX_NOP = 0,

    // Ghost / LSP
    RX_LSP_REQUEST,
    RX_LSP_APPLY,

    // Titan
    RX_TITAN_INFER,
    RX_TITAN_CANCEL,

    // Editor
    RX_GHOST_ACCEPT,
    RX_GHOST_DISMISS,

    // System
    RX_STATUS,
    RX_HEALTH,

    // Autonomous
    RX_AGENT_START,
    RX_AGENT_STOP,
    RX_AGENT_QUERY,

    // Slash commands
    RX_SLASH_EXECUTE,

    // Count for array sizing
    RX_CMD_COUNT
};

// Unified command structure
struct RX_Command {
    RX_CMD opcode;
    const char* arg0;
    const char* arg1;
    void* ctx;

    // Constructor for easy command creation
    RX_Command(RX_CMD cmd, const char* a0 = nullptr, const char* a1 = nullptr, void* context = nullptr)
        : opcode(cmd), arg0(a0), arg1(a1), ctx(context) {}
};

// Handler function type
typedef void (*RX_Handler)(RX_Command&);

// Dispatch function declaration
void RX_Dispatch(RX_Command& cmd);

// Handler declarations (implemented in Win32IDE)
void Handle_LspRequest(RX_Command& cmd);
void Handle_LspApply(RX_Command& cmd);
void Handle_TitanInfer(RX_Command& cmd);
void Handle_TitanCancel(RX_Command& cmd);
void Handle_GhostAccept(RX_Command& cmd);
void Handle_GhostDismiss(RX_Command& cmd);
void Handle_Status(RX_Command& cmd);
void Handle_Health(RX_Command& cmd);
void Handle_AgentStart(RX_Command& cmd);
void Handle_AgentStop(RX_Command& cmd);
void Handle_AgentQuery(RX_Command& cmd);
void Handle_SlashExecute(RX_Command& cmd);