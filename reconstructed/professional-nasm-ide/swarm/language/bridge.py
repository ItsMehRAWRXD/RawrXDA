"""Minimal Python bridge for interacting with NASM-based core and extensions.

This module intentionally stays small and pragmatic: it provides helpers
to assemble and run NASM sources (if toolchain available), invoke extension
handlers (assembly or script), and a small adapter to the model_registry
overlay function so the swarm can call into the ASM core and manage overlays.

Keep it minimal — no weight-level or unsafe features here.
"""
from __future__ import annotations
import os
import shutil
import subprocess
import tempfile
from typing import Optional, Sequence, Dict, Any

from model_registry import apply_overlay


def _which(exe: str) -> Optional[str]:
    return shutil.which(exe)


def assemble_nasm(asm_path: str, out_exe: Optional[str] = None) -> str:
    """Assemble an x86 NASM file into an executable (Linux/Windows toolchain permitting).

    Returns path to output binary. If toolchain not available, raises RuntimeError.
    This helper is intentionally a thin wrapper around system tools.
    """
    if not os.path.exists(asm_path):
        raise FileNotFoundError(asm_path)

    nasm = _which("nasm")
    ld = _which("ld") or _which("gcc")
    if nasm is None or ld is None:
        raise RuntimeError("nasm or linker not found in PATH")

    out_dir = tempfile.mkdtemp(prefix="nasm_build_")
    obj_path = os.path.join(out_dir, "out.o")
    if out_exe is None:
        out_exe = os.path.join(out_dir, "out.bin")

    # Assemble to object
    subprocess.check_call([nasm, "-felf64", asm_path, "-o", obj_path])
    # Link to executable
    if _which("gcc"):
        subprocess.check_call(["gcc", obj_path, "-o", out_exe])
    else:
        subprocess.check_call([ld, obj_path, "-o", out_exe])

    return out_exe


def run_binary(bin_path: str, args: Optional[Sequence[str]] = None, timeout: Optional[int] = None) -> Dict[str, Any]:
    """Run a binary and capture stdout/stderr.

    Returns a dict with returncode, stdout, stderr.
    """
    if args is None:
        args = []
    proc = subprocess.Popen([bin_path, *args], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        out, err = proc.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        proc.kill()
        out, err = proc.communicate()
        return {"returncode": proc.returncode, "stdout": out.decode(errors="ignore"), "stderr": err.decode(errors="ignore"), "timeout": True}
    return {"returncode": proc.returncode, "stdout": out.decode(errors="ignore"), "stderr": err.decode(errors="ignore"), "timeout": False}


def invoke_extension(extension_path: str, payload: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
    """Invoke an extension which can be an executable, script, or ASM source.

    - If extension_path ends with .asm, try to assemble and run it (toolchain required).
    - If it is a .py, run it with the same Python interpreter and send payload via stdin (JSON).
    - Otherwise, if executable, run it.

    Returns dict with returncode, stdout, stderr.
    """
    ext = extension_path
    if not os.path.exists(ext):
        raise FileNotFoundError(ext)

    if ext.endswith(".asm"):
        bin_path = assemble_nasm(ext)
        return run_binary(bin_path)
    elif ext.endswith(".py"):
        import json, sys
        p = subprocess.Popen([sys.executable, ext], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        inp = json.dumps(payload or {}).encode("utf-8")
        out, err = p.communicate(input=inp)
        return {"returncode": p.returncode, "stdout": out.decode(errors="ignore"), "stderr": err.decode(errors="ignore")}
    else:
        # try to execute directly
        return run_binary(ext)


def apply_runtime_overlay(model_path: str, overlay: Dict[str, Any], register: bool = True) -> str:
    """Convenience wrapper that writes a runtime overlay JSON next to the model.

    This does NOT modify binaries. It uses `model_registry.apply_overlay` under the hood.
    """
    return apply_overlay(model_path, overlay, register=register)


__all__ = ["assemble_nasm", "run_binary", "invoke_extension", "apply_runtime_overlay"]
