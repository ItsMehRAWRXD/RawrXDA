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
