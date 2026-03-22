# WebView2 / Chromium stderr: ICU & Crashpad messages

## What you are seeing

Lines like:

```text
[0217/015919.070:ERROR:base\i18n\icu_util.cc:223] Invalid file descriptor to ICU data received.
[0318/234207.467:ERROR:third_party\crashpad\...\registration_protocol_win.cc:108] CreateFile: The system cannot find the file specified. (0x2)
```

come from **Chromium’s** logging (the same engine stack used by **Microsoft Edge WebView2** and **Electron** apps). They are **not** emitted by RawrXD’s GGUF runner, Logger, or TpsSmoke.

| Message | Meaning (high level) |
|--------|----------------------|
| **`icu_util.cc` / ICU data** | The embedded browser could not initialize **ICU** (locale/unicode) data the way it expects (bad/missing path, sandbox, or stale user-data dir). Often **non-fatal** if the surface still renders. |
| **`crashpad` / `CreateFile` 0x2** | Chromium’s **Crashpad** helper tries to open a **named pipe** or crash-report endpoint; if nothing is listening, policy blocks it, or the profile path is wrong, you get **“file not found.”** Usually **does not** break normal browsing/editor use. |

## Is it RawrXD?

- **RawrXD Win32 IDE** paths that use WebView2: Monaco editor host, agentic browser layer (`docs/AGENTIC_BROWSER_LAYER.md`). If stderr is captured from the IDE process, these lines can appear when WebView2 spins up.
- The same log shape appears in **Cursor**, **VS Code** (some builds), **Edge**, and other **Electron/WebView2** tools. Check **which executable** owns the console/log file.

## What to do (Windows)

1. **Install or repair WebView2 Runtime**  
   Evergreen runtime: [Download WebView2](https://developer.microsoft.com/en-us/microsoft-edge/webview2/) — use **Evergreen Bootstrapper** or **Standalone Installer** if the IDE/browser surface is blank or unstable.

2. **Writable user-data directory**  
   If you override WebView2 user data folder in code or env, ensure the path **exists** and the process **can write** there (not Program Files, not a deleted temp path).

3. **Corporate / security software**  
   Some policies block Crashpad pipes or lock profile folders; ICU warnings can follow. If the UI works, logs are often **safe to ignore**.

4. **Reduce noise when debugging RawrXD**  
   Filter stderr for your own tags (`[GGUFRunner]`, `TpsSmoke`, `Logger`) or ignore lines containing `icu_util`, `crashpad`, `third_party\`.

5. **If the WebView is actually broken**  
   See also `docs/VSCODE_BLANK_WINDOW_FIX.md` (GPU/user-data workarounds) and `docs/SOVEREIGN_GUIDE.md` (VC++ runtime + WebView2 prerequisite note).

## Summary

These messages are **Chromium infrastructure noise** during WebView2/Electron startup or crash reporting setup. They are **unrelated** to GGUF tensor offsets or mmap. Fix them only if the **embedded browser fails**; otherwise repair WebView2 runtime and profile paths, then treat as benign.
