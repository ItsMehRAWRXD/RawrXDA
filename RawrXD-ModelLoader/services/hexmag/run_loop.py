import asyncio
from typing import List
from core.contracts import Event, Finding

class Engine:
    """
    The main execution loop for the HexMag swarm.
    """
    def __init__(self):
        self.q: List[Event] = []
        self.history: List[Finding] = []
        self.event_count = 0

    def add(self, event: Event) -> None:
        """Add an event to the processing queue."""
        self.q.append(event)

    async def step(self) -> None:
        """
        Process one step of the swarm loop.
        In a real implementation, this would dispatch events to bots.
        """
        if not self.q:
            await asyncio.sleep(0.01)
            return

        # Pop the next event
        event = self.q.pop(0)
        self.event_count += 1
        
        # Placeholder: In the future, this will route 'event' to registered bots
        # and collect 'Finding' objects into self.history.
