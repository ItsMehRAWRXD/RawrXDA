// ExtensionHost_stub.cpp
// Minimal link stub providing the symbols Win32IDE_Commands.cpp references
// from `RawrXD::Extensions::ExtensionHost` without dragging in the full
// ExtensionHost subsystem (sandbox manager, host process, VS Code API).
// The unified inference pipeline does not depend on extensions, so a
// graceful no-op surface is sufficient for runtime behavior.
#include "ExtensionHost.h"
#include "ExtensionHostProcess.h"

namespace RawrXD::Extensions {

// IPC_Channel is forward-declared in ExtensionHostProcess.h and held by
// std::unique_ptr inside ExtensionHostProcess.  Provide a minimal empty
// definition here so the unique_ptr destructor can complete.
class IPC_Channel { };

// IProcessBroker is similarly forward-declared and held by unique_ptr.
class IProcessBroker { };

// ---------------------------------------------------------------------------
// ExtensionHostProcess destructor — needed because ExtensionHost holds
// std::unique_ptr<ExtensionHostProcess> members; the unique_ptr destructor
// instantiates this dtor at the singleton's static-storage destruction.
// ---------------------------------------------------------------------------
ExtensionHostProcess::~ExtensionHostProcess() = default;

// ---------------------------------------------------------------------------
// ExtensionHost
// ---------------------------------------------------------------------------
ExtensionHost::~ExtensionHost() = default;

ExtensionHost& ExtensionHost::GetInstance() {
    static ExtensionHost g_instance;
    return g_instance;
}

bool ExtensionHost::ExecuteCommand(const std::string& /*commandId*/,
                                   const std::string& /*args*/) {
    return false;
}

std::vector<std::string> ExtensionHost::GetAvailableCommands() const {
    return {};
}

} // namespace RawrXD::Extensions
