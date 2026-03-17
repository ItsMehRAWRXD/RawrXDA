#!/usr/bin/env python3
"""
BigDaddyG Beast Mini - Lightweight AI Agent
Standalone version without external dependencies
"""

import json
import random
import time

class BeastMini:
    def __init__(self, agent_id="beast-01", size_mb=250):
        self.id = agent_id
        self.size_mb = size_mb
        self.personality = "helpful"
        self.context = []
        self.knowledge_base = {
            "greetings": ["Hey there!", "Hello!", "What's up?", "Howdy!"],
            "code_help": [
                "I can help you write clean, efficient code.",
                "Let me analyze that code structure for you.",
                "Here's an optimized solution...",
                "That looks good, but consider this improvement..."
            ],
            "debugging": [
                "Let me trace through this logic...",
                "I found the issue! It's likely caused by...",
                "Try adding some debug logs here:",
                "This error suggests..."
            ],
            "farewells": ["See ya!", "Later!", "Happy coding!", "Peace out!"]
        }
        self.current_task = None
        print(f"🔥 Beast Mini {self.id} initialized ({self.size_mb}MB)")
    
    def process_request(self, user_input, task_type="general"):
        """Process user input and generate appropriate response"""
        self.context.append({"user": user_input, "timestamp": time.time()})
        self.current_task = task_type
        
        # Simulate processing time
        processing_time = random.uniform(0.1, 0.5)
        time.sleep(processing_time)
        
        response = self.generate_response(user_input, task_type)
        
        self.context.append({"agent": response, "timestamp": time.time()})
        self.current_task = None
        
        return {
            "response": response,
            "processing_time_ms": int(processing_time * 1000),
            "agent_id": self.id,
            "confidence": random.uniform(0.8, 0.95)
        }
    
    def generate_response(self, input_text, task_type):
        """Generate contextual response based on input and task type"""
        input_lower = input_text.lower()
        
        # Greeting detection
        if any(greeting in input_lower for greeting in ["hello", "hi", "hey", "howdy"]):
            return random.choice(self.knowledge_base["greetings"])
        
        # Farewell detection
        if any(farewell in input_lower for farewell in ["bye", "goodbye", "see ya", "later"]):
            return random.choice(self.knowledge_base["farewells"])
        
        # Task-specific responses
        if task_type == "code_generation":
            return self.handle_code_generation(input_text)
        elif task_type == "debugging":
            return self.handle_debugging(input_text)
        elif task_type == "optimization":
            return self.handle_optimization(input_text)
        else:
            return self.handle_general(input_text)
    
    def handle_code_generation(self, input_text):
        """Handle code generation requests"""
        if "function" in input_text.lower():
            return """Here's a template function:

```python
def process_data(data):
    \"\"\"Process input data and return results\"\"\"
    try:
        # Add your logic here
        result = data * 2
        return result
    except Exception as e:
        print(f"Error: {e}")
        return None
```

Need any specific modifications?"""
        
        elif "class" in input_text.lower():
            return """Here's a class template:

```python
class DataProcessor:
    def __init__(self):
        self.data = []
    
    def add_item(self, item):
        self.data.append(item)
    
    def process(self):
        return [item * 2 for item in self.data]
```

What functionality do you need?"""
        
        else:
            return random.choice(self.knowledge_base["code_help"])
    
    def handle_debugging(self, input_text):
        """Handle debugging requests"""
        if "error" in input_text.lower():
            return "Let me help you debug that error. Can you share the error message and the code that's causing it?"
        elif "not working" in input_text.lower():
            return "I'll help you figure out what's wrong. Let's trace through the logic step by step."
        else:
            return random.choice(self.knowledge_base["debugging"])
    
    def handle_optimization(self, input_text):
        """Handle optimization requests"""
        return f"For optimization, I suggest:\n1. Profile the code first\n2. Identify bottlenecks\n3. Apply appropriate optimizations\n\nWhat specific area needs optimization?"
    
    def handle_general(self, input_text):
        """Handle general queries"""
        return f"I understand you're asking about: '{input_text}'. I'm a {self.size_mb}MB AI agent focused on coding assistance. How can I help?"
    
    def get_status(self):
        """Get current agent status"""
        return {
            "id": self.id,
            "size_mb": self.size_mb,
            "personality": self.personality,
            "current_task": self.current_task,
            "context_length": len(self.context),
            "memory_usage": f"{self.size_mb}MB"
        }

def main():
    """Interactive demo of Beast Mini agent"""
    print("=" * 60)
    print("🔥 BigDaddyG Beast Mini - Interactive Demo")
    print("=" * 60)
    
    # Create agent
    agent = BeastMini("beast-demo", 280)
    
    # Demo interactions
    test_inputs = [
        ("Hello Beast!", "general"),
        ("Create a function to calculate fibonacci", "code_generation"),
        ("My code has a bug and won't run", "debugging"),
        ("How can I make this faster?", "optimization"),
        ("Generate a class for user management", "code_generation"),
        ("Goodbye!", "general")
    ]
    
    print(f"\n🤖 Agent Status: {json.dumps(agent.get_status(), indent=2)}")
    print("\n📋 Running Demo Scenarios...\n")
    
    for i, (user_input, task_type) in enumerate(test_inputs, 1):
        print(f"[Scenario {i}] User: {user_input}")
        result = agent.process_request(user_input, task_type)
        
        print(f"[Agent {result['agent_id']}] ({result['processing_time_ms']}ms, {result['confidence']:.1%} confidence)")
        print(f"{result['response']}")
        print("-" * 50)
        time.sleep(1)  # Pause between responses
    
    print(f"\n📊 Final Agent Status: {json.dumps(agent.get_status(), indent=2)}")
    print("\n✅ Demo completed! Beast Mini is ready for swarm integration.")

if __name__ == "__main__":
    main()