# HexMag Bot Ecosystem

This directory contains the dynamic bots that power the HexMag swarm.
The engine automatically loads any valid bot found in this folder.

## Creating a New Bot

To create a new bot, add a `.py` file to this directory (e.g., `my_bot.py`).
Your file must define a class that inherits from `core.contracts.Bot`.

### Template

```python
from typing import List
from core.contracts import Bot, Event, Finding

class MyCustomBot(Bot):
    name = "my-custom-bot"
    version = "1.0.0"

    def supports(self, event: Event) -> bool:
        # Return True if this bot should handle the event
        return event.kind == "llm.question" and "my keyword" in event.payload.get("question", "")

    async def run(self, event: Event) -> List[Finding]:
        # Perform logic here
        return [
            Finding(
                bot=self.name,
                labels={"my.label"},
                score=1.0,
                rationale="I found something!",
                data={"answer": "Here is my result"}
            )
        ]
```

## Existing Bots

### `codegen.py` (CodegenBot)
- **Triggers**: Questions containing "parser", "parse", "ide", "editor".
- **Output**: Generates C++ boilerplate for ASM Parsers or Text Editors.
- **Use Case**: Backend for the Qt IDE Copilot feature.
