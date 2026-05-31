from __future__ import annotations

import argparse
import json
import os
from typing import Dict

from .ide_controller import IdeController, IdeRunRequest

def _parse_model_overrides(values: list[str]) -> Dict[int, str]:
    out: Dict[int, str] = {}
    for v in values:
        idx_text, model = v.split(":", 1)
        out[int(idx_text.strip())] = model.strip()
    return out


def _parse_role_models(values: list[str]) -> Dict[str, str]:
    out: Dict[str, str] = {}
    for v in values:
        role, model = v.split(":", 1)
        out[role.strip().lower()] = model.strip()
    return out


def _parse_pane_models(values: list[str]) -> Dict[str, str]:
    out: Dict[str, str] = {}
    for v in values:
        pane, model = v.split(":", 1)
        out[pane.strip()] = model.strip()
    return out


def main() -> None:
    parser = argparse.ArgumentParser(description="SecFlow cybersecurity workflow automation")
    parser.add_argument("--workflow", required=True, help="Path to workflow JSON")
    parser.add_argument("--incident", help="Path to incident JSON")
    parser.add_argument("--operator", default="soc-automation", help="Operator identity")
    parser.add_argument("--approve", action="append", default=[], help="Approved step id (repeatable)")
    parser.add_argument("--dry-run", action="store_true", help="Do not enforce high-impact actions")
    parser.add_argument("--repo-root", default=os.getcwd(), help="Repository root used for workflow path boundary checks")
    parser.add_argument("--swarm-size", type=int, default=8, help="Number of swarm agents (1-64)")
    parser.add_argument("--default-model", default="phi3-mini-q4.gguf", help="Default model for swarm agents")
    parser.add_argument(
        "--swarm-model",
        action="append",
        default=[],
        help="Per-agent model override in format idx:model (repeatable, e.g. --swarm-model 2:codestral22b.gguf)",
    )
    parser.add_argument(
        "--role-model",
        action="append",
        default=[],
        help="Per-role model override in format role:model (ask/plan/agent/support)",
    )
    parser.add_argument(
        "--pane-model",
        action="append",
        default=[],
        help="Per-pane model assignment in format pane:model (repeatable)",
    )
    parser.add_argument("--drow", default="default-drow", help="Per-project Drow state key")
    parser.add_argument("--serial-swarm", action="store_true", help="Run swarm serially instead of parallel")
    parser.add_argument(
        "--share-source",
        action="append",
        default=[],
        help="Screen-share source selection (ide or desktop), max two",
    )
    parser.add_argument("--pretty", action="store_true", help="Pretty JSON output")
    args = parser.parse_args()

    controller = IdeController(args.repo_root)
    req = IdeRunRequest(
        workflow_path=args.workflow,
        incident_path=args.incident,
        operator=args.operator,
        approvals=args.approve,
        dry_run=args.dry_run,
        swarm_size=args.swarm_size,
        model_by_agent=_parse_model_overrides(args.swarm_model),
        default_model=args.default_model,
        role_models=_parse_role_models(args.role_model),
        pane_models=_parse_pane_models(args.pane_model),
        drow_name=args.drow,
        parallel_swarm=not args.serial_swarm,
        share_sources=args.share_source,
    )
    payload = controller.run(req)

    if args.pretty:
        print(json.dumps(payload, indent=2))
    else:
        print(json.dumps(payload))


if __name__ == "__main__":
    main()
