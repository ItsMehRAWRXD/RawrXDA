from __future__ import annotations

import os
import re
from typing import Dict, List


_MODEL_PATTERN = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._\-/]{0,127}$")


class SecurityValidationError(ValueError):
    pass


def validate_swarm_size(size: int) -> int:
    if size < 1 or size > 64:
        raise SecurityValidationError("swarm size must be between 1 and 64")
    return size


def validate_model_name(name: str) -> str:
    if not name or not isinstance(name, str):
        raise SecurityValidationError("model name must be a non-empty string")
    if len(name) > 128:
        raise SecurityValidationError("model name too long")
    if ".." in name or name.startswith("/") or name.startswith("\\"):
        raise SecurityValidationError("model name/path traversal is not allowed")
    if not _MODEL_PATTERN.match(name):
        raise SecurityValidationError("model name contains invalid characters")
    return name


def validate_model_map(models: Dict[int, str], swarm_size: int) -> Dict[int, str]:
    validated: Dict[int, str] = {}
    for idx, model in models.items():
        if idx < 1 or idx > swarm_size:
            raise SecurityValidationError(f"model index {idx} out of range for swarm size {swarm_size}")
        validated[idx] = validate_model_name(model)
    return validated


def normalize_swarm_models(swarm_size: int, models: Dict[int, str], default_model: str) -> List[str]:
    validate_swarm_size(swarm_size)
    default_valid = validate_model_name(default_model)
    validated_map = validate_model_map(models, swarm_size)
    out = []
    for i in range(1, swarm_size + 1):
        out.append(validated_map.get(i, default_valid))
    return out


def validate_workflow_path(path: str, repo_root: str) -> str:
    if not path:
        raise SecurityValidationError("workflow path is required")
    abs_root = os.path.abspath(repo_root)
    abs_path = os.path.abspath(path)
    if not abs_path.startswith(abs_root):
        raise SecurityValidationError("workflow path must stay inside repository root")
    if not abs_path.lower().endswith(".json"):
        raise SecurityValidationError("workflow must be a .json file")
    return abs_path


def validate_role_models(role_models: Dict[str, str]) -> Dict[str, str]:
    allowed_roles = {"ask", "plan", "agent", "support"}
    out: Dict[str, str] = {}
    for role, model in role_models.items():
        role_l = str(role).strip().lower()
        if role_l not in allowed_roles:
            raise SecurityValidationError(f"invalid role: {role}")
        out[role_l] = validate_model_name(model)
    return out


def validate_pane_models(pane_models: Dict[str, str]) -> Dict[str, str]:
    out: Dict[str, str] = {}
    for pane_id, model in pane_models.items():
        pid = str(pane_id).strip()
        if not pid or len(pid) > 64:
            raise SecurityValidationError("pane id must be 1-64 chars")
        if any(ch in pid for ch in ["..", "/", "\\"]):
            raise SecurityValidationError("pane id contains invalid path-like characters")
        out[pid] = validate_model_name(model)
    return out


def validate_drow_name(name: str) -> str:
    if not name or not isinstance(name, str):
        raise SecurityValidationError("drow name must be a non-empty string")
    val = name.strip()
    if len(val) < 2 or len(val) > 64:
        raise SecurityValidationError("drow name length must be 2-64")
    if not re.match(r"^[A-Za-z0-9][A-Za-z0-9._-]{1,63}$", val):
        raise SecurityValidationError("drow name contains invalid characters")
    return val


def validate_share_sources(sources: List[str]) -> List[str]:
    allowed = {"ide", "desktop"}
    if len(sources) > 2:
        raise SecurityValidationError("at most two share sources are allowed")
    out: List[str] = []
    for src in sources:
        s = str(src).strip().lower()
        if s not in allowed:
            raise SecurityValidationError(f"invalid share source: {src}")
        out.append(s)
    return out
