# RE MVP: symbols + xrefs shell (E10)

**Done when:** Dockable flow: **load artifact** → **symbol list** → **xref list** (read-only v1 acceptable).

## v1 scope

- Input: path to **PE / ELF / blob** (or sovereign asm artifact from Codex pipeline).
- Output: table views backed by a **symbol provider** (see E11).

## Repo leverage

- **`docs/SOVEREIGN_PRODUCTION_SCOPE_AND_ROADMAP.md`** — RE roadmap.
- **Asm / Codex outputs** — ingest as “artifact” without requiring full PDB server in v1.

## UI placement

- Win32: new dock or **View → Reverse engineering** (parity with strategic pillars).
- Electron: optional **Modules → RE (preview)** when `modules.reTools` enabled (future).
