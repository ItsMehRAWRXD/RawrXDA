from __future__ import annotations

import json
import os
from dataclasses import dataclass
from typing import Any, Dict, List

from .engine import WorkflowEngine
from .ide_runtime import CollaborationRuntime, DrowStateStore, SessionPool, SwarmRuntime, ChatPaneManager
from .models import ExecutionContext
from .security import (
    normalize_swarm_models,
    validate_drow_name,
    validate_pane_models,
    validate_role_models,
    validate_share_sources,
    validate_swarm_size,
    validate_workflow_path,
)


@dataclass
class IdeRunRequest:
    workflow_path: str
    incident_path: str | None
    operator: str
    approvals: List[str]
    dry_run: bool
    swarm_size: int
    model_by_agent: Dict[int, str]
    default_model: str
    role_models: Dict[str, str]
    pane_models: Dict[str, str]
    drow_name: str
    parallel_swarm: bool
    share_sources: List[str]


class IdeController:
    def __init__(self, repo_root: str) -> None:
        self.repo_root = os.path.abspath(repo_root)
        self.engine = WorkflowEngine()
        self.session_pool = SessionPool()
        self.swarm = SwarmRuntime(self.session_pool)
        self.panes = ChatPaneManager(self.session_pool)
        self.collab = CollaborationRuntime()
        self.drow_store = DrowStateStore(self.repo_root)

    def run(self, req: IdeRunRequest) -> Dict[str, Any]:
        workflow_path = validate_workflow_path(req.workflow_path, self.repo_root)
        swarm_size = validate_swarm_size(req.swarm_size)
        models = normalize_swarm_models(swarm_size, req.model_by_agent, req.default_model)
        role_models = validate_role_models(req.role_models)
        pane_models = validate_pane_models(req.pane_models)
        drow_name = validate_drow_name(req.drow_name)
        share_sources = validate_share_sources(req.share_sources)

        workflow = self.engine.load_workflow(workflow_path)
        incident = self._load_incident(req.incident_path)
        incident["swarm"] = {
            "size": swarm_size,
            "models": models,
            "roles": role_models,
        }
        incident["drow"] = drow_name
        incident["share_sources"] = share_sources

        ctx = ExecutionContext(
            operator=req.operator,
            incident=incident,
            approvals=req.approvals,
            dry_run=req.dry_run,
        )
        ctx.vars["swarm"] = {
            "size": swarm_size,
            "models": models,
            "roles": role_models,
        }
        ctx.vars["drow"] = drow_name

        report = self.engine.execute(workflow, ctx)
        payload = self.engine.report_to_dict(report)

        swarm_result = self.swarm.run(
            swarm_size=swarm_size,
            default_model=req.default_model,
            model_by_agent=req.model_by_agent,
            role_models=role_models,
            drow_name=drow_name,
            parallel=req.parallel_swarm,
        )
        drow_state = self.drow_store.save_result(drow_name, swarm_result)

        pane_results: Dict[str, Any] = {}
        for pane_id, model in pane_models.items():
            self.panes.open_pane(pane_id, model, allow_share_same_model=True)
            pane_results[pane_id] = self.panes.infer(pane_id, f"drow:{drow_name};pane:{pane_id}")

        share_results = []
        for idx, src in enumerate(share_sources, start=1):
            share = self.collab.start_share(src, f"share-{idx}")
            share_results.append({
                "id": share.id,
                "source": share.source,
                "label": share.label,
                "active": share.active,
                "started_utc": share.started_utc,
            })

        payload["swarm"] = {
            "size": swarm_size,
            "models": models,
            "role_models": role_models,
        }
        payload["parallel_swarm"] = req.parallel_swarm
        payload["drow"] = {
            "name": drow_name,
            "state": drow_state,
        }
        payload["swarm_runtime"] = swarm_result
        payload["blockers"] = swarm_result.get("blockers", [])
        payload["panes"] = pane_results
        payload["screen_shares"] = share_results
        return payload

    def _load_incident(self, path: str | None) -> Dict[str, Any]:
        if not path:
            return {
                "id": "INC-DEMO-IDE-001",
                "severity": 8,
                "type": "investigation",
                "host": "wkstn-445",
                "suspect_account": "j.smith",
                "source_ip": "185.199.110.153",
            }
        with open(path, "r", encoding="utf-8") as f:
            return json.load(f)
