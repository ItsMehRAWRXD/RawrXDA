#!/usr/bin/env python3
"""
Run the swarm as a single AI-model reachable from any IDE.

$ python hexmag_engine.py
-> http://localhost:8000/ask (see OpenAPI schema at /docs)
"""
import asyncio
import json
import os
import sqlite3
import time
from typing import Any, Dict, List, Optional, Set

import uvicorn
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel

try:
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
            if self.event_count > 100:
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


app = FastAPI(title="HexMag-Swarm-as-Model", version="1.0.0")
swarm = SwarmModel()


@app.post("/ask", response_model=AskResponse)
async def ask_endpoint(req: AskRequest) -> AskResponse:
    return await swarm.ask(req.question, req.code, req.timeout)


@app.get("/health")
def health() -> Dict[str, Any]:
    return {"status": "ok", "queue": len(getattr(swarm.engine, "q", []))}


import argparse

async def run(port: int = 8000) -> None:
    async def background() -> None:
        while True:
            await swarm.engine.step()

    config = uvicorn.Config(app, host="0.0.0.0", port=port)
    server = uvicorn.Server(config)
    
    await asyncio.gather(
        background(),
        server.serve(),
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=8000, help="Port to run the server on")
    args = parser.parse_args()
    
    asyncio.run(run(port=args.port))
