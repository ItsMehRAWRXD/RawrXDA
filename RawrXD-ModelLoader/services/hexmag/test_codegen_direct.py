import asyncio
import sys
import os

# Add the current directory to sys.path so we can import core and bots
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

from core.contracts import Event
from run_loop import Engine

async def test_codegen(size="small"):
    print("Initializing Engine...")
    engine = Engine()
    
    # Verify bots are loaded
    print(f"Loaded bots: {[b.name for b in engine.bots]}")
    
    # Create the event
    question = f"Write me a C++ ASM parser (size: {size})"
    event = Event(kind="llm.question", payload={"question": question, "size": size})
    
    print(f"Sending event: {question}")
    engine.add(event)
    
    # Run the loop
    # We need to run step() enough times to process the event.
    # step() pops one event.
    await engine.step()
    
    # Check history
    found = False
    for finding in engine.history:
        if finding.bot == "codegen-bot":
            print("\n--- Finding from codegen-bot ---")
            print(f"Rationale: {finding.rationale}")
            print(f"Code Snippet (first 100 chars):\n{finding.data['answer'][:100]}...")
            found = True
            
            if size == "large":
                if "Large / Enterprise" in finding.data['answer']:
                    print("\nSUCCESS: Generated code is Large/Enterprise version")
                else:
                    print("\nFAILURE: Generated code is NOT Large/Enterprise version")
            else:
                if "Small" in finding.data['answer']:
                    print("\nSUCCESS: Generated code is Small version")
                else:
                    print("\nFAILURE: Generated code is NOT Small version")
                
    if not found:
        print("\nFAILURE: No finding produced by codegen-bot")
        # Check if fallback ran
        for finding in engine.history:
             print(f"Other finding: {finding.bot} - {finding.rationale}")

if __name__ == "__main__":
    size = sys.argv[1] if len(sys.argv) > 1 else "small"
    asyncio.run(test_codegen(size))
