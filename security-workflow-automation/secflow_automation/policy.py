from __future__ import annotations

from dataclasses import dataclass, field
from typing import Dict, List, Tuple

from .models import ExecutionContext, WorkflowDefinition, WorkflowStep


@dataclass
class PolicyDecision:
    allowed: bool
    reason: str


@dataclass
class SecurityPolicyEngine:
    high_impact_actions: List[str] = field(
        default_factory=lambda: [
            "isolate_host",
            "disable_account",
            "block_network_segment",
            "block_ip",
        ]
    )

    def validate_workflow(self, workflow: WorkflowDefinition) -> Tuple[bool, List[str]]:
        errors: List[str] = []
        ids = set()

        for step in workflow.steps:
            if step.id in ids:
                errors.append(f"duplicate step id: {step.id}")
            ids.add(step.id)

        for step in workflow.steps:
            for dep in step.depends_on:
                if dep not in ids:
                    errors.append(f"step {step.id} depends on missing step: {dep}")

        if not workflow.steps:
            errors.append("workflow has no steps")

        return (len(errors) == 0, errors)

    def authorize_step(self, workflow: WorkflowDefinition, step: WorkflowStep, ctx: ExecutionContext) -> PolicyDecision:
        requires_explicit_approval = step.id in workflow.required_approvals
        high_impact = step.action in self.high_impact_actions

        if high_impact and step.id not in ctx.approvals:
            return PolicyDecision(
                allowed=False,
                reason=(
                    f"step {step.id} is high-impact ({step.action}) and requires explicit approval"
                ),
            )

        if requires_explicit_approval and step.id not in ctx.approvals:
            return PolicyDecision(
                allowed=False,
                reason=f"step {step.id} is in required_approvals but has no approval token",
            )

        return PolicyDecision(allowed=True, reason="authorized")
