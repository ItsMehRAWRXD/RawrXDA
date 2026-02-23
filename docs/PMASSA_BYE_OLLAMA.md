# PMASSA — Bye Ollama

**PMASSA** = direction to **phase out Ollama** as a required or preferred backend. Local inference and other backends take precedence.

---

## Intent

- **Bye Ollama:** Ollama (HTTP API at `localhost:11434`) is no longer the default or required path for local AI in the IDE.
- **Preferred:** Local GGUF (in-process or via RawrXD inference engine), model router, and other backends.
- **Ollama code:** Remains in tree for compatibility (settings, GhostText fallback, model-from-Ollama pull). It can be disabled via build option **`RAWRXD_PMASSA=ON`** when that is fully wired (compile definition `RAWRXD_PMASSA`).

---

## Build (optional)

To build in PMASSA mode (no Ollama dependency in behavior; future: exclude Ollama objects):

```bat
cmake -B build_ide -G Ninja -DCMAKE_BUILD_TYPE=Release -DRAWRXD_PMASSA=ON
cmake --build build_ide --target RawrXD-Win32IDE
```

When `RAWRXD_PMASSA=ON`, the define **`RAWRXD_PMASSA`** is set so code can `#ifndef RAWRXD_PMASSA` around Ollama-specific paths if desired.

---

## What stays

- Local GGUF load and run (Win32 IDE, inference engine).
- Model router (local file, HuggingFace, HTTP).
- GhostText / FIM: primary path = local completion server or built-in; Ollama as optional fallback when not in PMASSA build.
- Other backends (e.g. cloud) unchanged.

---

## Summary

**PMASSA = bye Ollama** — Ollama is deprecated as the default; use local GGUF and the inference stack instead. Ollama support remains compile-time optional via `RAWRXD_PMASSA`.
