#!/usr/bin/env python3
"""
Interchangeable Execution (IE) System
Allows AIs and users to seamlessly pick up where the other left off
Creates a shared execution context that both can read/write to
"""

import threading
import json
import time
import uuid
from typing import Dict, List, Any, Optional, Union
from dataclasses import dataclass, asdict
from enum import Enum
import queue

class ExecutionActor(Enum):
    """Who is executing the task"""
    USER = "user"
    AI = "ai"
    SYSTEM = "system"

class TaskState(Enum):
    """Current state of a task"""
    CREATED = "created"
    IN_PROGRESS = "in_progress"
    PAUSED = "paused"
    WAITING_FOR_HANDOFF = "waiting_for_handoff"
    COMPLETED = "completed"
    ERROR = "error"

@dataclass
class ExecutionContext:
    """Shared execution context that both AI and user can access"""
    task_id: str
    task_name: str
    current_actor: ExecutionActor
    state: TaskState
    created_at: float
    last_updated: float
    
    # Shared data
    code_content: str = ""
    current_file: str = ""
    cursor_position: int = 0
    selected_text: str = ""
    
    # Execution state
    variables: Dict[str, Any] = None
    breakpoints: List[int] = None
    call_stack: List[str] = None
    terminal_output: str = ""
    error_messages: List[str] = None
    
    # AI-specific data
    ai_thoughts: str = ""
    ai_plan: List[str] = None
    ai_suggestions: List[str] = None
    
    # User-specific data
    user_notes: str = ""
    user_intent: str = ""
    user_preferences: Dict[str, Any] = None
    
    def __post_init__(self):
        if self.variables is None:
            self.variables = {}
        if self.breakpoints is None:
            self.breakpoints = []
        if self.call_stack is None:
            self.call_stack = []
        if self.error_messages is None:
            self.error_messages = []
        if self.ai_plan is None:
            self.ai_plan = []
        if self.ai_suggestions is None:
            self.ai_suggestions = []
        if self.user_preferences is None:
            self.user_preferences = {}

class InterchangeableExecutionManager:
    """Manages seamless handoffs between AI and user execution"""
    
    def __init__(self):
        self.active_contexts: Dict[str, ExecutionContext] = {}
        self.handoff_queue = queue.Queue()
        self.execution_lock = threading.Lock()
        self.observers: List[callable] = []
        
        # AI capabilities
        self.ai_capabilities = {
            'code_generation': True,
            'debugging': True,
            'refactoring': True,
            'testing': True,
            'documentation': True,
            'optimization': True
        }
        
        # User capabilities
        self.user_capabilities = {
            'creative_design': True,
            'business_logic': True,
            'domain_knowledge': True,
            'decision_making': True,
            'review': True,
            'approval': True
        }
    
    def create_context(self, task_name: str, initial_actor: ExecutionActor = ExecutionActor.USER) -> str:
        """Create a new execution context"""
        task_id = str(uuid.uuid4())
        context = ExecutionContext(
            task_id=task_id,
            task_name=task_name,
            current_actor=initial_actor,
            state=TaskState.CREATED,
            created_at=time.time(),
            last_updated=time.time()
        )
        
        with self.execution_lock:
            self.active_contexts[task_id] = context
        
        self._notify_observers('context_created', context)
        return task_id
    
    def handoff_to_ai(self, task_id: str, handoff_reason: str = "") -> bool:
        """Hand off execution to AI"""
        with self.execution_lock:
            if task_id not in self.active_contexts:
                return False
            
            context = self.active_contexts[task_id]
            if context.current_actor == ExecutionActor.AI:
                return False  # Already with AI
            
            # Update context for AI handoff
            context.current_actor = ExecutionActor.AI
            context.state = TaskState.IN_PROGRESS
            context.last_updated = time.time()
            
            # Add handoff information
            context.user_notes += f"\n[Handoff to AI: {handoff_reason}]"
            
            # Queue for AI processing
            self.handoff_queue.put({
                'task_id': task_id,
                'from_actor': ExecutionActor.USER,
                'to_actor': ExecutionActor.AI,
                'reason': handoff_reason,
                'context': context
            })
        
        self._notify_observers('handoff_to_ai', context)
        return True
    
    def handoff_to_user(self, task_id: str, handoff_reason: str = "") -> bool:
        """Hand off execution to user"""
        with self.execution_lock:
            if task_id not in self.active_contexts:
                return False
            
            context = self.active_contexts[task_id]
            if context.current_actor == ExecutionActor.USER:
                return False  # Already with user
            
            # Update context for user handoff
            context.current_actor = ExecutionActor.USER
            context.state = TaskState.WAITING_FOR_HANDOFF
            context.last_updated = time.time()
            
            # Add AI summary
            context.ai_thoughts += f"\n[Handoff to User: {handoff_reason}]"
            
            # Queue for user processing
            self.handoff_queue.put({
                'task_id': task_id,
                'from_actor': ExecutionActor.AI,
                'to_actor': ExecutionActor.USER,
                'reason': handoff_reason,
                'context': context
            })
        
        self._notify_observers('handoff_to_user', context)
        return True
    
    def update_context(self, task_id: str, updates: Dict[str, Any], actor: ExecutionActor):
        """Update execution context"""
        with self.execution_lock:
            if task_id not in self.active_contexts:
                return False
            
            context = self.active_contexts[task_id]
            
            # Only allow updates from current actor or system
            if context.current_actor != actor and actor != ExecutionActor.SYSTEM:
                return False
            
            # Apply updates
            for key, value in updates.items():
                if hasattr(context, key):
                    setattr(context, key, value)
            
            context.last_updated = time.time()
        
        self._notify_observers('context_updated', context)
        return True
    
    def get_context(self, task_id: str) -> Optional[ExecutionContext]:
        """Get current execution context"""
        with self.execution_lock:
            return self.active_contexts.get(task_id)
    
    def get_handoff_suggestions(self, task_id: str) -> List[str]:
        """Get suggestions for when to hand off to the other actor"""
        context = self.get_context(task_id)
        if not context:
            return []
        
        suggestions = []
        
        # AI handoff suggestions (when user should take over)
        if context.current_actor == ExecutionActor.AI:
            if "TODO" in context.code_content or "FIXME" in context.code_content:
                suggestions.append("User input needed for TODO/FIXME items")
            
            if len(context.error_messages) > 0:
                suggestions.append("User decision needed for error handling")
            
            if "business logic" in context.ai_thoughts.lower():
                suggestions.append("User expertise needed for business logic")
            
            if context.state == TaskState.ERROR:
                suggestions.append("User intervention needed for error resolution")
        
        # User handoff suggestions (when AI should take over)
        elif context.current_actor == ExecutionActor.USER:
            if "refactor" in context.user_intent.lower():
                suggestions.append("AI can help with code refactoring")
            
            if "debug" in context.user_intent.lower():
                suggestions.append("AI can assist with debugging")
            
            if "test" in context.user_intent.lower():
                suggestions.append("AI can generate test cases")
            
            if "optimize" in context.user_intent.lower():
                suggestions.append("AI can optimize code performance")
        
        return suggestions
    
    def auto_handoff(self, task_id: str) -> bool:
        """Automatically determine if handoff is needed"""
        context = self.get_context(task_id)
        if not context:
            return False
        
        suggestions = self.get_handoff_suggestions(task_id)
        
        # Auto-handoff based on context analysis
        if context.current_actor == ExecutionActor.AI:
            # AI should hand off to user for:
            if any(keyword in context.code_content.lower() for keyword in ['business', 'domain', 'requirement']):
                return self.handoff_to_user(task_id, "Domain knowledge required")
            
            if context.state == TaskState.ERROR and len(context.error_messages) > 2:
                return self.handoff_to_user(task_id, "Complex error resolution needed")
        
        elif context.current_actor == ExecutionActor.USER:
            # User should hand off to AI for:
            if any(keyword in context.user_intent.lower() for keyword in ['generate', 'create', 'write']):
                return self.handoff_to_ai(task_id, "Code generation requested")
            
            if 'debug' in context.user_intent.lower() and len(context.error_messages) > 0:
                return self.handoff_to_ai(task_id, "Debugging assistance needed")
        
        return False
    
    def get_execution_summary(self, task_id: str) -> Dict[str, Any]:
        """Get summary of current execution state"""
        context = self.get_context(task_id)
        if not context:
            return {}
        
        return {
            'task_id': task_id,
            'task_name': context.task_name,
            'current_actor': context.current_actor.value,
            'state': context.state.value,
            'code_length': len(context.code_content),
            'current_file': context.current_file,
            'cursor_position': context.cursor_position,
            'variables_count': len(context.variables),
            'breakpoints_count': len(context.breakpoints),
            'error_count': len(context.error_messages),
            'ai_suggestions_count': len(context.ai_suggestions),
            'last_updated': context.last_updated,
            'execution_time': time.time() - context.created_at
        }
    
    def get_handoff_queue(self) -> List[Dict]:
        """Get pending handoffs"""
        handoffs = []
        while not self.handoff_queue.empty():
            try:
                handoffs.append(self.handoff_queue.get_nowait())
            except queue.Empty:
                break
        return handoffs
    
    def _notify_observers(self, event_type: str, context: ExecutionContext):
        """Notify observers of context changes"""
        for observer in self.observers:
            try:
                observer(event_type, context)
            except Exception as e:
                print(f"Observer error: {e}")
    
    def add_observer(self, observer: callable):
        """Add observer for context changes"""
        self.observers.append(observer)
    
    def remove_observer(self, observer: callable):
        """Remove observer"""
        if observer in self.observers:
            self.observers.remove(observer)

# Example usage and integration
class IDEInterchangeableExecution:
    """Integration with IDE for seamless AI/User handoffs"""
    
    def __init__(self):
        self.ie_manager = InterchangeableExecutionManager()
        self.current_task_id = None
        
        # Add observer for IDE updates
        self.ie_manager.add_observer(self._handle_context_change)
    
    def start_coding_session(self, task_name: str, initial_actor: ExecutionActor = ExecutionActor.USER) -> str:
        """Start a new coding session with IE"""
        task_id = self.ie_manager.create_context(task_name, initial_actor)
        self.current_task_id = task_id
        
        print(f"🚀 Started IE coding session: {task_name}")
        print(f"   Initial actor: {initial_actor.value}")
        print(f"   Task ID: {task_id}")
        
        return task_id
    
    def user_types_code(self, code: str, file_path: str = ""):
        """User is typing code"""
        if not self.current_task_id:
            return
        
        updates = {
            'code_content': code,
            'current_file': file_path,
            'user_intent': 'coding'
        }
        
        self.ie_manager.update_context(self.current_task_id, updates, ExecutionActor.USER)
        
        # Check if auto-handoff is needed
        if self.ie_manager.auto_handoff(self.current_task_id):
            print("🤖 Auto-handoff to AI triggered")
    
    def ai_suggests_code(self, suggestion: str, reasoning: str = ""):
        """AI suggests code changes"""
        if not self.current_task_id:
            return
        
        context = self.ie_manager.get_context(self.current_task_id)
        if context:
            updates = {
                'ai_suggestions': context.ai_suggestions + [suggestion],
                'ai_thoughts': context.ai_thoughts + f"\n[AI Suggestion: {reasoning}]"
            }
            
            self.ie_manager.update_context(self.current_task_id, updates, ExecutionActor.AI)
    
    def request_handoff(self, reason: str, to_actor: ExecutionActor):
        """Request handoff to specific actor"""
        if not self.current_task_id:
            return False
        
        if to_actor == ExecutionActor.AI:
            return self.ie_manager.handoff_to_ai(self.current_task_id, reason)
        elif to_actor == ExecutionActor.USER:
            return self.ie_manager.handoff_to_user(self.current_task_id, reason)
        
        return False
    
    def get_current_state(self) -> Dict[str, Any]:
        """Get current execution state"""
        if not self.current_task_id:
            return {}
        
        return self.ie_manager.get_execution_summary(self.current_task_id)
    
    def _handle_context_change(self, event_type: str, context: ExecutionContext):
        """Handle context changes"""
        print(f"📝 Context change: {event_type}")
        print(f"   Task: {context.task_name}")
        print(f"   Actor: {context.current_actor.value}")
        print(f"   State: {context.state.value}")
        
        if event_type == 'handoff_to_ai':
            print("🤖 AI is now in control")
        elif event_type == 'handoff_to_user':
            print("👤 User is now in control")

# Demo usage
if __name__ == "__main__":
    print("🔄 Interchangeable Execution (IE) System Demo")
    print("=" * 50)
    
    # Create IE system
    ie_ide = IDEInterchangeableExecution()
    
    # Start coding session
    task_id = ie_ide.start_coding_session("Create a Python web scraper", ExecutionActor.USER)
    
    # Simulate user coding
    print("\n👤 User starts coding...")
    ie_ide.user_types_code("import requests\nfrom bs4 import BeautifulSoup", "scraper.py")
    
    # User requests AI help
    print("\n👤 User requests AI assistance...")
    ie_ide.request_handoff("Need help with error handling", ExecutionActor.AI)
    
    # AI takes over
    print("\n🤖 AI takes over...")
    ie_ide.ai_suggests_code("try:\n    response = requests.get(url)\nexcept requests.RequestException as e:\n    print(f'Error: {e}')", "Added error handling")
    
    # AI hands back to user
    print("\n🤖 AI hands back to user...")
    ie_ide.request_handoff("Error handling implemented, ready for user review", ExecutionActor.USER)
    
    # Show final state
    print("\n📊 Final execution state:")
    state = ie_ide.get_current_state()
    for key, value in state.items():
        print(f"   {key}: {value}")
    
    print("\n✅ IE System Demo Complete!")
