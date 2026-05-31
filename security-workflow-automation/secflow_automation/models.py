from __future__ import annotations

from dataclasses import dataclass, field
from datetime import datetime, timezone
from typing import Any, Dict, List, Optional


@dataclass
class WorkflowStep:
    id: str
    action: str
    params: Dict[str, Any] = field(default_factory=dict)
    depends_on: List[str] = field(default_factory=list)
    condition: Optional[str] = None
    retries: int = 0


@dataclass
class WorkflowDefinition:
    name: str
    description: str
    risk_tier: str
    required_approvals: List[str] = field(default_factory=list)
    steps: List[WorkflowStep] = field(default_factory=list)


@dataclass
class ExecutionContext:
    operator: str
    incident: Dict[str, Any]
    findings: List[Dict[str, Any]] = field(default_factory=list)
    artifacts: List[Dict[str, Any]] = field(default_factory=list)
    vars: Dict[str, Any] = field(default_factory=dict)
    approvals: List[str] = field(default_factory=list)
    dry_run: bool = True


@dataclass
class StepResult:
    step_id: str
    success: bool
    status: str
    details: Dict[str, Any] = field(default_factory=dict)
    attempts: int = 1


@dataclass
class AuditRecord:
    timestamp_utc: str
    level: str
    event: str
    step_id: str
    details: Dict[str, Any] = field(default_factory=dict)

    @staticmethod
    def create(level: str, event: str, step_id: str, details: Optional[Dict[str, Any]] = None) -> "AuditRecord":
        return AuditRecord(
            timestamp_utc=datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
            level=level,
            event=event,
            step_id=step_id,
            details=details or {},
        )


@dataclass
class ExecutionReport:
    workflow: str
    operator: str
    status: str
    started_utc: str
    ended_utc: str
    step_results: List[StepResult] = field(default_factory=list)
    audit_log: List[AuditRecord] = field(default_factory=list)
