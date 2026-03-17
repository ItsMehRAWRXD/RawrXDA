"""Minimal Python helper for the enhanced assembly Ollama wrapper.

This bridges to `ollama_request_ext` exported by `ollama_wrapper.o` (when linked
into a shared library) and provides:
  * Safe JSON assembly
  * Dynamic model name passing
  * Execution + output file parsing (/tmp/ollama_out.json)

Build (Linux):
    nasm -felf64 src/ollama_wrapper.asm -o build/ollama_wrapper.o
    gcc -shared -o lib/libollama_wrapper.so build/ollama_wrapper.o

Usage:
    from ollama_wrapper_helper import ollama_generate
    result = ollama_generate({"prompt": "Hello"}, model="llama2")
    print(result["response"])

Return codes from assembly:
    >=0 : system() exit code (success path; file attempted removed)
    -1  : fopen failed
    -2  : fwrite incomplete
    -3  : fclose failed
    -4  : model length too large
    -5  : command assembly overflow

If native library not found, falls back to invoking curl directly
(with same semantics) but still parses the output file.
"""
from __future__ import annotations
import ctypes
import json
import os
import subprocess
from typing import Dict, Any, Optional, Tuple

LIB_CANDIDATES = [
    os.path.join("lib", "libollama_wrapper.so"),
    os.path.join("lib", "ollama_wrapper.dll"),
    os.path.join("build", "libollama_wrapper.so"),
]

_OUT_PATH = "/tmp/ollama_out.json"

class _AsmWrapper:
    def __init__(self) -> None:
        self.lib = None
        for cand in LIB_CANDIDATES:
            if os.path.exists(cand):
                try:
                    self.lib = ctypes.CDLL(cand)
                    break
                except OSError:
                    pass
        if self.lib is None:
            print("[ollama_wrapper_helper] Native library not found; using curl subprocess fallback.")
            return
        # int ollama_request_ext(const char *json_ptr,size_t json_len,const char *model_ptr,size_t model_len)
        self.lib.ollama_request_ext.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.c_char_p, ctypes.c_size_t]
        self.lib.ollama_request_ext.restype = ctypes.c_int

    def request_ext(self, json_bytes: bytes, model_bytes: bytes) -> int:
        if self.lib is None:
            return self._fallback(json_bytes, model_bytes)
        return self.lib.ollama_request_ext(json_bytes, len(json_bytes), model_bytes, len(model_bytes))

    def _fallback(self, json_bytes: bytes, model_bytes: bytes) -> int:
        model = model_bytes.decode('utf-8')
        # Build command matching assembly logic
        cmd = [
            "/usr/bin/curl", "-s", "-X", "POST",
            f"http://127.0.0.1:11434/v1/complete?model={model}",
            "-H", "Content-Type: application/json",
            "--data-binary", "@-",
        ]
        try:
            completed = subprocess.run(cmd, input=json_bytes, capture_output=True, check=False)
            # write output to /tmp/ollama_out.json for parity
            with open(_OUT_PATH, "wb") as f:
                f.write(completed.stdout)
            return completed.returncode
        except Exception as e:
            print("[ollama_wrapper_helper] Fallback error:", e)
            return -99

_wrapper = _AsmWrapper()

def _build_json(payload: Dict[str, Any]) -> bytes:
    # Ensure stable minimal JSON encoding
    return json.dumps(payload, separators=(",", ":"), ensure_ascii=False).encode('utf-8')

def _read_output() -> Tuple[Optional[Dict[str, Any]], Optional[str]]:
    if not os.path.exists(_OUT_PATH):
        return None, None
    raw = None
    try:
        with open(_OUT_PATH, "rb") as f:
            data = f.read()
        raw = data.decode('utf-8', errors='replace')
        try:
            return json.loads(raw), raw
        except json.JSONDecodeError:
            return None, raw
    finally:
        # best-effort cleanup
        try:
            os.remove(_OUT_PATH)
        except OSError:
            pass

def ollama_generate(payload: Dict[str, Any], model: str = "local") -> Dict[str, Any]:
    """Generate a completion via assembly wrapper.

    Args:
        payload: Dict containing at least a 'prompt' key.
        model: Model name string (no whitespace or shell metacharacters recommended).
    Returns:
        Dict with keys: 'ok', 'exit_code', 'response', 'raw', 'error' (optional)
    """
    if 'prompt' not in payload:
        return {'ok': False, 'exit_code': -10, 'error': 'missing prompt'}

    # Build JSON (the real Ollama /v1/complete expects a JSON body; here we pass payload directly)
    json_bytes = _build_json(payload)
    model_bytes = model.encode('utf-8')

    code = _wrapper.request_ext(json_bytes, model_bytes)
    parsed, raw = _read_output()

    # Heuristic: attempt to extract 'response' field if JSON parsed
    response_text = None
    if parsed and isinstance(parsed, dict):
        response_text = parsed.get('response') or parsed.get('completion') or raw
    else:
        response_text = raw

    return {
        'ok': code >= 0,
        'exit_code': code,
        'response': response_text,
        'raw': raw,
        'error': None if code >= 0 else f'assembly exit code {code}'
    }

if __name__ == '__main__':
    import sys
    prompt = ' '.join(sys.argv[1:]) or 'Write a short poem about NASM.'
    result = ollama_generate({'prompt': prompt}, model='llama2')
    print(json.dumps(result, indent=2))
