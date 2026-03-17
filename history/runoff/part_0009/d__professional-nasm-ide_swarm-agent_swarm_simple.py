#!/usr/bin/env python3
"""
Simplified Swarm Agent System
Compatible with Python 3.13+
"""

import time
import threading
import queue
from datetime import datetime
from enum import Enum
from dataclasses import dataclass


class AgentType(Enum):
    AI_INFERENCE = "AI Inference"
    TEXT_EDITOR = "Text Editor"
    TEAM_VIEW = "Team Viewer"
    MARKETPLACE = "Marketplace"
    CODE_ANALYSIS = "Code Analysis"
    BUILD_SYSTEM = "Build System"
    DEBUG_AGENT = "Debug Agent"
    DOCS_AGENT = "Docs Agent"
    TEST_AGENT = "Test Agent"
    DEPLOY_AGENT = "Deploy Agent"


@dataclass
class Task:
    task_id: str
    task_type: str
    payload: dict


class SwarmAgent(threading.Thread):
    """Individual swarm agent"""
    
    def __init__(self, agent_id, agent_type, task_queue, result_queue):
        super().__init__(daemon=True)
        self.agent_id = agent_id
        self.agent_type = agent_type
        self.task_queue = task_queue
        self.result_queue = result_queue
        self.tasks_completed = 0
        self.running = True
        
    def run(self):
        print(f"[Agent {self.agent_id}] {self.agent_type.value} - STARTED")
        
        while self.running:
            try:
                task = self.task_queue.get(timeout=1)
                print(f"[Agent {self.agent_id}] Processing task {task.task_id}...")
                
                # Simulate processing
                time.sleep(0.5)
                
                result = {
                    "task_id": task.task_id,
                    "agent_id": self.agent_id,
                    "agent_type": self.agent_type.value,
                    "status": "completed",
                    "timestamp": datetime.now().isoformat()
                }
                
                self.result_queue.put(result)
                self.tasks_completed += 1
                
                print(f"[Agent {self.agent_id}] Task {task.task_id} completed! (Total: {self.tasks_completed})")
                
            except queue.Empty:
                time.sleep(0.1)
            except Exception as e:
                print(f"[Agent {self.agent_id}] ERROR: {e}")


class SwarmController:
    """Main swarm controller"""
    
    def __init__(self, num_agents=10):
        self.num_agents = num_agents
        self.agents = []
        self.task_queue = queue.Queue()
        self.result_queue = queue.Queue()
        self.task_counter = 0
        
    def start(self):
        print("=" * 70)
        print(" NASM IDE SWARM AGENT SYSTEM")
        print("=" * 70)
        print()
        
        agent_types = list(AgentType)
        
        for i in range(self.num_agents):
            agent_type = agent_types[i % len(agent_types)]
            agent = SwarmAgent(i, agent_type, self.task_queue, self.result_queue)
            agent.start()
            self.agents.append(agent)
            time.sleep(0.1)
        
        print()
        print(f"[OK] {self.num_agents} agents started successfully!")
        print()
        
    def submit_task(self, task_type, payload=None):
        self.task_counter += 1
        task = Task(
            task_id=f"task_{self.task_counter}",
            task_type=task_type,
            payload=payload or {}
        )
        self.task_queue.put(task)
        return task.task_id
    
    def get_results(self, count=None):
        results = []
        try:
            while True:
                if count and len(results) >= count:
                    break
                result = self.result_queue.get(timeout=0.1)
                results.append(result)
        except queue.Empty:
            pass
        return results
    
    def demo(self):
        print("Running demonstration...")
        print()
        
        # Submit demo tasks
        demo_tasks = [
            ("ai_inference", {"prompt": "Analyze code"}),
            ("build", {"project": "nasm_ide_integration.asm"}),
            ("format", {"file": "main.asm"}),
            ("search", {"query": "nasm extensions"}),
            ("test", {"suite": "unit_tests"}),
        ]
        
        print(f"Submitting {len(demo_tasks)} tasks...")
        for task_type, payload in demo_tasks:
            task_id = self.submit_task(task_type, payload)
            print(f"  -> {task_id}: {task_type}")
        
        print()
        print("Waiting for results...")
        time.sleep(3)
        
        results = self.get_results()
        print()
        print(f"[OK] Received {len(results)} results:")
        for result in results:
            print(f"  - {result['task_id']} by {result['agent_type']}")
        
        print()
        print("=" * 70)
        print("Demonstration complete!")
        print()
        
        # Show statistics
        total_tasks = sum(agent.tasks_completed for agent in self.agents)
        print(f"Statistics:")
        print(f"  Total tasks completed: {total_tasks}")
        print(f"  Active agents: {len(self.agents)}")
        print(f"  Queue depth: {self.task_queue.qsize()}")
        print()
        
    def stop(self):
        print("Stopping swarm...")
        for agent in self.agents:
            agent.running = False
        print("[OK] All agents stopped")


def main():
    controller = SwarmController(num_agents=10)
    
    try:
        controller.start()
        controller.demo()
        
        print("Press Ctrl+C to exit...")
        while True:
            time.sleep(1)
            
    except KeyboardInterrupt:
        print("\n")
        controller.stop()
        print("Goodbye!")


if __name__ == "__main__":
    main()
