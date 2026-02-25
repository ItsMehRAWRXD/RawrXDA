#!/usr/bin/env python3
"""
RawrEngine entry point — delegates to Ship/RawrEngine.py
=========================================================
This module exists so that launcher scripts referencing
``backend/rawr_engine.py`` continue to work.  The authoritative
implementation lives in ``Ship/RawrEngine.py``.
"""

import os
import sys

# Ensure the repo root is on sys.path so ``Ship.RawrEngine`` resolves
_repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
if _repo_root not in sys.path:
    sys.path.insert(0, _repo_root)

# Also add Ship/ directly for ``import RawrEngine``
_ship_dir = os.path.join(_repo_root, "Ship")
if _ship_dir not in sys.path:
    sys.path.insert(0, _ship_dir)

from RawrEngine import main  # noqa: E402

if __name__ == "__main__":
    raise SystemExit(main())
