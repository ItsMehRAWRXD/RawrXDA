from __future__ import annotations

from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass, asdict
from datetime import datetime, timezone
from itertools import count
from threading import Lock
import json
import os
from typing import Any, Dict, List, Optional


ROLES = ["ask", "plan", "agent", "support"]


def _utc_now() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


@dataclass
class ModelSession:
    id: str
    model: str
    stale: bool
    created_utc: str
    last_used_utc: str


class SessionPool:
    def __init__(self) -> None:
        self._lock = Lock()
        self._seq = count(1)
        self._sessions: Dict[str, ModelSession] = {}
        self._by_model: Dict[str, str] = {}

    def open_session(self, model: str, allow_share_same_model: bool = True) -> ModelSession:
        with self._lock:
            if allow_share_same_model and model in self._by_model:
                sid = self._by_model[model]
                sess = self._sessions[sid]
                if sess.stale:
                    sess = self._fresh_session(model)
                sess.last_used_utc = _utc_now()
                return sess
            return self._fresh_session(model)

    def mark_stale(self, session_id: str) -> None:
        with self._lock:
            if session_id in self._sessions:
                self._sessions[session_id].stale = True

    def ensure_fresh(self, session_id: str) -> ModelSession:
        with self._lock:
            sess = self._sessions[session_id]
            if sess.stale:
                return self._fresh_session(sess.model)
            sess.last_used_utc = _utc_now()
            return sess

    def _fresh_session(self, model: str) -> ModelSession:
        sid = f"sess-{next(self._seq)}"
        sess = ModelSession(
            id=sid,
            model=model,
            stale=False,
            created_utc=_utc_now(),
            last_used_utc=_utc_now(),
        )
        self._sessions[sid] = sess
        self._by_model[model] = sid
        return sess


@dataclass
class ChatPane:
    pane_id: str
    model: str
    session_id: str


class ChatPaneManager:
    def __init__(self, session_pool: SessionPool) -> None:
        self.pool = session_pool
        self.panes: Dict[str, ChatPane] = {}

    def open_pane(self, pane_id: str, model: str, allow_share_same_model: bool = True) -> ChatPane:
        sess = self.pool.open_session(model, allow_share_same_model=allow_share_same_model)
        pane = ChatPane(pane_id=pane_id, model=model, session_id=sess.id)
        self.panes[pane_id] = pane
        return pane

    def infer(self, pane_id: str, prompt: str) -> Dict[str, Any]:
        pane = self.panes[pane_id]
        sess = self.pool.ensure_fresh(pane.session_id)
        pane.session_id = sess.id
        tps = 8200.0 if "cuda" in pane.model.lower() else 9100.0
        if "amd" in pane.model.lower() or "rocm" in pane.model.lower() or "phi3" in pane.model.lower():
            tps = 9800.0
        return {
            "pane_id": pane_id,
            "session_id": sess.id,
            "model": pane.model,
            "response": f"[{pane.model}] processed prompt ({len(prompt)} chars)",
            "tps": tps,
        }


@dataclass
class AgentRun:
    agent_id: int
    role: str
    model: str
    success: bool
    blocker: Optional[str]
    output: str


class SwarmRuntime:
    def __init__(self, session_pool: SessionPool) -> None:
        self.pool = session_pool

    def run(
        self,
        swarm_size: int,
        default_model: str,
        model_by_agent: Dict[int, str],
        role_models: Dict[str, str],
        drow_name: str,
        parallel: bool,
    ) -> Dict[str, Any]:
        runs: List[AgentRun] = []

        def _execute(agent_id: int) -> AgentRun:
            role = ROLES[(agent_id - 1) % len(ROLES)]
            model = model_by_agent.get(agent_id, role_models.get(role, default_model))
            sess = self.pool.open_session(model, allow_share_same_model=True)

            # Simulate occasional staleness and immediate fresh session recovery.
            if agent_id % 11 == 0:
                self.pool.mark_stale(sess.id)
                sess = self.pool.ensure_fresh(sess.id)

            blocker = None
            if not model:
                blocker = "model-missing"

            return AgentRun(
                agent_id=agent_id,
                role=role,
                model=model,
                success=blocker is None,
                blocker=blocker,
                output=f"drow={drow_name}; role={role}; session={sess.id}",
            )

        if parallel:
            with ThreadPoolExecutor(max_workers=swarm_size) as ex:
                future_map = {ex.submit(_execute, idx): idx for idx in range(1, swarm_size + 1)}
                for fut in as_completed(future_map):
                    runs.append(fut.result())
        else:
            for idx in range(1, swarm_size + 1):
                runs.append(_execute(idx))

        runs.sort(key=lambda r: r.agent_id)
        blockers = [r.blocker for r in runs if r.blocker]
        tps_estimate = round(sum(9800.0 if "amd" in r.model.lower() or "phi3" in r.model.lower() else 8200.0 for r in runs), 2)

        return {
            "swarm_size": swarm_size,
            "parallel": parallel,
            "drow": drow_name,
            "agent_runs": [asdict(r) for r in runs],
            "blockers": blockers,
            "status": "ok" if not blockers else "blocked",
            "estimated_total_tps": tps_estimate,
        }


@dataclass
class ScreenShareSession:
    id: str
    source: str
    label: str
    started_utc: str
    active: bool


class CollaborationRuntime:
    def __init__(self) -> None:
        self._seq = count(1)
        self._shares: Dict[str, ScreenShareSession] = {}

    def start_share(self, source: str, label: str) -> ScreenShareSession:
        sid = f"share-{next(self._seq)}"
        share = ScreenShareSession(
            id=sid,
            source=source,
            label=label,
            started_utc=_utc_now(),
            active=True,
        )
        self._shares[sid] = share
        return share

    def stop_share(self, share_id: str) -> bool:
        if share_id not in self._shares:
            return False
        self._shares[share_id].active = False
        return True

    def list_shares(self) -> List[Dict[str, Any]]:
        return [asdict(v) for v in self._shares.values()]


class DrowStateStore:
    def __init__(self, repo_root: str) -> None:
        self.path = os.path.join(repo_root, ".secflow_drow_state.json")
        self._lock = Lock()

    def load(self) -> Dict[str, Any]:
        if not os.path.exists(self.path):
            return {"drows": {}}
        try:
            with open(self.path, "r", encoding="utf-8") as f:
                return json.load(f)
        except Exception:
            return {"drows": {}}

    def save_result(self, drow_name: str, result: Dict[str, Any]) -> Dict[str, Any]:
        with self._lock:
            state = self.load()
            state.setdefault("drows", {})
            state["drows"][drow_name] = {
                "updated_utc": _utc_now(),
                "status": result.get("status", "unknown"),
                "blockers": result.get("blockers", []),
                "swarm_size": result.get("swarm_size", 0),
                "parallel": result.get("parallel", True),
            }
            with open(self.path, "w", encoding="utf-8") as f:
                json.dump(state, f, indent=2)
            return state["drows"][drow_name]
