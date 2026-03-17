"""Safe model introspection and registry.

This module collects non-invasive metadata about model files and stores
reports in a local sqlite registry. It deliberately avoids reading or
modifying tensor bytes or providing any weight-editing functionality.
"""
from __future__ import annotations
import os
import json
import hashlib
import sqlite3
import time
from typing import Dict, Any, List

DB_SCHEMA = """
CREATE TABLE IF NOT EXISTS reports (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    model_path TEXT NOT NULL,
    report_json TEXT NOT NULL,
    created_at REAL NOT NULL
);
"""


def init_db(db_path: str = "model_registry.db") -> None:
    con = sqlite3.connect(db_path)
    cur = con.cursor()
    cur.executescript(DB_SCHEMA)
    con.commit()
    con.close()


def _sha256_of_file(path: str, chunk_size: int = 4 * 1024 * 1024) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while True:
            chunk = f.read(chunk_size)
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest()


def _first_n_hex(path: str, n: int = 84) -> str:
    with open(path, "rb") as f:
        data = f.read(n)
    return data.hex()


def _extract_readable_strings(path: str, min_len: int = 4) -> List[str]:
    out: List[str] = []
    try:
        with open(path, "rb") as f:
            data = f.read()
    except Exception:
        return out
    cur = []
    for b in data:
        if 32 <= b < 127:
            cur.append(chr(b))
        else:
            if len(cur) >= min_len:
                out.append("".join(cur))
            cur = []
    if len(cur) >= min_len:
        out.append("".join(cur))
    return out


def _load_json_if_exists(base: str, names=("manifest.json", "config.json", "tokenizer.json")) -> Dict[str, Any]:
    out = {}
    for name in names:
        p = os.path.join(base, name) if os.path.isdir(base) else (base if os.path.basename(base) == name else None)
        if p and os.path.exists(p):
            try:
                with open(p, "r", encoding="utf-8") as fh:
                    out[name] = json.load(fh)
            except Exception:
                out[name] = None
    return out


def introspect_model(path: str, register: bool = False, db_path: str = "model_registry.db", dry_run: bool = False) -> Dict[str, Any]:
    """Create a safe introspection report for a model file or directory.

    - For directories: walks files and collects filename, size, sha256, first84 hex, and readable strings sample.
    - For single files: returns metadata for that file.
    """
    init_db(db_path)
    report: Dict[str, Any] = {
        "path": os.path.abspath(path),
        "is_dir": os.path.isdir(path),
        "files": [],
        "loaded_json": {},
        "created_at": time.time(),
    }

    targets: List[str] = []
    if os.path.isdir(path):
        for root, _, files in os.walk(path):
            for fn in files:
                targets.append(os.path.join(root, fn))
    elif os.path.exists(path):
        targets = [path]
    else:
        raise FileNotFoundError(path)

    for p in targets:
        try:
            size = os.path.getsize(p)
            sha = _sha256_of_file(p)
            first84 = _first_n_hex(p, 84)
            strings = _extract_readable_strings(p, min_len=6)[:10]
            report["files"].append({
                "relpath": os.path.relpath(p, start=path) if os.path.isdir(path) else os.path.basename(p),
                "abs_path": os.path.abspath(p),
                "size": size,
                "sha256": sha,
                "first84_hex": first84,
                "strings_sample": strings,
            })
        except Exception as e:
            report["files"].append({"abs_path": p, "error": str(e)})

    # try to load manifest/config/tokenizer if present at root
    if os.path.isdir(path):
        report["loaded_json"] = _load_json_if_exists(path)
    else:
        report["loaded_json"] = _load_json_if_exists(path)

    if register and not dry_run:
        con = sqlite3.connect(db_path)
        cur = con.cursor()
        cur.execute("INSERT INTO reports (model_path, report_json, created_at) VALUES (?, ?, ?)",
                    (report["path"], json.dumps(report), report["created_at"]))
        con.commit()
        con.close()

    return report


def list_reports(db_path: str = "model_registry.db", limit: int = 20) -> List[Dict[str, Any]]:
    init_db(db_path)
    con = sqlite3.connect(db_path)
    cur = con.cursor()
    cur.execute("SELECT id, model_path, created_at FROM reports ORDER BY created_at DESC LIMIT ?", (limit,))
    rows = cur.fetchall()
    con.close()
    return [{"id": r[0], "model_path": r[1], "created_at": r[2]} for r in rows]


def get_report(id: int, db_path: str = "model_registry.db") -> Dict[str, Any]:
    init_db(db_path)
    con = sqlite3.connect(db_path)
    cur = con.cursor()
    cur.execute("SELECT report_json FROM reports WHERE id = ?", (id,))
    row = cur.fetchone()
    con.close()
    if not row:
        raise KeyError(id)
    return json.loads(row[0])


def clone_model(src: str, dst: str, register: bool = True, db_path: str = "model_registry.db") -> str:
    import shutil
    src = os.path.abspath(src)
    dst = os.path.abspath(dst)
    if os.path.isdir(src):
        shutil.copytree(src, dst)
    else:
        # ensure dst dir
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy2(src, dst)
    if register:
        introspect_model(dst, register=True, db_path=db_path)
    return dst


def apply_overlay(model_path: str, overlay: Dict[str, Any], register: bool = True, db_path: str = "model_registry.db") -> str:
    """Write a JSON overlay next to the model (not modifying binaries). Returns path to overlay file."""
    base = os.path.abspath(model_path)
    if os.path.isdir(base):
        out = os.path.join(base, "runtime_overlay.json")
    else:
        out = base + ".runtime_overlay.json"
    with open(out, "w", encoding="utf-8") as fh:
        json.dump(overlay, fh, indent=2)
    if register:
        introspect_model(base, register=True, db_path=db_path)
    return out


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("path")
    parser.add_argument("--register", action="store_true")
    args = parser.parse_args()
    rpt = introspect_model(args.path, register=args.register)
    print(json.dumps(rpt, indent=2))
