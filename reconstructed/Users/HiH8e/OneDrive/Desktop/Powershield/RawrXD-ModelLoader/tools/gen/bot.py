import os
import json
import textwrap
import sys
from typing import List, Optional

TEMPLATE = """\
from core.contracts import Bot, Event, Finding

class {ClassName}(Bot):
    name = "{bot_name}"
    version = "0.0.1"

    def supports(self, event: Event) -> bool:
        # TODO: adjust condition
        return event.kind in {supports_kinds}

    async def run(self, event: Event):
        # TODO: implement logic
        return [Finding(
            bot=self.name,
            labels={"stub"},
            score=0.1,
            rationale=f"Stub for {self.name} saw {event.kind}",
            data={{"payload": event.payload}}
        )]
"""

MANIFEST = {
  "name": None,
  "capabilities": [],
  "inputs": [{"type": "*", "schema": "event"}],
  "outputs": [{"type": "finding"}],
  "perf": {"p95_latency_ms": 100, "cost_per_call_usd": 0.0},
  "tags": ["type:stub"]
}

def pascal(s: str) -> str:
    """Convert snake or dash names to PascalCase for class generation."""
    return "".join(part.capitalize() for part in s.replace("-", "_").split("_"))

def generate(bot_name: str, supports: Optional[List[str]] = None) -> None:
    """Create a new bot stub under bots/<bot_name>/ with a manifest."""
    supports = supports or ["*"]
    bot_dir = os.path.join("bots", bot_name)
    os.makedirs(bot_dir, exist_ok=True)

    cls_name = pascal(bot_name)
    code = TEMPLATE.format(ClassName=cls_name, bot_name=bot_name, supports_kinds=set(supports))
    with open(os.path.join(bot_dir, "__init__.py"), "w", encoding="utf-8") as module_file:
        module_file.write(textwrap.dedent(code))

    manifest = MANIFEST.copy()
    manifest["name"] = bot_name
    with open(os.path.join(bot_dir, "manifest.json"), "w", encoding="utf-8") as manifest_file:
        json.dump(manifest, manifest_file, indent=2)

    print(f"Created bot stub at {bot_dir}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python tools/gen_bot.py <bot-name> [kind1 kind2 ...]")
        sys.exit(1)

    bot_name = sys.argv[1]
    kinds = sys.argv[2:] or ["*"]
    generate(bot_name, kinds)
