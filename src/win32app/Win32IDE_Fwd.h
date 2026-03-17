#pragma once

// =============================================================================
// Win32IDE_Fwd.h — Forward declarations for Win32IDE subsystems
// Include this header in subsystem .cpp files that only need to hold a
// Win32IDE* pointer or reference, without pulling in the full class definition.
// =============================================================================

// Class forward declarations (ordered alphabetically)
class BenchmarkMenu;
class CheckpointManager;
class CICDSettings;
class FeatureRegistryPanel;
class InterpretabilityPanel;
class ModelRegistry;
class MultiFileSearchWidget;
class MultiResponseEngine;
class SubAgentManager;

namespace RawrXD {
    class AutonomousAgenticPipelineCoordinator;
    class GhostTextRenderer;
    namespace LSPServer { class RawrXDLSPServer; }
}

namespace vscode { class VSCodeExtensionAPI; }

// DirectWrite forward declarations (avoids pulling in <dwrite.h> in every TU)
struct IDWriteFactory;
struct IDWriteTextFormat;
struct IDWriteTextLayout;
