# RawrXD Copilot MASM (Batches 1–2)

This folder contains the MASM x64 implementation scaffolding for a GitHub Copilot HTTPS client with OAuth device flow, JWT handling, streaming UI, and a tool-calling framework.

Important notes:
- WinHTTP APIs are Unicode (wide) in practice; the sample sources currently use ANSI for clarity and may require `W` variants or wide strings to build.
- Several helper routines are referenced but not yet provided (e.g., `JsonExtractString`, `JsonExtractObject`, `HttpPostJson`, `Vector_Push`, `ToolRegistry_Find`, `CopilotHttp_GetGithubPublicKey`, etc.). These will be added in later batches.
- MASM doesn’t ship Windows API .inc prototypes by default; you will either need compatible include files (e.g., from MASM32/UASM) or declare externals manually.

## Layout
- Batch 1
  - `copilot_http_client.asm`
  - `copilot_auth.asm`
  - `copilot_chat_protocol.asm`
- Batch 2
  - `copilot_token_parser.asm`
  - `chat_stream_ui.asm`
  - `tool_integration.asm`

## Prerequisites
- Visual Studio with Desktop development with C++ workload (for `ml64.exe`, `link.exe`, and Windows SDK libs).
- A Developer Command Prompt (or pre-initialized environment with `vcvars64.bat`).
- Windows SDK libraries (`winhttp.lib`, `bcrypt.lib`, `crypt32.lib`).

## Quick Build (manual)
Run from a VS 64-bit Developer Command Prompt:

```
ml64 /c /Fo copilot_http_client.obj copilot_http_client.asm
ml64 /c /Fo copilot_auth.obj copilot_auth.asm
ml64 /c /Fo copilot_chat_protocol.obj copilot_chat_protocol.asm
ml64 /c /Fo copilot_token_parser.obj copilot_token_parser.asm
ml64 /c /Fo chat_stream_ui.obj chat_stream_ui.asm
ml64 /c /Fo tool_integration.obj tool_integration.asm
link /OUT:RawrXD-Copilot-MASM.exe /SUBSYSTEM:CONSOLE *.obj winhttp.lib crypt32.lib bcrypt.lib
```

Note: You may need to adjust includes, convert strings to wide (`L"..."`) and call `WinHttp*` wide forms, or add missing helper modules as they arrive in Batches 3+.

## Next Steps
- Integrate Batch 3 sources (`agentic_loop`, `cursor_cmdk`, `diff_engine`).
- Add missing helpers (JSON parsing, HTTP helpers, registry/data structures, RSA key import helper).
- Convert WinHTTP call-sites to wide strings and verify headers and request composition.
- Add a minimal `main.asm` harness for device flow and a chat round-trip once token acquisition is complete.

## Batch 3: Agentic Loop, Command Palette, Diff Viewer (Added)

- `agentic_loop.asm` - Agent planning and execution loop. Uses `ModelRouter_CallModel` to generate plans and executes tool-callable steps.
- `cursor_cmdk.asm` - Command palette (Cmd-K style) with 50+ commands; added commands to toggle thinking UI and set agent modes.
- `diff_engine.asm` - Inline diff viewer to accept or reject suggested edits.

## Thinking UI & Agent Modes

We added a standardized "thinking" UI and a modes system so any model can be used consistently:

- Thinking UI: a code-style box (monospace font) for model "thinking" output and an adjacent message area styled with a white background, black UTF-8-capable font, and a grey border. Implemented in `chat_stream_ui.asm`.

UI style details (as requested):
- Thinking box: a monospace codebox (Consolas), white background, black text, with a grey client-edge border to visually separate it from the chat area. Designed for pre-response "thinking" output (streamed tokens shown here while the model computes).
- Final message area: embedded area with a subtle grey border, white background, and black UTF-8-capable font (RichEdit/Unicode recommended for full UTF-8 support).

Note: The current MASM samples use ANSI controls for brevity; a production build should switch to RichEdit / wide-character APIs for proper UTF-8 rendering and use owner-draw or theming to precisely match the visuals.
- Modes: bit-flag modes for agent behavior (Max, Search-Web, Turbo, Auto-Instant, Legacy, Thinking standard). Exposed via `model_router.asm` functions and Cmd-K commands.
- Cmd-K: The Command Palette (`cursor_cmdk.asm`) includes quick toggles for **Thinking UI** and agent modes (Max, Search-Web, Turbo, Auto-Instant, Legacy). Use Cmd-K then type the command name.
- Model fallback: Model router uses a primary model and will perform a single fallback call on error (no stacked responses).

See `model_router.asm` for the stub implementation and usage notes. These are initial, production-oriented stubs; real deployment requires completing JSON helpers, wide-char conversion, and robust error handling.
