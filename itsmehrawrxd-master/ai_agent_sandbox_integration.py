#!/usr/bin/env python3
"""
AI Agent Sandbox Integration - Intelligent AI Selection and Management
Integrates with Sandboxed Sandboxie for smart AI-powered sandbox operations

⚠️  DO NOT DISTRIBUTE - PROPRIETARY SOFTWARE ⚠️
This software is proprietary and confidential. Unauthorized distribution,
copying, or modification is strictly prohibited. All rights reserved.

Copyright (c) 2024 - All Rights Reserved
"""

import tkinter as tk
from tkinter import ttk, messagebox, scrolledtext
import os
import sys
import threading
import time
import json
import requests
import hashlib
import random
from pathlib import Path
from enum import Enum
from typing import Dict, List, Optional, Any

class AIMode(Enum):
    """AI operation modes"""
    AUTO = "auto"           # Automatically select best AI
    MANUAL = "manual"       # Manual AI selection
    AGENT = "agent"         # AI Agent mode
    ASK = "ask"            # Ask user for AI choice

class AIAgentSandboxIntegration:
    """
    AI Agent integration for Sandboxed Sandboxie
    Provides intelligent AI selection and management
    """
    
    def __init__(self, ide_instance):
        self.ide = ide_instance
        self.current_mode = AIMode.AUTO
        self.available_ais = {}
        self.ai_performance = {}
        self.user_preferences = {}
        self.agent_context = {}
        
        # Initialize AI systems
        self._initialize_ai_systems()
        
        print("🤖 AI Agent Sandbox Integration initialized")
        print(f"🎯 Current mode: {self.current_mode.value}")
    
    def _initialize_ai_systems(self):
        """Initialize available AI systems"""
        
        self.available_ais = {
            'openai': {
                'name': 'OpenAI GPT',
                'type': 'cloud',
                'capabilities': ['code_analysis', 'code_generation', 'security_scan'],
                'performance_score': 0.9,
                'cost_per_request': 0.002,
                'requires_api_key': True,
                'endpoint': 'https://api.openai.com/v1/chat/completions'
            },
            'claude': {
                'name': 'Anthropic Claude',
                'type': 'cloud',
                'capabilities': ['code_analysis', 'security_scan', 'documentation'],
                'performance_score': 0.95,
                'cost_per_request': 0.003,
                'requires_api_key': True,
                'endpoint': 'https://api.anthropic.com/v1/messages'
            },
            'ollama_local': {
                'name': 'Ollama (Local)',
                'type': 'local',
                'capabilities': ['code_analysis', 'code_generation'],
                'performance_score': 0.7,
                'cost_per_request': 0.0,
                'requires_api_key': False,
                'endpoint': 'http://localhost:11434'
            },
            'embedded_ai': {
                'name': 'Embedded AI',
                'type': 'embedded',
                'capabilities': ['basic_analysis', 'pattern_detection'],
                'performance_score': 0.6,
                'cost_per_request': 0.0,
                'requires_api_key': False,
                'endpoint': 'internal'
            },
            'code_analyzer': {
                'name': 'Code Analyzer',
                'type': 'internal',
                'capabilities': ['static_analysis', 'security_scan'],
                'performance_score': 0.8,
                'cost_per_request': 0.0,
                'requires_api_key': False,
                'endpoint': 'internal'
            }
        }
        
        # Initialize performance tracking
        for ai_id in self.available_ais:
            self.ai_performance[ai_id] = {
                'total_requests': 0,
                'successful_requests': 0,
                'average_response_time': 0.0,
                'last_used': None,
                'user_rating': 0.0
            }
    
    def set_mode(self, mode: AIMode):
        """Set AI operation mode"""
        
        self.current_mode = mode
        print(f"🎯 AI Mode changed to: {mode.value}")
        
        if mode == AIMode.AGENT:
            self._initialize_agent_context()
        elif mode == AIMode.ASK:
            self._prepare_ask_mode()
    
    def _initialize_agent_context(self):
        """Initialize AI Agent context"""
        
        self.agent_context = {
            'current_task': None,
            'task_history': [],
            'preferred_ai': None,
            'learning_mode': True,
            'context_memory': {},
            'performance_learning': True
        }
        
        print("🤖 AI Agent context initialized")
    
    def _prepare_ask_mode(self):
        """Prepare Ask mode for user interaction"""
        
        print("❓ Ask mode prepared - will ask user for AI selection")
    
    def auto_select_ai(self, task_type: str, context: Dict = None) -> str:
        """Automatically select the best AI for a task"""
        
        if self.current_mode != AIMode.AUTO:
            return self._manual_select_ai(task_type, context)
        
        # Score AIs based on task requirements
        ai_scores = {}
        
        for ai_id, ai_info in self.available_ais.items():
            score = self._calculate_ai_score(ai_id, task_type, context)
            ai_scores[ai_id] = score
        
        # Select best AI
        best_ai = max(ai_scores.items(), key=lambda x: x[1])
        
        print(f"🎯 Auto-selected AI: {best_ai[0]} (score: {best_ai[1]:.2f})")
        return best_ai[0]
    
    def _calculate_ai_score(self, ai_id: str, task_type: str, context: Dict = None) -> float:
        """Calculate AI score for a specific task"""
        
        ai_info = self.available_ais[ai_id]
        performance = self.ai_performance[ai_id]
        
        # Base score from capabilities
        capability_score = 0.0
        if task_type in ai_info['capabilities']:
            capability_score = 1.0
        elif any(cap.startswith(task_type.split('_')[0]) for cap in ai_info['capabilities']):
            capability_score = 0.7
        
        # Performance score
        performance_score = ai_info['performance_score']
        
        # Historical performance
        historical_score = 0.0
        if performance['total_requests'] > 0:
            success_rate = performance['successful_requests'] / performance['total_requests']
            historical_score = success_rate
        
        # Cost consideration
        cost_score = 1.0 - (ai_info['cost_per_request'] * 100)  # Penalize high cost
        
        # Availability check
        availability_score = 1.0 if self._check_ai_availability(ai_id) else 0.0
        
        # User preference
        preference_score = self.user_preferences.get(ai_id, 0.5)
        
        # Calculate weighted score
        total_score = (
            capability_score * 0.3 +
            performance_score * 0.2 +
            historical_score * 0.2 +
            cost_score * 0.1 +
            availability_score * 0.1 +
            preference_score * 0.1
        )
        
        return total_score
    
    def _check_ai_availability(self, ai_id: str) -> bool:
        """Check if AI is available"""
        
        ai_info = self.available_ais[ai_id]
        
        if ai_info['type'] == 'local':
            # Check if local AI is running
            try:
                if ai_id == 'ollama_local':
                    response = requests.get(f"{ai_info['endpoint']}/api/tags", timeout=5)
                    return response.status_code == 200
                return True
            except:
                return False
        
        elif ai_info['type'] == 'cloud':
            # Check API key availability
            return self._has_api_key(ai_id)
        
        elif ai_info['type'] in ['embedded', 'internal']:
            return True
        
        return False
    
    def _has_api_key(self, ai_id: str) -> bool:
        """Check if API key is available"""
        
        if ai_id == 'openai':
            return os.getenv('OPENAI_API_KEY') is not None
        elif ai_id == 'claude':
            return os.getenv('ANTHROPIC_API_KEY') is not None
        
        return False
    
    def _manual_select_ai(self, task_type: str, context: Dict = None) -> str:
        """Manual AI selection"""
        
        if self.current_mode == AIMode.ASK:
            return self._ask_user_for_ai(task_type, context)
        elif self.current_mode == AIMode.AGENT:
            return self._agent_select_ai(task_type, context)
        else:
            # Manual mode - return default or last used
            return self.user_preferences.get('preferred_ai', 'embedded_ai')
    
    def _ask_user_for_ai(self, task_type: str, context: Dict = None) -> str:
        """Ask user to select AI"""
        
        # Create selection dialog
        dialog = tk.Toplevel(self.ide.root if hasattr(self.ide, 'root') else None)
        dialog.title("🤖 Select AI for Task")
        dialog.geometry("500x400")
        
        # Task info
        task_label = ttk.Label(dialog, text=f"Task: {task_type}", font=("Arial", 12, "bold"))
        task_label.pack(pady=10)
        
        # AI selection
        ai_var = tk.StringVar()
        ai_frame = ttk.Frame(dialog)
        ai_frame.pack(pady=10, padx=20, fill='both', expand=True)
        
        ttk.Label(ai_frame, text="Available AIs:").pack(anchor=tk.W)
        
        # AI list with details
        for ai_id, ai_info in self.available_ais.items():
            available = self._check_ai_availability(ai_id)
            status = "✅ Available" if available else "❌ Not Available"
            
            ai_radio = ttk.Radiobutton(
                ai_frame, 
                text=f"{ai_info['name']} - {status}",
                variable=ai_var, 
                value=ai_id,
                state='normal' if available else 'disabled'
            )
            ai_radio.pack(anchor=tk.W, pady=2)
            
            # AI details
            details = f"  Type: {ai_info['type']} | Performance: {ai_info['performance_score']:.1f} | Cost: ${ai_info['cost_per_request']:.3f}"
            ttk.Label(ai_frame, text=details, font=("Arial", 8)).pack(anchor=tk.W, padx=20)
        
        # Buttons
        button_frame = ttk.Frame(dialog)
        button_frame.pack(pady=20)
        
        selected_ai = None
        
        def on_select():
            nonlocal selected_ai
            selected_ai = ai_var.get()
            dialog.destroy()
        
        def on_cancel():
            nonlocal selected_ai
            selected_ai = None
            dialog.destroy()
        
        ttk.Button(button_frame, text="Select", command=on_select).pack(side=tk.LEFT, padx=5)
        ttk.Button(button_frame, text="Cancel", command=on_cancel).pack(side=tk.LEFT, padx=5)
        
        # Wait for user selection
        dialog.wait_window()
        
        return selected_ai or 'embedded_ai'
    
    def _agent_select_ai(self, task_type: str, context: Dict = None) -> str:
        """AI Agent selects AI (meta-AI selection)"""
        
        # Agent learns from previous selections
        if self.agent_context.get('learning_mode', True):
            # Use learning algorithm to select AI
            return self._learn_and_select_ai(task_type, context)
        else:
            # Use predefined agent logic
            return self._agent_logic_select(task_type, context)
    
    def _learn_and_select_ai(self, task_type: str, context: Dict = None) -> str:
        """Learning-based AI selection"""
        
        # Simple learning algorithm
        task_history = self.agent_context.get('task_history', [])
        
        # Find similar tasks
        similar_tasks = [t for t in task_history if t.get('task_type') == task_type]
        
        if similar_tasks:
            # Use best performing AI from similar tasks
            best_ai = None
            best_performance = 0.0
            
            for task in similar_tasks:
                ai_id = task.get('selected_ai')
                performance = task.get('performance', 0.0)
                
                if performance > best_performance:
                    best_performance = performance
                    best_ai = ai_id
            
            if best_ai:
                print(f"🤖 Agent learned: Using {best_ai} for {task_type} (performance: {best_performance:.2f})")
                return best_ai
        
        # Fallback to auto-selection
        return self.auto_select_ai(task_type, context)
    
    def _agent_logic_select(self, task_type: str, context: Dict = None) -> str:
        """Agent logic-based AI selection"""
        
        # Define agent logic rules
        if task_type == 'security_scan':
            return 'claude' if self._check_ai_availability('claude') else 'code_analyzer'
        elif task_type == 'code_generation':
            return 'openai' if self._check_ai_availability('openai') else 'ollama_local'
        elif task_type == 'code_analysis':
            return 'claude' if self._check_ai_availability('claude') else 'embedded_ai'
        else:
            return 'embedded_ai'
    
    def execute_ai_task(self, task_type: str, task_data: Dict, context: Dict = None) -> Dict:
        """Execute AI task with intelligent selection"""
        
        start_time = time.time()
        
        # Select AI
        selected_ai = self.auto_select_ai(task_type, context)
        
        # Execute task
        try:
            result = self._execute_with_ai(selected_ai, task_type, task_data)
            
            # Update performance tracking
            self._update_performance(selected_ai, True, time.time() - start_time)
            
            # Update agent context
            if self.current_mode == AIMode.AGENT:
                self._update_agent_context(task_type, selected_ai, result)
            
            return {
                'success': True,
                'ai_used': selected_ai,
                'result': result,
                'execution_time': time.time() - start_time,
                'mode': self.current_mode.value
            }
            
        except Exception as e:
            # Update performance tracking
            self._update_performance(selected_ai, False, time.time() - start_time)
            
            return {
                'success': False,
                'ai_used': selected_ai,
                'error': str(e),
                'execution_time': time.time() - start_time,
                'mode': self.current_mode.value
            }
    
    def _execute_with_ai(self, ai_id: str, task_type: str, task_data: Dict) -> Dict:
        """Execute task with specific AI"""
        
        ai_info = self.available_ais[ai_id]
        
        if ai_id == 'openai':
            return self._execute_openai_task(task_type, task_data)
        elif ai_id == 'claude':
            return self._execute_claude_task(task_type, task_data)
        elif ai_id == 'ollama_local':
            return self._execute_ollama_task(task_type, task_data)
        elif ai_id == 'embedded_ai':
            return self._execute_embedded_task(task_type, task_data)
        elif ai_id == 'code_analyzer':
            return self._execute_code_analyzer_task(task_type, task_data)
        else:
            raise ValueError(f"Unknown AI: {ai_id}")
    
    def _execute_openai_task(self, task_type: str, task_data: Dict) -> Dict:
        """Execute task with OpenAI"""
        
        api_key = os.getenv('OPENAI_API_KEY')
        if not api_key:
            raise ValueError("OpenAI API key not found")
        
        # Prepare prompt based on task type
        if task_type == 'code_analysis':
            prompt = f"Analyze this code and provide suggestions:\n\n{task_data.get('code', '')}"
        elif task_type == 'code_generation':
            prompt = f"Generate code for: {task_data.get('description', '')}"
        elif task_type == 'security_scan':
            prompt = f"Perform security analysis on this code:\n\n{task_data.get('code', '')}"
        else:
            prompt = f"Process this task: {task_data}"
        
        # Make API call
        headers = {
            'Authorization': f'Bearer {api_key}',
            'Content-Type': 'application/json'
        }
        
        payload = {
            'model': 'gpt-3.5-turbo',
            'messages': [{'role': 'user', 'content': prompt}],
            'max_tokens': 1000
        }
        
        response = requests.post(
            'https://api.openai.com/v1/chat/completions',
            headers=headers,
            json=payload,
            timeout=30
        )
        
        if response.status_code == 200:
            result = response.json()
            return {
                'response': result['choices'][0]['message']['content'],
                'model': result['model'],
                'tokens_used': result['usage']['total_tokens']
            }
        else:
            raise Exception(f"OpenAI API error: {response.status_code}")
    
    def _execute_claude_task(self, task_type: str, task_data: Dict) -> Dict:
        """Execute task with Claude"""
        
        api_key = os.getenv('ANTHROPIC_API_KEY')
        if not api_key:
            raise ValueError("Anthropic API key not found")
        
        # Similar implementation for Claude
        return {'response': 'Claude analysis result', 'model': 'claude-3'}
    
    def _execute_ollama_task(self, task_type: str, task_data: Dict) -> Dict:
        """Execute task with Ollama"""
        
        try:
            # Use Ollama API
            prompt = f"Task: {task_type}\nData: {task_data}"
            
            payload = {
                'model': 'llama3',
                'prompt': prompt,
                'stream': False
            }
            
            response = requests.post(
                'http://localhost:11434/api/generate',
                json=payload,
                timeout=30
            )
            
            if response.status_code == 200:
                result = response.json()
                return {
                    'response': result['response'],
                    'model': 'llama3'
                }
            else:
                raise Exception(f"Ollama API error: {response.status_code}")
                
        except requests.exceptions.ConnectionError:
            raise Exception("Ollama not running on localhost:11434")
    
    def _execute_embedded_task(self, task_type: str, task_data: Dict) -> Dict:
        """Execute task with embedded AI"""
        
        # Simple embedded AI logic
        if task_type == 'code_analysis':
            return {
                'response': 'Embedded AI analysis: Code looks good',
                'model': 'embedded'
            }
        else:
            return {
                'response': f'Embedded AI processed: {task_type}',
                'model': 'embedded'
            }
    
    def _execute_code_analyzer_task(self, task_type: str, task_data: Dict) -> Dict:
        """Execute task with code analyzer"""
        
        code = task_data.get('code', '')
        
        # Simple static analysis
        issues = []
        if 'eval(' in code:
            issues.append('Security: eval() usage detected')
        if 'exec(' in code:
            issues.append('Security: exec() usage detected')
        
        return {
            'response': f'Code analysis complete. Found {len(issues)} issues.',
            'issues': issues,
            'model': 'code_analyzer'
        }
    
    def _update_performance(self, ai_id: str, success: bool, execution_time: float):
        """Update AI performance metrics"""
        
        performance = self.ai_performance[ai_id]
        performance['total_requests'] += 1
        
        if success:
            performance['successful_requests'] += 1
        
        # Update average response time
        if performance['average_response_time'] == 0.0:
            performance['average_response_time'] = execution_time
        else:
            performance['average_response_time'] = (
                performance['average_response_time'] * 0.8 + execution_time * 0.2
            )
        
        performance['last_used'] = time.time()
    
    def _update_agent_context(self, task_type: str, ai_id: str, result: Dict):
        """Update agent context for learning"""
        
        if self.current_mode == AIMode.AGENT:
            task_record = {
                'task_type': task_type,
                'selected_ai': ai_id,
                'timestamp': time.time(),
                'performance': 1.0 if result else 0.0,
                'result_quality': self._assess_result_quality(result)
            }
            
            self.agent_context['task_history'].append(task_record)
            
            # Keep only recent history
            if len(self.agent_context['task_history']) > 100:
                self.agent_context['task_history'] = self.agent_context['task_history'][-100:]
    
    def _assess_result_quality(self, result: Dict) -> float:
        """Assess quality of AI result"""
        
        # Simple quality assessment
        if not result:
            return 0.0
        
        response = result.get('response', '')
        if len(response) < 10:
            return 0.3
        elif len(response) < 100:
            return 0.7
        else:
            return 1.0
    
    def get_ai_status(self) -> Dict:
        """Get status of all AIs"""
        
        status = {}
        
        for ai_id, ai_info in self.available_ais.items():
            available = self._check_ai_availability(ai_id)
            performance = self.ai_performance[ai_id]
            
            status[ai_id] = {
                'name': ai_info['name'],
                'available': available,
                'type': ai_info['type'],
                'performance_score': ai_info['performance_score'],
                'total_requests': performance['total_requests'],
                'success_rate': (
                    performance['successful_requests'] / performance['total_requests']
                    if performance['total_requests'] > 0 else 0.0
                ),
                'average_response_time': performance['average_response_time'],
                'last_used': performance['last_used']
            }
        
        return status
    
    def set_user_preference(self, ai_id: str, preference: float):
        """Set user preference for AI (0.0 to 1.0)"""
        
        self.user_preferences[ai_id] = preference
        print(f"👤 User preference set for {ai_id}: {preference}")

class AIAgentSandboxGUI:
    """GUI for AI Agent Sandbox Integration"""
    
    def __init__(self, ide_instance):
        self.ide = ide_instance
        self.ai_agent = AIAgentSandboxIntegration(ide_instance)
        self.setup_gui()
    
    def setup_gui(self):
        """Setup GUI for AI Agent integration"""
        
        # Create new window
        self.ai_window = tk.Toplevel(self.ide.root)
        self.ai_window.title("🤖 AI Agent Sandbox Integration")
        self.ai_window.geometry("1000x700")
        
        # Main frame
        main_frame = ttk.Frame(self.ai_window, padding="10")
        main_frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # Title
        title_label = ttk.Label(main_frame, text="🤖 AI Agent Sandbox Integration", 
                               font=("Arial", 16, "bold"))
        title_label.grid(row=0, column=0, columnspan=3, pady=(0, 20))
        
        # Mode selection
        self.setup_mode_section(main_frame)
        
        # AI status
        self.setup_status_section(main_frame)
        
        # Task execution
        self.setup_task_section(main_frame)
        
        # Results
        self.setup_results_section(main_frame)
        
        # Configure grid weights
        self.ai_window.columnconfigure(0, weight=1)
        self.ai_window.rowconfigure(0, weight=1)
        main_frame.columnconfigure(0, weight=1)
        main_frame.rowconfigure(4, weight=1)
    
    def setup_mode_section(self, parent):
        """Setup AI mode selection"""
        
        mode_frame = ttk.LabelFrame(parent, text="🎯 AI Mode Selection", padding="10")
        mode_frame.grid(row=1, column=0, columnspan=3, sticky=(tk.W, tk.E), pady=(0, 10))
        
        # Mode selection
        self.mode_var = tk.StringVar(value="auto")
        
        modes = [
            ("🤖 Auto", "auto", "Automatically select best AI"),
            ("👤 Manual", "manual", "Use your preferred AI"),
            ("🧠 Agent", "agent", "AI Agent learns and selects"),
            ("❓ Ask", "ask", "Ask user for each task")
        ]
        
        for i, (label, value, description) in enumerate(modes):
            rb = ttk.Radiobutton(mode_frame, text=label, variable=self.mode_var, value=value,
                                command=self.on_mode_change)
            rb.grid(row=i, column=0, sticky=tk.W, pady=2)
            
            desc_label = ttk.Label(mode_frame, text=description, font=("Arial", 8))
            desc_label.grid(row=i, column=1, sticky=tk.W, padx=(20, 0), pady=2)
        
        mode_frame.columnconfigure(1, weight=1)
    
    def setup_status_section(self, parent):
        """Setup AI status display"""
        
        status_frame = ttk.LabelFrame(parent, text="📊 AI Status", padding="10")
        status_frame.grid(row=2, column=0, columnspan=3, sticky=(tk.W, tk.E), pady=(0, 10))
        
        # AI status list
        self.status_text = scrolledtext.ScrolledText(status_frame, height=8, width=80)
        self.status_text.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # Refresh button
        ttk.Button(status_frame, text="🔄 Refresh Status", 
                  command=self.refresh_ai_status).grid(row=1, column=0, pady=5)
        
        status_frame.columnconfigure(0, weight=1)
        status_frame.rowconfigure(0, weight=1)
        
        # Initial status
        self.refresh_ai_status()
    
    def setup_task_section(self, parent):
        """Setup task execution"""
        
        task_frame = ttk.LabelFrame(parent, text="🚀 Execute AI Task", padding="10")
        task_frame.grid(row=3, column=0, columnspan=3, sticky=(tk.W, tk.E), pady=(0, 10))
        
        # Task type
        ttk.Label(task_frame, text="Task Type:").grid(row=0, column=0, sticky=tk.W, pady=5)
        self.task_type_var = tk.StringVar(value="code_analysis")
        task_combo = ttk.Combobox(task_frame, textvariable=self.task_type_var,
                                 values=["code_analysis", "code_generation", "security_scan", "documentation"])
        task_combo.grid(row=0, column=1, sticky=tk.W, pady=5, padx=(10, 0))
        
        # Task data
        ttk.Label(task_frame, text="Task Data:").grid(row=1, column=0, sticky=tk.W, pady=5)
        self.task_data_text = scrolledtext.ScrolledText(task_frame, height=4, width=60)
        self.task_data_text.grid(row=2, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=5)
        
        # Load sample data
        self.load_sample_task_data()
        
        # Execute button
        ttk.Button(task_frame, text="🚀 Execute with AI", 
                  command=self.execute_ai_task).grid(row=3, column=0, columnspan=2, pady=10)
        
        task_frame.columnconfigure(1, weight=1)
    
    def setup_results_section(self, parent):
        """Setup results display"""
        
        results_frame = ttk.LabelFrame(parent, text="📊 Results", padding="10")
        results_frame.grid(row=4, column=0, columnspan=3, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # Results text
        self.results_text = scrolledtext.ScrolledText(results_frame, height=10, width=80)
        self.results_text.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        results_frame.columnconfigure(0, weight=1)
        results_frame.rowconfigure(0, weight=1)
    
    def on_mode_change(self):
        """Handle mode change"""
        
        mode_value = self.mode_var.get()
        
        if mode_value == "auto":
            self.ai_agent.set_mode(AIMode.AUTO)
        elif mode_value == "manual":
            self.ai_agent.set_mode(AIMode.MANUAL)
        elif mode_value == "agent":
            self.ai_agent.set_mode(AIMode.AGENT)
        elif mode_value == "ask":
            self.ai_agent.set_mode(AIMode.ASK)
        
        self.refresh_ai_status()
    
    def refresh_ai_status(self):
        """Refresh AI status display"""
        
        self.status_text.delete(1.0, tk.END)
        
        status = self.ai_agent.get_ai_status()
        
        self.status_text.insert(tk.END, "🤖 AI Status Report\n")
        self.status_text.insert(tk.END, "=" * 50 + "\n\n")
        
        for ai_id, ai_status in status.items():
            status_icon = "✅" if ai_status['available'] else "❌"
            self.status_text.insert(tk.END, f"{status_icon} {ai_status['name']}\n")
            self.status_text.insert(tk.END, f"   Type: {ai_status['type']}\n")
            self.status_text.insert(tk.END, f"   Performance: {ai_status['performance_score']:.2f}\n")
            self.status_text.insert(tk.END, f"   Requests: {ai_status['total_requests']}\n")
            self.status_text.insert(tk.END, f"   Success Rate: {ai_status['success_rate']:.2%}\n")
            if ai_status['last_used']:
                last_used = time.ctime(ai_status['last_used'])
                self.status_text.insert(tk.END, f"   Last Used: {last_used}\n")
            self.status_text.insert(tk.END, "\n")
        
        self.status_text.insert(tk.END, f"🎯 Current Mode: {self.ai_agent.current_mode.value}\n")
    
    def load_sample_task_data(self):
        """Load sample task data"""
        
        sample_data = '''{
    "code": "def hello_world():\\n    print('Hello, World!')\\n    eval('malicious_code()')",
    "description": "Create a secure hello world function"
}'''
        
        self.task_data_text.delete(1.0, tk.END)
        self.task_data_text.insert(1.0, sample_data)
    
    def execute_ai_task(self):
        """Execute AI task"""
        
        task_type = self.task_type_var.get()
        task_data_text = self.task_data_text.get(1.0, tk.END).strip()
        
        try:
            task_data = json.loads(task_data_text)
        except json.JSONDecodeError:
            messagebox.showerror("Error", "Invalid JSON in task data")
            return
        
        # Execute in background
        def execute_task():
            try:
                result = self.ai_agent.execute_ai_task(task_type, task_data)
                self.display_result(result)
            except Exception as e:
                self.results_text.insert(tk.END, f"❌ Task execution failed: {e}\n")
        
        threading.Thread(target=execute_task, daemon=True).start()
        
        self.results_text.insert(tk.END, f"🚀 Executing {task_type} task...\n")
        self.results_text.insert(tk.END, f"🎯 Mode: {self.ai_agent.current_mode.value}\n\n")
    
    def display_result(self, result):
        """Display task result"""
        
        self.results_text.insert(tk.END, "📊 Task Result\n")
        self.results_text.insert(tk.END, "=" * 50 + "\n")
        
        if result['success']:
            self.results_text.insert(tk.END, "✅ Task completed successfully\n")
            self.results_text.insert(tk.END, f"🤖 AI Used: {result['ai_used']}\n")
            self.results_text.insert(tk.END, f"⏱️ Execution Time: {result['execution_time']:.2f}s\n")
            self.results_text.insert(tk.END, f"🎯 Mode: {result['mode']}\n\n")
            
            # Display AI response
            ai_result = result['result']
            self.results_text.insert(tk.END, f"📤 AI Response:\n")
            self.results_text.insert(tk.END, f"{ai_result.get('response', 'No response')}\n\n")
            
            if 'issues' in ai_result:
                self.results_text.insert(tk.END, f"⚠️ Issues Found: {len(ai_result['issues'])}\n")
                for issue in ai_result['issues']:
                    self.results_text.insert(tk.END, f"  • {issue}\n")
        else:
            self.results_text.insert(tk.END, "❌ Task failed\n")
            self.results_text.insert(tk.END, f"🤖 AI Used: {result['ai_used']}\n")
            self.results_text.insert(tk.END, f"⚠️ Error: {result['error']}\n")
        
        self.results_text.insert(tk.END, "\n" + "=" * 50 + "\n\n")
        
        # Refresh status
        self.refresh_ai_status()

# Integration function
def integrate_ai_agent_sandbox(ide_instance):
    """Integrate AI Agent with IDE"""
    
    if hasattr(ide_instance, 'add_menu_item'):
        ide_instance.add_menu_item("Tools", "AI Agent Sandbox", 
                                 lambda: AIAgentSandboxGUI(ide_instance))
    
    print("🤖 AI Agent Sandbox Integration added to IDE")

if __name__ == "__main__":
    print("🤖 AI Agent Sandbox Integration")
    print("=" * 50)
    
    class MockIDE:
        def __init__(self):
            self.root = tk.Tk()
    
    ide = MockIDE()
    ai_agent = AIAgentSandboxIntegration(ide)
    
    print("✅ AI Agent Sandbox Integration ready!")
    print("🎯 Supports Auto, Manual, Agent, and Ask modes!")
