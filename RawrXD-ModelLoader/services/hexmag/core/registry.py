from __future__ import annotations

import importlib
import pkgutil
from typing import List

from core.contracts import Bot


def load_bots() -> List[Bot]:
    """Dynamically load Bot implementations located under the bots/ package."""
    bots: List[Bot] = []
    try:
        iterator = pkgutil.iter_modules(["bots"])
    except FileNotFoundError:
        return bots

    for module_info in iterator:
        if module_info.name.startswith("_"):
            continue
        try:
            module = importlib.import_module(f"bots.{module_info.name}")
        except Exception:
            continue

        for attr_name in dir(module):
            obj = getattr(module, attr_name)
            try:
                if isinstance(obj, type) and issubclass(obj, Bot) and obj is not Bot:
                    bots.append(obj())
                    break
            except Exception:
                continue
    return bots
