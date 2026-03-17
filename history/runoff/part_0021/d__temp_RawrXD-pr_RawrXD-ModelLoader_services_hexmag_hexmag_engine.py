#!/usr/bin/env python3
"""
Run the swarm as a single AI-model reachable from any IDE.

$ python hexmag_engine.py [--port PORT]
-> http://localhost:8001/ask (see OpenAPI schema at /docs)
"""
import argparse
import asyncio
import json
import os
import sqlite3
import sys
import time
from typing import Any, Dict, List, Optional, Set

import uvicorn
from fastapi import FastAPI, HTTPException
from fastapi.responses import StreamingResponse
from pydantic import BaseModel

try:
    from .core.contracts import Event, Finding
    from .run_loop import Engine
except ImportError:
    try:
        # Try absolute imports if running as script
        from core.contracts import Event, Finding
        from run_loop import Engine
    except ImportError:
        print("Warning: HexMag core files (contracts.py, run_loop.py) not found. Using stubs.")

    class Event:  # type: ignore
        def __init__(self, kind: str, payload: Dict[str, Any], source: str = "API/IDE"):
            self.kind = kind
            self.payload = payload
            self.source_bot = source

    class Finding:  # type: ignore
        def __init__(self, bot: str, score: float, labels: Set[str], rationale: str, data: Dict[str, Any]):
            self.bot = bot
            self.score = score
            self.labels = labels
            self.rationale = rationale
            self.data = data

    class Engine:  # type: ignore
        def __init__(self):
            self.q: List[Event] = []
            self.history: List[Finding] = []
            self.event_count = 0

        def add(self, event: Event) -> None:
            self.q.append(event)

        async def step(self) -> None:
            await asyncio.sleep(0.01)
            self.event_count += 1
            
            # Process queued events
            if self.q:
                event = self.q.pop(0)
                
                # Handle llm.question events
                if event.kind == "llm.question":
                    question = event.payload.get("question", "")
                    answer = f"Response to: {question[:50]}" if question else "Default response"
                    finding = Finding(
                        bot="HexMag-Engine",
                        score=1.0,
                        labels={"llm.answer"},
                        rationale="Stub answer",
                        data={"answer": answer}
                    )
                    self.history.append(finding)
                
                # Handle agent.goal events
                elif event.kind == "agent.goal":
                    goal = event.payload.get("goal", "")
                    finding = Finding(
                        bot="HexMag-Agent",
                        score=1.0,
                        labels={"goal.satisfied"},
                        rationale="Goal satisfied by stub",
                        data={"goal": goal}
                    )
                    self.history.append(finding)
            
            if self.event_count > 1000:
                raise RuntimeError("Stub engine timeout")


DB_FILE = "hexmag.sqlite"


class AskRequest(BaseModel):
    question: str
    code: Optional[str] = None
    timeout: float = 25.0


class AskResponse(BaseModel):
    answer: str
    sources: List[str]
    meta: Dict[str, Any]


class AgentRequest(BaseModel):
    goal: str
    max_time: float = 30.0



class SwarmModel:
    """Manages the HexMag engine and exposes a blocking ask() helper."""

    def __init__(self) -> None:
        self.engine = Engine()
        self.db_init()

    def db_init(self) -> None:
        with sqlite3.connect(DB_FILE) as c:
            c.execute(
                """
                CREATE TABLE IF NOT EXISTS findings (
                    id INTEGER PRIMARY KEY,
                    ts REAL,
                    bot TEXT,
                    score REAL,
                    labels TEXT,
                    rationale TEXT,
                    data TEXT
                )
                """
            )

    async def ask(self, question: str, code: Optional[str], timeout: float) -> AskResponse:
        prompt = question
        if code:
            prompt = f"{question}\n\nCode context:\n```\n{code}\n```"

        t0 = time.time()
        self.engine.add(Event(kind="llm.question", payload={"question": prompt}, source="API/IDE"))

        sources: Set[str] = set()
        while time.time() - t0 < timeout:
            await self.engine.step()

            final_answer: Optional[Finding] = None
            for item in reversed(getattr(self.engine, "history", [])):
                labels = getattr(item, "labels", set())
                data = getattr(item, "data", {})
                if "web.content" in labels and data.get("url"):
                    sources.add(data["url"])
                if "search.results" in labels:
                    sources.update(data.get("links", []))
                if final_answer is None and "llm.answer" in labels:
                    final_answer = item

            if final_answer:
                return AskResponse(
                    answer=final_answer.data["answer"],
                    sources=sorted(sources)[:10],
                    meta={
                        "events_processed": getattr(self.engine, "event_count", 0),
                        "findings": len(getattr(self.engine, "history", [])),
                        "elapsed": round(time.time() - t0, 2),
                    },
                )

            await asyncio.sleep(0.1)

        raise HTTPException(status_code=504, detail="Swarm did not produce an answer in time")

    async def agent(self, goal: str, max_time: float) -> Dict[str, Any]:
        """Execute a goal and stream status messages."""
        t0 = time.time()
        self.engine.add(Event(kind="agent.goal", payload={"goal": goal}, source="API/IDE"))

        while time.time() - t0 < max_time:
            await self.engine.step()
            
            # Check for goal satisfaction in findings
            for item in reversed(getattr(self.engine, "history", [])):
                labels = getattr(item, "labels", set())
                if "goal.satisfied" in labels:
                    return {"kind": "goal.satisfied", "elapsed": round(time.time() - t0, 2)}

            await asyncio.sleep(0.1)

        raise HTTPException(status_code=504, detail="Goal execution timeout")


app = FastAPI(title="HexMag-Swarm-as-Model", version="1.0.0")
swarm = SwarmModel()


@app.post("/ask", response_model=AskResponse)
async def ask_endpoint(req: AskRequest) -> AskResponse:
    return await swarm.ask(req.question, req.code, req.timeout)


@app.post("/agent")
async def agent_endpoint(req: AgentRequest):
    """Stream goal execution events (SSE format)."""
    async def event_generator():
        try:
            result = await swarm.agent(req.goal, req.max_time)
            # Send goal.satisfied event in SSE format
            yield f"data: {json.dumps(result)}\n\n"
        except HTTPException as exc:
            yield f"data: {json.dumps({'kind': 'error', 'detail': exc.detail})}\n\n"

    return StreamingResponse(event_generator(), media_type="text/event-stream")


@app.get("/health")
def health() -> Dict[str, Any]:
    return {"status": "ok", "queue": len(getattr(swarm.engine, "q", []))}


async def run(port: int = 8000) -> None:
    async def background() -> None:
        while True:
            await swarm.engine.step()

    await asyncio.gather(
        background(),
        uvicorn.Server(uvicorn.Config(app, host="0.0.0.0", port=port)).serve(),
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Start HexMag swarm engine")
    parser.add_argument("--port", type=int, default=8000, help="Port to listen on (default: 8000)")
    args = parser.parse_args()
    asyncio.run(run(args.port))
