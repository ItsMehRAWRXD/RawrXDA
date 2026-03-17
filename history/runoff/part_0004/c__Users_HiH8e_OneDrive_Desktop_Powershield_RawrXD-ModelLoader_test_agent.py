#!/usr/bin/env python3
"""Quick sanity check for the HexMag agent endpoint."""
import json
import sys

import httpx


URL = "http://localhost:8000/agent"
GOAL = "list the first 3 prime numbers and stop"


def main() -> int:
    with httpx.stream("POST", URL, json={"goal": GOAL, "max_time": 30}) as response:
        for line in response.iter_lines():
            if not line:
                continue
            if line.startswith("data:"):
                message = json.loads(line[5:])
                print(message)
                if message.get("kind") == "goal.satisfied":
                    print("[OK] Agent reached goal autonomously")
                    return 0
            if line.startswith("event: finished"):
                print("[OK] Stream closed with final result")
                return 0

    print("[ERR] Goal never satisfied")
    return 1


if __name__ == "__main__":
    sys.exit(main())
