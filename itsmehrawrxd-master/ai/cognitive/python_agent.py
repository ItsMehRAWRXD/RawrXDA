# Python template for the multi-modal, attention-deficient AI agent
from dataclasses import dataclass
from typing import Optional, List, Dict, Any
import time
import random
import queue
from datetime import datetime

@dataclass
class Focus:
    code_context: str
    file_path: str
    line_number: int
    start_time: float
    intensity: float = 1.0
    
    def __post_init__(self):
        if self.start_time == 0:
            self.start_time = time.time()

@dataclass
class Distraction:
    origin: str
    content: str
    priority: float
    timestamp: float
    
    def __post_init__(self):
        if self.timestamp == 0:
            self.timestamp = time.time()

class CognitiveStateManager:
    def __init__(self, ai_provider=None, vdb=None):
        self.ai_provider = ai_provider
        self.vdb = vdb
        self.current_focus: Optional[Focus] = None
        self.previous_focus: Optional[Focus] = None
        self.distractions = queue.Queue()
        self.rng = random.Random()
        
    def run_cycle(self):
        """Main cognitive cycle"""
        # Check if we should shift attention
        if self.should_shift_attention():
            self.shift_attention()
        
        # Process current focus
        if self.current_focus:
            self.execute_focus_burst()
        
        # Check for impulsive actions
        if self.should_do_impulse_action():
            self.do_impulse_action()
        
        # Process distractions
        self.process_distractions()
    
    def add_distraction(self, distraction: Distraction):
        """Add a distraction"""
        self.distractions.put(distraction)
    
    def set_focus(self, focus: Focus):
        """Set current focus"""
        if self.current_focus:
            self.previous_focus = self.current_focus
        self.current_focus = focus
    
    def get_current_focus(self) -> Optional[Focus]:
        """Get current focus"""
        return self.current_focus
    
    def get_previous_focus(self) -> Optional[Focus]:
        """Get previous focus"""
        return self.previous_focus
    
    def get_recent_distractions(self) -> List[Distraction]:
        """Get recent distractions"""
        recent = []
        temp_queue = queue.Queue()
        
        # Copy distractions to temp queue
        while not self.distractions.empty():
            distraction = self.distractions.get()
            recent.append(distraction)
            temp_queue.put(distraction)
        
        # Restore distractions
        while not temp_queue.empty():
            self.distractions.put(temp_queue.get())
        
        return recent
    
    def get_impulsive_refactor_idea(self, code_context: str) -> str:
        """Get impulsive refactor idea"""
        if self.ai_provider:
            prompt = f"Give me a quick refactor idea for this code: {code_context}"
            return self.ai_provider.generate_completion(prompt)
        return "Consider extracting this into a separate function"
    
    def should_shift_attention(self) -> bool:
        """Check if we should shift attention"""
        if not self.current_focus:
            return True
        
        focus_duration = time.time() - self.current_focus.start_time
        
        # Probabilistic attention shift based on focus duration
        shift_probability = min(0.1 + focus_duration * 0.01, 0.8)
        
        return self.rng.random() < shift_probability
    
    def shift_attention(self):
        """Shift attention to a new focus"""
        if self.distractions.empty():
            return
        
        # Get highest priority distraction
        distraction = self.distractions.get()
        
        # Create new focus from distraction
        new_focus = Focus(
            code_context=distraction.content,
            file_path="unknown",
            line_number=0,
            start_time=time.time(),
            intensity=distraction.priority
        )
        self.set_focus(new_focus)
    
    def execute_focus_burst(self):
        """Execute a burst of focused work"""
        if not self.current_focus:
            return
        
        # Simulate focused work
        work_units = self.rng.randint(1, 5)
        
        for i in range(work_units):
            # Simulate work on current focus
            if self.ai_provider:
                prompt = f"Continue working on: {self.current_focus.code_context}"
                self.ai_provider.generate_completion(prompt)
    
    def should_do_impulse_action(self) -> bool:
        """Check if we should do an impulsive action"""
        return self.rng.random() < 0.15  # 15% chance of impulsive action
    
    def do_impulse_action(self):
        """Do an impulsive action"""
        impulsive_actions = [
            "Refactor this function",
            "Add error handling",
            "Optimize this loop",
            "Add comments",
            "Extract this into a class",
            "Add unit tests",
            "Improve variable names"
        ]
        
        action = self.rng.choice(impulsive_actions)
        
        # Create distraction from impulsive action
        impulse = Distraction(
            origin="impulse",
            content=action,
            priority=0.8,
            timestamp=time.time()
        )
        self.add_distraction(impulse)
    
    def process_distractions(self):
        """Process pending distractions"""
        # Limit distraction queue size
        while self.distractions.qsize() > 10:
            self.distractions.get()

# Example usage
if __name__ == "__main__":
    # Create a simple AI provider mock
    class MockAIProvider:
        def generate_completion(self, prompt: str) -> str:
            return f"AI response to: {prompt}"
    
    # Initialize the cognitive state manager
    ai_provider = MockAIProvider()
    state_manager = CognitiveStateManager(ai_provider)
    
    # Set initial focus
    initial_focus = Focus(
        code_context="def calculate_sum(a, b):",
        file_path="math.py",
        line_number=1,
        start_time=time.time()
    )
    state_manager.set_focus(initial_focus)
    
    # Run cognitive cycles
    for i in range(10):
        print(f"Cycle {i+1}:")
        state_manager.run_cycle()
        
        current_focus = state_manager.get_current_focus()
        if current_focus:
            print(f"  Current focus: {current_focus.code_context}")
        
        time.sleep(0.1)  # Small delay between cycles