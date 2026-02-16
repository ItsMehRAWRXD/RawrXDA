# Command & Feature Registry Alignment

**Purpose:** Document how the SSOT command table, SharedFeatureRegistry, and auto_feature_registry stay in sync so handlers are discoverable the same way from GUI and CLI.

---

## 1. Single Source of Truth: COMMAND_TABLE

- **File:** `src/core/command_registry.hpp`
- **Macro:** `COMMAND_TABLE(X)` — one line per command: ID, symbol, canonical name, CLI alias, exposure (GUI/CLI/BOTH), category, handler, flags.
- **Rule:** Every GUI menu item and every CLI `!` command that is part of the canonical surface is defined here. Adding a command = add one line; removing = delete one line. Drift is structurally impossible.

## 2. Auto-Registration into SharedFeatureRegistry

- **File:** `src/core/unified_command_dispatch.cpp`
- **Mechanism:** Static `AutoRegistrar` runs before `main()`. It iterates `g_commandRegistry[]` (generated from COMMAND_TABLE) and calls `SharedFeatureRegistry::instance().registerFeature(fd)` for each entry.
- **Result:** SharedFeatureRegistry is populated from the compile-time table. No manual feature_registration.cpp for COMMAND_TABLE commands.

## 3. SharedFeatureRegistry

- **Role:** Runtime query layer. Lookup by canonical name, CLI alias, or command ID. Used by:
  - Win32 WM_COMMAND dispatch (menu/toolbar)
  - CLI terminal (`!` commands)
  - Manifest / help / telemetry
- **Population:**
  1. **From COMMAND_TABLE** (via AutoRegistrar) — primary user-facing commands.
  2. **From auto_feature_registry** (via `initAutoFeatureRegistry()`) — additional feature IDs and handlers, often aliases or alternate entry points (e.g. `hotpatch.server_add`, `hotpatch.server_remove` with different CLI names).

## 4. auto_feature_registry.cpp

- **File:** `src/core/auto_feature_registry.cpp`
- **Function:** `initAutoFeatureRegistry()` — calls `autoReg(...)` for hundreds of features.
- **Naming:** Uses dotted IDs like `agent.start_loop`, `hotpatch.server_add`. These may overlap or extend COMMAND_TABLE:
  - Some map to the **same** handler as COMMAND_TABLE (e.g. same handler registered under a second ID/alias).
  - Some are **extra** commands not in COMMAND_TABLE (e.g. `!hotpatch_server_add` vs `!hotpatch_server add`).
- **Alignment:** When adding a new user-facing command that should appear in menus and CLI, add it to **COMMAND_TABLE** first. If you also need a separate alias or alternate CLI name, register it in `initAutoFeatureRegistry()` with the same handler. Keep canonical names and CLI aliases documented in one place (COMMAND_TABLE) for the primary surface.

## 5. Handler Locations

| Source              | Handlers in COMMAND_TABLE | Handlers only in auto_feature_registry |
|---------------------|---------------------------|----------------------------------------|
| feature_handlers.cpp| Most real implementations  | —                                       |
| ssot_handlers.cpp   | delegateToGui + CLI text  | —                                       |
| ssot_handlers_ext.cpp | delegateToGui + CLI/AI  | —                                       |
| missing_handler_stubs.cpp | Stubs for unresolved refs | —                                |
| auto_feature_registry.cpp | —                    | handleHotpatchServerAdd, handleHotpatchServerRemove, and many autoReg entries |

COMMAND_TABLE references handlers by name (e.g. `handleHotpatchServer`, `handleHotpatchServerRemove`). Those symbols are defined in feature_handlers.cpp or ssot_handlers.cpp. The auto_feature_registry adds **additional** feature descriptors that may point to the same or different handlers (e.g. handleHotpatchServerAdd in auto_feature_registry.cpp).

## 6. Discoverability

- **GUI:** Menu/toolbar IDs come from COMMAND_TABLE (ID field). Dispatch uses SharedFeatureRegistry or direct ID → handler map from the same table.
- **CLI:** Parser resolves `!alias` via SharedFeatureRegistry (cliCommand / canonical name). Both COMMAND_TABLE-registered and auto_feature_registry-registered entries can be found if they set `cliSupported = true`.
- **Manifest:** Win32IDE_FeatureManifest.cpp maintains a **separate** static table of feature status (Real/Partial/Missing) per variant (Win32, CLI, Headless, Replay). It does not derive from the registries; keep it updated when a feature gains real CLI or GUI behavior.

## 7. Summary

| Registry / Table       | Purpose |
|------------------------|--------|
| **COMMAND_TABLE**      | SSOT for canonical commands (menu + CLI). One line per command. |
| **SharedFeatureRegistry** | Runtime lookup; populated from COMMAND_TABLE (AutoRegistrar) + initAutoFeatureRegistry(). |
| **auto_feature_registry** | Extra feature IDs and aliases; same SharedFeatureRegistry. |
| **Feature manifest**   | Status per variant (Real/Partial/Missing); update when behavior changes. |

To add a new command: (1) Add one line to COMMAND_TABLE with handler. (2) Implement handler in feature_handlers or ssot_handlers. (3) If you need an extra alias, add autoReg in initAutoFeatureRegistry(). (4) Update manifest if the feature gains new behavior in a variant.
