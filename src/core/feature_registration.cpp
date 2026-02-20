// ============================================================================

// feature_registration.cpp — DEPRECATED: Manual Registration Replaced

// ============================================================================

// Architecture: C++20, Win32, no Qt, no exceptions

//

// ┌──────────────────────────────────────────────────────────────────────────┐

// │ THIS FILE IS NOW DEPRECATED.                                            │

// │                                                                          │

// │ All feature registration is auto-generated from COMMAND_TABLE in         │

// │ command_registry.hpp by the AutoRegistrar in unified_command_dispatch.cpp.│

// │                                                                          │

// │ The 631 lines of manual reg() calls have been replaced by a single       │

// │ loop that reads g_commandRegistry[] and populates SharedFeatureRegistry.  │

// │                                                                          │

// │ To add a new command: Add ONE line to COMMAND_TABLE in                    │

// │ command_registry.hpp. Everything else is automatic.                       │

// │                                                                          │

// │ This file is kept as a compilation unit to avoid build breakage.          │

// │ It compiles to zero code.                                                │

// └──────────────────────────────────────────────────────────────────────────┘

//

// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.

// ============================================================================



// The auto-registration is performed by unified_command_dispatch.cpp

// via the static AutoRegistrar object. That code iterates

// g_commandRegistry[] and calls SharedFeatureRegistry::registerFeature()

// for every entry in COMMAND_TABLE.

//

// No manual registration calls needed here anymore.

// This file intentionally compiles to nothing.

