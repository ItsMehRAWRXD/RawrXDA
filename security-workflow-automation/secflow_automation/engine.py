from __future__ import annotations

import json
from dataclasses import asdict
from datetime import datetime, timezone
from typing import Any, Dict, List, Optional

from .actions import execute_action
from .models import (
    AuditRecord,
    ExecutionContext,
    ExecutionReport,
    StepResult,
    WorkflowDefinition,
    WorkflowStep,
)
from .policy import SecurityPolicyEngine


class WorkflowEngine:
    def __init__(self, policy: Optional[SecurityPolicyEngine] = None) -> None:
        self.policy = policy or SecurityPolicyEngine()

    def load_workflow(self, path: str) -> WorkflowDefinition:
        with open(path, "r", encoding="utf-8") as f:
            raw = json.load(f)

        steps = [
            WorkflowStep(
                id=s["id"],
                action=s["action"],
                params=s.get("params", {}),
                depends_on=s.get("depends_on", []),
                condition=s.get("condition"),
                retries=int(s.get("retries", 0)),
            )
            for s in raw.get("steps", [])
        ]

        wf = WorkflowDefinition(
            name=raw["name"],
            description=raw.get("description", ""),
            risk_tier=raw.get("risk_tier", "high"),
            required_approvals=raw.get("required_approvals", []),
            steps=steps,
        )

        ok, errors = self.policy.validate_workflow(wf)
        if not ok:
            raise ValueError("invalid workflow: " + "; ".join(errors))

        return wf

    def execute(self, workflow: WorkflowDefinition, ctx: ExecutionContext) -> ExecutionReport:
        start = datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")
        audit: List[AuditRecord] = []
        results: List[StepResult] = []

        by_id = {s.id: s for s in workflow.steps}
        executed: Dict[str, StepResult] = {}

        while len(executed) < len(workflow.steps):
            progress = False

            for step in workflow.steps:
                if step.id in executed:
                    continue

                if not self._deps_satisfied(step, executed):
                    continue

                progress = True

                if step.condition and not self._condition_met(step.condition, ctx):
                    sr = StepResult(step_id=step.id, success=True, status="skipped", details={"reason": "condition_false"})
                    executed[step.id] = sr
                    results.append(sr)
                    audit.append(AuditRecord.create("INFO", "step_skipped", step.id, sr.details))
                    continue

                decision = self.policy.authorize_step(workflow, step, ctx)
                if not decision.allowed:
                    sr = StepResult(step_id=step.id, success=False, status="blocked", details={"reason": decision.reason})
                    executed[step.id] = sr
                    results.append(sr)
                    audit.append(AuditRecord.create("ERROR", "step_blocked", step.id, sr.details))
                    continue

                attempt = 0
                final: Optional[StepResult] = None

                while attempt <= step.retries:
                    attempt += 1
                    audit.append(AuditRecord.create("INFO", "step_started", step.id, {"attempt": attempt, "action": step.action}))

                    action_result = execute_action(step.action, ctx, step.params)
                    if action_result.success:
                        final = StepResult(
                            step_id=step.id,
                            success=True,
                            status="completed",
                            details=action_result.details,
                            attempts=attempt,
                        )
                        self._persist_step_output(ctx, step.id, action_result.details)
                        audit.append(AuditRecord.create("INFO", "step_completed", step.id, action_result.details))
                        break

                    audit.append(AuditRecord.create("WARN", "step_retry", step.id, {"attempt": attempt, **action_result.details}))

                if final is None:
                    final = StepResult(
                        step_id=step.id,
                        success=False,
                        status="failed",
                        details={"error": "retries_exhausted"},
                        attempts=attempt,
                    )
                    audit.append(AuditRecord.create("ERROR", "step_failed", step.id, final.details))

                executed[step.id] = final
                results.append(final)

            if not progress:
                raise RuntimeError("workflow made no progress, likely dependency cycle")

        status = "completed"
        if any(r.status in {"failed", "blocked"} for r in results):
            status = "partial_failure"

        end = datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")
        return ExecutionReport(
            workflow=workflow.name,
            operator=ctx.operator,
            status=status,
            started_utc=start,
            ended_utc=end,
            step_results=results,
            audit_log=audit,
        )

    @staticmethod
    def report_to_dict(report: ExecutionReport) -> Dict[str, Any]:
        return {
            "workflow": report.workflow,
            "operator": report.operator,
            "status": report.status,
            "started_utc": report.started_utc,
            "ended_utc": report.ended_utc,
            "step_results": [asdict(r) for r in report.step_results],
            "audit_log": [asdict(a) for a in report.audit_log],
        }

    @staticmethod
    def _deps_satisfied(step: WorkflowStep, executed: Dict[str, StepResult]) -> bool:
        for dep in step.depends_on:
            if dep not in executed:
                return False
            if executed[dep].status in {"failed", "blocked"}:
                return False
        return True

    @staticmethod
    def _condition_met(rule: str, ctx: ExecutionContext) -> bool:
        operators = [">=", "<=", "==", "!=", ">", "<"]
        op = None
        for candidate in operators:
            if candidate in rule:
                op = candidate
                break
        if op is None:
            return False

        left, right = [x.strip() for x in rule.split(op, 1)]

        left_val = WorkflowEngine._resolve_ref(left, ctx)
        right_val = WorkflowEngine._coerce_literal(right)

        try:
            if op == "==":
                return left_val == right_val
            if op == "!=":
                return left_val != right_val
            if op == ">=":
                return float(left_val) >= float(right_val)
            if op == "<=":
                return float(left_val) <= float(right_val)
            if op == ">":
                return float(left_val) > float(right_val)
            if op == "<":
                return float(left_val) < float(right_val)
        except Exception:
            return False

        return False

    @staticmethod
    def _resolve_ref(expr: str, ctx: ExecutionContext) -> Any:
        if expr.startswith("incident."):
            return WorkflowEngine._nested_get(ctx.incident, expr[len("incident."):])
        if expr.startswith("vars."):
            return WorkflowEngine._nested_get(ctx.vars, expr[len("vars."):])
        return WorkflowEngine._coerce_literal(expr)

    @staticmethod
    def _nested_get(obj: Dict[str, Any], path: str) -> Any:
        cur: Any = obj
        for part in path.split("."):
            if not isinstance(cur, dict) or part not in cur:
                return None
            cur = cur[part]
        return cur

    @staticmethod
    def _coerce_literal(raw: str) -> Any:
        val = raw.strip().strip("\"").strip("'")
        lowered = val.lower()
        if lowered == "true":
            return True
        if lowered == "false":
            return False
        try:
            if "." in val:
                return float(val)
            return int(val)
        except ValueError:
            return val

    @staticmethod
    def _persist_step_output(ctx: ExecutionContext, step_id: str, details: Dict[str, Any]) -> None:
        ctx.vars.setdefault("steps", {})[step_id] = details
        ctx.vars[step_id] = details
