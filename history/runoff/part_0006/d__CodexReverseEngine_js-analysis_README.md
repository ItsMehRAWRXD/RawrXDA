# CodexReverse JS Analysis

Beautifies JS bundles, extracts normalized functions/classes/exports, builds a call graph for MCP entrypoints, and writes a clean API map.

## Install

```powershell
cd D:\CodexReverseEngine\js-analysis
npm install
```

## Run (single file or directory)

```powershell
node scripts\analyze.js --inputs D:\Cursor_Critical_Source\cursor-agent\dist\main.js --out D:\Cursor_Analysis_Results\agent
node scripts\analyze.js --inputs D:\Cursor_Critical_Source\cursor-mcp\dist\main.js --out D:\Cursor_Analysis_Results\mcp
```

## Run on both bundles at once

```powershell
node scripts\analyze.js --inputs D:\Cursor_Critical_Source\cursor-agent\dist\main.js,D:\Cursor_Critical_Source\cursor-mcp\dist\main.js --out D:\Cursor_Analysis_Results\combined
```

Beautified output is stored under:

- `D:\Cursor_Analysis_Results\combined\beautified\cursor-agent_dist\main.js`
- `D:\Cursor_Analysis_Results\combined\beautified\cursor-mcp_dist\main.js`

## Outputs

- `functions.txt` / `classes.txt` / `exports.txt` / `imports.txt`
- `urls.txt`
- `mcp_patterns.txt` / `mcp_entrypoints.txt`
- `call_graph.json`
- `api_map.json`
- `summary.json`
- `errors.json` (only if parse/beautify errors occur)

## MCP entrypoint locations

```powershell
node scripts\mcp_locate.js D:\Cursor_Analysis_Results\combined

# Optional: custom beautified directory
node scripts\mcp_locate.js D:\Cursor_Analysis_Results\combined --beautified D:\Cursor_Analysis_Results\combined\beautified
```

Outputs:

- `mcp_entrypoint_locations.json`
- `mcp_entrypoint_locations.txt`

## MCP transport ranking

Ranks entrypoint locations by nearby transport keywords (connect, http, ws, upgrade, etc.).

```powershell
node scripts\mcp_rank.js D:\Cursor_Analysis_Results\combined
```

Outputs:

- `mcp_entrypoint_ranked.json`
- `mcp_entrypoint_ranked.txt`

## MCP top-10 snippets + hooks

Generates the top-10 transport candidates with byte offsets, pseudocode, and MASM hook templates.

```powershell
node scripts\mcp_snippets.js D:\Cursor_Analysis_Results\combined
```

Outputs:

- `mcp_entrypoint_snippets.json`
- `mcp_entrypoint_snippets.txt`

## Quick test

```powershell
npm test
```
