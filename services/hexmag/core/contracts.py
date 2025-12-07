from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Dict, List, Set, Optional


@dataclass(frozen=True)
class Event:
    """Drive message flowing through the swarm."""

    kind: str
    payload: Dict[str, Any]
    source_bot: str = "user/initial"


@dataclass
class Finding:
    """Structured result produced by a bot."""

    bot: str
    labels: Set[str]
    score: float
    rationale: str
    data: Dict[str, Any] = field(default_factory=dict)


class Bot:
    """Abstract base class for all swarm bots."""

    name: str = "base-bot"
    version: str = "0.0.1"

    def supports(self, event: Event) -> bool:
        raise NotImplementedError

    async def run(self, event: Event) -> List[Finding]:
        raise NotImplementedError
