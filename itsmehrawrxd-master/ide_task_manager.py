#!/usr/bin/env python3
"""
IDE Task Manager
Comprehensive task and process management for the IDE
Monitors AI sessions, memory usage, system resources, and performance

⚠️  DO NOT DISTRIBUTE - PROPRIETARY SOFTWARE ⚠️
This software is proprietary and confidential. Unauthorized distribution,
copying, or modification is strictly prohibited. All rights reserved.

Copyright (c) 2024 - All Rights Reserved
"""

import threading
import time
import psutil
import os
import sys
import json
from typing import Dict, List, Any, Optional, Tuple
from dataclasses import dataclass, asdict
from enum import Enum
import queue
from collections import defaultdict
import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox
import subprocess

class TaskType(Enum):
    """Types of tasks"""
    AI_SESSION = "ai_session"
    COMPILATION = "compilation"
    DEBUGGING = "debugging"
    TESTING = "testing"
    BUILD = "build"
    DEPLOYMENT = "deployment"
    BACKGROUND = "background"
    SYSTEM = "system"

class TaskStatus(Enum):
    """Task status"""
    RUNNING = "running"
    IDLE = "idle"
    WAITING = "waiting"
    COMPLETED = "completed"
    ERROR = "error"
    PAUSED = "paused"
    TERMINATED = "terminated"

class TaskPriority(Enum):
    """Task priority"""
    CRITICAL = 1
    HIGH = 2
    MEDIUM = 3
    LOW = 4
    BACKGROUND = 5

@dataclass
class TaskInfo:
    """Task information"""
    task_id: str
    name: str
    task_type: TaskType
    status: TaskStatus
    priority: TaskPriority
    created_at: float
    started_at: float = 0
    completed_at: float = 0
    
    # Resource usage
    cpu_percent: float = 0.0
    memory_mb: float = 0.0
    memory_percent: float = 0.0
    
    # Process info
    process_id: int = 0
    parent_id: int = 0
    thread_count: int = 0
    
    # Performance metrics
    response_time: float = 0.0
    throughput: float = 0.0
    error_count: int = 0
    
    # Additional info
    description: str = ""
    tags: List[str] = None
    dependencies: List[str] = None
    
    def __post_init__(self):
        if self.tags is None:
            self.tags = []
        if self.dependencies is None:
            self.dependencies = []

class IDETaskManager:
    """Comprehensive IDE task manager"""
    
    def __init__(self):
        self.tasks: Dict[str, TaskInfo] = {}
        self.processes: Dict[int, psutil.Process] = {}
        self.monitoring_thread = None
        self.cleanup_thread = None
        self.performance_data = defaultdict(list)
        
        # System monitoring
        self.system_stats = {
            'cpu_percent': 0.0,
            'memory_percent': 0.0,
            'disk_usage': 0.0,
            'network_io': {'bytes_sent': 0, 'bytes_recv': 0},
            'process_count': 0
        }
        
        # Performance thresholds
        self.thresholds = {
            'cpu_warning': 80.0,
            'memory_warning': 85.0,
            'disk_warning': 90.0,
            'response_time_warning': 5.0
        }
        
        # Start monitoring
        self._start_monitoring()
    
    def _start_monitoring(self):
        """Start background monitoring"""
        self.monitoring_thread = threading.Thread(
            target=self._monitoring_loop, 
            daemon=True
        )
        self.monitoring_thread.start()
        
        self.cleanup_thread = threading.Thread(
            target=self._cleanup_loop, 
            daemon=True
        )
        self.cleanup_thread.start()
    
    def create_task(self, name: str, task_type: TaskType, 
                   priority: TaskPriority = TaskPriority.MEDIUM,
                   description: str = "", tags: List[str] = None) -> str:
        """Create a new task"""
        task_id = f"{task_type.value}_{int(time.time() * 1000)}"
        
        task = TaskInfo(
            task_id=task_id,
            name=name,
            task_type=task_type,
            status=TaskStatus.IDLE,
            priority=priority,
            created_at=time.time(),
            description=description,
            tags=tags or []
        )
        
        self.tasks[task_id] = task
        print(f"📋 Created task: {name} ({task_id})")
        
        return task_id
    
    def start_task(self, task_id: str, process_id: int = None) -> bool:
        """Start a task"""
        if task_id not in self.tasks:
            return False
        
        task = self.tasks[task_id]
        task.status = TaskStatus.RUNNING
        task.started_at = time.time()
        
        if process_id:
            task.process_id = process_id
            try:
                self.processes[process_id] = psutil.Process(process_id)
            except psutil.NoSuchProcess:
                pass
        
        print(f"▶️ Started task: {task.name}")
        return True
    
    def pause_task(self, task_id: str) -> bool:
        """Pause a task"""
        if task_id not in self.tasks:
            return False
        
        task = self.tasks[task_id]
        if task.status == TaskStatus.RUNNING:
            task.status = TaskStatus.PAUSED
            print(f"⏸️ Paused task: {task.name}")
            return True
        
        return False
    
    def resume_task(self, task_id: str) -> bool:
        """Resume a paused task"""
        if task_id not in self.tasks:
            return False
        
        task = self.tasks[task_id]
        if task.status == TaskStatus.PAUSED:
            task.status = TaskStatus.RUNNING
            print(f"▶️ Resumed task: {task.name}")
            return True
        
        return False
    
    def complete_task(self, task_id: str, success: bool = True) -> bool:
        """Complete a task"""
        if task_id not in self.tasks:
            return False
        
        task = self.tasks[task_id]
        task.status = TaskStatus.COMPLETED if success else TaskStatus.ERROR
        task.completed_at = time.time()
        
        print(f"✅ Completed task: {task.name} ({'success' if success else 'error'})")
        return True
    
    def terminate_task(self, task_id: str) -> bool:
        """Terminate a task"""
        if task_id not in self.tasks:
            return False
        
        task = self.tasks[task_id]
        
        # Terminate process if exists
        if task.process_id and task.process_id in self.processes:
            try:
                process = self.processes[task.process_id]
                process.terminate()
                del self.processes[task.process_id]
            except psutil.NoSuchProcess:
                pass
        
        task.status = TaskStatus.TERMINATED
        task.completed_at = time.time()
        
        print(f"🛑 Terminated task: {task.name}")
        return True
    
    def get_task_status(self, task_id: str) -> Optional[Dict[str, Any]]:
        """Get task status"""
        if task_id not in self.tasks:
            return None
        
        task = self.tasks[task_id]
        
        return {
            'task_id': task_id,
            'name': task.name,
            'type': task.task_type.value,
            'status': task.status.value,
            'priority': task.priority.value,
            'created_at': task.created_at,
            'started_at': task.started_at,
            'completed_at': task.completed_at,
            'duration': time.time() - task.created_at,
            'cpu_percent': task.cpu_percent,
            'memory_mb': task.memory_mb,
            'memory_percent': task.memory_percent,
            'process_id': task.process_id,
            'thread_count': task.thread_count,
            'response_time': task.response_time,
            'throughput': task.throughput,
            'error_count': task.error_count,
            'description': task.description,
            'tags': task.tags
        }
    
    def get_all_tasks_status(self) -> Dict[str, Any]:
        """Get status of all tasks"""
        total_tasks = len(self.tasks)
        running_tasks = len([t for t in self.tasks.values() if t.status == TaskStatus.RUNNING])
        idle_tasks = len([t for t in self.tasks.values() if t.status == TaskStatus.IDLE])
        completed_tasks = len([t for t in self.tasks.values() if t.status == TaskStatus.COMPLETED])
        error_tasks = len([t for t in self.tasks.values() if t.status == TaskStatus.ERROR])
        
        # Calculate total resource usage
        total_cpu = sum(t.cpu_percent for t in self.tasks.values())
        total_memory = sum(t.memory_mb for t in self.tasks.values())
        
        return {
            'total_tasks': total_tasks,
            'running_tasks': running_tasks,
            'idle_tasks': idle_tasks,
            'completed_tasks': completed_tasks,
            'error_tasks': error_tasks,
            'total_cpu_percent': total_cpu,
            'total_memory_mb': total_memory,
            'system_stats': self.system_stats,
            'performance_data': dict(self.performance_data)
        }
    
    def _monitoring_loop(self):
        """Background monitoring loop"""
        while True:
            try:
                # Update system stats
                self._update_system_stats()
                
                # Update task stats
                self._update_task_stats()
                
                # Check for warnings
                self._check_performance_warnings()
                
                time.sleep(2)  # Monitor every 2 seconds
                
            except Exception as e:
                print(f"Task monitoring error: {e}")
                time.sleep(5)
    
    def _update_system_stats(self):
        """Update system statistics"""
        try:
            # CPU usage
            self.system_stats['cpu_percent'] = psutil.cpu_percent(interval=1)
            
            # Memory usage
            memory = psutil.virtual_memory()
            self.system_stats['memory_percent'] = memory.percent
            
            # Disk usage
            disk = psutil.disk_usage('/')
            self.system_stats['disk_usage'] = (disk.used / disk.total) * 100
            
            # Network I/O
            net_io = psutil.net_io_counters()
            self.system_stats['network_io'] = {
                'bytes_sent': net_io.bytes_sent,
                'bytes_recv': net_io.bytes_recv
            }
            
            # Process count
            self.system_stats['process_count'] = len(psutil.pids())
            
        except Exception as e:
            print(f"Error updating system stats: {e}")
    
    def _update_task_stats(self):
        """Update task statistics"""
        for task in self.tasks.values():
            if task.process_id and task.process_id in self.processes:
                try:
                    process = self.processes[task.process_id]
                    
                    # Update CPU and memory
                    task.cpu_percent = process.cpu_percent()
                    memory_info = process.memory_info()
                    task.memory_mb = memory_info.rss / (1024 * 1024)
                    task.memory_percent = (memory_info.rss / psutil.virtual_memory().total) * 100
                    
                    # Update thread count
                    task.thread_count = process.num_threads()
                    
                    # Calculate response time
                    if task.started_at > 0:
                        task.response_time = time.time() - task.started_at
                    
                except psutil.NoSuchProcess:
                    # Process no longer exists
                    task.status = TaskStatus.TERMINATED
                    if task.process_id in self.processes:
                        del self.processes[task.process_id]
    
    def _check_performance_warnings(self):
        """Check for performance warnings"""
        warnings = []
        
        # System warnings
        if self.system_stats['cpu_percent'] > self.thresholds['cpu_warning']:
            warnings.append(f"High CPU usage: {self.system_stats['cpu_percent']:.1f}%")
        
        if self.system_stats['memory_percent'] > self.thresholds['memory_warning']:
            warnings.append(f"High memory usage: {self.system_stats['memory_percent']:.1f}%")
        
        if self.system_stats['disk_usage'] > self.thresholds['disk_warning']:
            warnings.append(f"High disk usage: {self.system_stats['disk_usage']:.1f}%")
        
        # Task warnings
        for task in self.tasks.values():
            if task.status == TaskStatus.RUNNING:
                if task.response_time > self.thresholds['response_time_warning']:
                    warnings.append(f"Slow task: {task.name} ({task.response_time:.1f}s)")
                
                if task.cpu_percent > 90:
                    warnings.append(f"High CPU task: {task.name} ({task.cpu_percent:.1f}%)")
                
                if task.memory_percent > 10:
                    warnings.append(f"High memory task: {task.name} ({task.memory_mb:.1f}MB)")
        
        # Log warnings
        for warning in warnings:
            print(f"⚠️ {warning}")
    
    def _cleanup_loop(self):
        """Background cleanup loop"""
        while True:
            try:
                # Clean up completed tasks older than 1 hour
                current_time = time.time()
                tasks_to_remove = []
                
                for task_id, task in self.tasks.items():
                    if (task.status in [TaskStatus.COMPLETED, TaskStatus.TERMINATED] and
                        current_time - task.completed_at > 3600):  # 1 hour
                        tasks_to_remove.append(task_id)
                
                for task_id in tasks_to_remove:
                    del self.tasks[task_id]
                
                if tasks_to_remove:
                    print(f"🧹 Cleaned up {len(tasks_to_remove)} old tasks")
                
                time.sleep(300)  # Cleanup every 5 minutes
                
            except Exception as e:
                print(f"Task cleanup error: {e}")
                time.sleep(600)
    
    def get_performance_report(self) -> Dict[str, Any]:
        """Get comprehensive performance report"""
        return {
            'system_performance': {
                'cpu_percent': self.system_stats['cpu_percent'],
                'memory_percent': self.system_stats['memory_percent'],
                'disk_usage': self.system_stats['disk_usage'],
                'process_count': self.system_stats['process_count']
            },
            'task_performance': {
                'total_tasks': len(self.tasks),
                'active_tasks': len([t for t in self.tasks.values() if t.status == TaskStatus.RUNNING]),
                'avg_cpu_percent': sum(t.cpu_percent for t in self.tasks.values()) / max(len(self.tasks), 1),
                'avg_memory_mb': sum(t.memory_mb for t in self.tasks.values()) / max(len(self.tasks), 1),
                'total_memory_mb': sum(t.memory_mb for t in self.tasks.values())
            },
            'recommendations': self._get_performance_recommendations()
        }
    
    def _get_performance_recommendations(self) -> List[str]:
        """Get performance optimization recommendations"""
        recommendations = []
        
        # System recommendations
        if self.system_stats['cpu_percent'] > 80:
            recommendations.append("Consider reducing CPU-intensive tasks")
        
        if self.system_stats['memory_percent'] > 85:
            recommendations.append("Consider freeing up memory or reducing memory usage")
        
        if self.system_stats['disk_usage'] > 90:
            recommendations.append("Consider cleaning up disk space")
        
        # Task recommendations
        slow_tasks = [t for t in self.tasks.values() 
                     if t.status == TaskStatus.RUNNING and t.response_time > 10]
        if slow_tasks:
            recommendations.append(f"Consider optimizing {len(slow_tasks)} slow tasks")
        
        high_memory_tasks = [t for t in self.tasks.values() 
                           if t.memory_mb > 100]
        if high_memory_tasks:
            recommendations.append(f"Consider optimizing {len(high_memory_tasks)} memory-intensive tasks")
        
        return recommendations

class TaskManagerGUI:
    """GUI for IDE Task Manager"""
    
    def __init__(self, parent):
        self.parent = parent
        self.task_manager = IDETaskManager()
        self.refresh_job = None
        
    def show_task_manager(self):
        """Show task manager window"""
        task_window = tk.Toplevel(self.parent)
        task_window.title("IDE Task Manager")
        task_window.geometry("1000x700")
        task_window.configure(bg='#2d2d30')
        
        # Header
        header_frame = ttk.Frame(task_window)
        header_frame.pack(fill=tk.X, padx=10, pady=10)
        
        ttk.Label(header_frame, text="IDE Task Manager", 
                 font=('Segoe UI', 16, 'bold')).pack()
        ttk.Label(header_frame, text="Monitor and manage all IDE processes, AI sessions, and system resources", 
                 font=('Segoe UI', 10)).pack()
        
        # System status
        system_frame = ttk.LabelFrame(task_window, text="System Status")
        system_frame.pack(fill=tk.X, padx=10, pady=5)
        
        self.system_status_text = scrolledtext.ScrolledText(
            system_frame, height=6, wrap=tk.WORD,
            bg='#1e1e1e', fg='#ffffff', font=('Consolas', 9)
        )
        self.system_status_text.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        # Tasks list
        tasks_frame = ttk.LabelFrame(task_window, text="Active Tasks")
        tasks_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)
        
        # Create treeview for tasks
        columns = ('Name', 'Type', 'Status', 'CPU%', 'Memory', 'Duration', 'PID')
        self.tasks_tree = ttk.Treeview(tasks_frame, columns=columns, show='headings', height=10)
        
        # Configure columns
        for col in columns:
            self.tasks_tree.heading(col, text=col)
            self.tasks_tree.column(col, width=100)
        
        # Scrollbar for tasks
        tasks_scrollbar = ttk.Scrollbar(tasks_frame, orient=tk.VERTICAL, command=self.tasks_tree.yview)
        self.tasks_tree.configure(yscrollcommand=tasks_scrollbar.set)
        
        self.tasks_tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        tasks_scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        
        # Control buttons
        control_frame = ttk.Frame(task_window)
        control_frame.pack(fill=tk.X, padx=10, pady=5)
        
        ttk.Button(control_frame, text="🔄 Refresh", 
                  command=self.refresh_task_manager).pack(side=tk.LEFT, padx=5)
        ttk.Button(control_frame, text="⏸️ Pause Selected", 
                  command=self.pause_selected_task).pack(side=tk.LEFT, padx=5)
        ttk.Button(control_frame, text="▶️ Resume Selected", 
                  command=self.resume_selected_task).pack(side=tk.LEFT, padx=5)
        ttk.Button(control_frame, text="🛑 Terminate Selected", 
                  command=self.terminate_selected_task).pack(side=tk.LEFT, padx=5)
        ttk.Button(control_frame, text="📊 Performance Report", 
                  command=self.show_performance_report).pack(side=tk.LEFT, padx=5)
        
        # Initialize
        self.refresh_task_manager()
        
        # Auto-refresh every 3 seconds
        self.refresh_job = task_window.after(3000, lambda: self.refresh_task_manager())
    
    def refresh_task_manager(self):
        """Refresh task manager display"""
        # Update system status
        self.system_status_text.delete(1.0, tk.END)
        
        system_stats = self.task_manager.system_stats
        status_text = f"""System Performance:
CPU Usage: {system_stats['cpu_percent']:.1f}%
Memory Usage: {system_stats['memory_percent']:.1f}%
Disk Usage: {system_stats['disk_usage']:.1f}%
Process Count: {system_stats['process_count']}
Network I/O: {system_stats['network_io']['bytes_sent']:,} bytes sent, {system_stats['network_io']['bytes_recv']:,} bytes received

Task Summary:
"""
        
        all_status = self.task_manager.get_all_tasks_status()
        status_text += f"""Total Tasks: {all_status['total_tasks']}
Running: {all_status['running_tasks']}
Idle: {all_status['idle_tasks']}
Completed: {all_status['completed_tasks']}
Errors: {all_status['error_tasks']}
Total CPU: {all_status['total_cpu_percent']:.1f}%
Total Memory: {all_status['total_memory_mb']:.1f}MB
"""
        
        self.system_status_text.insert(tk.END, status_text)
        
        # Update tasks tree
        for item in self.tasks_tree.get_children():
            self.tasks_tree.delete(item)
        
        for task_id, task in self.task_manager.tasks.items():
            task_status = self.task_manager.get_task_status(task_id)
            if task_status:
                duration = f"{task_status['duration']:.1f}s" if task_status['duration'] > 0 else "N/A"
                
                self.tasks_tree.insert('', 'end', values=(
                    task_status['name'],
                    task_status['type'],
                    task_status['status'],
                    f"{task_status['cpu_percent']:.1f}%",
                    f"{task_status['memory_mb']:.1f}MB",
                    duration,
                    task_status['process_id'] if task_status['process_id'] > 0 else "N/A"
                ), tags=(task_status['status'],))
        
        # Configure tags for color coding
        self.tasks_tree.tag_configure('running', foreground='#00ff00')
        self.tasks_tree.tag_configure('idle', foreground='#ffff00')
        self.tasks_tree.tag_configure('completed', foreground='#00ffff')
        self.tasks_tree.tag_configure('error', foreground='#ff0000')
        self.tasks_tree.tag_configure('paused', foreground='#ff8800')
        
        # Schedule next refresh
        if self.refresh_job:
            self.refresh_job = self.parent.after(3000, lambda: self.refresh_task_manager())
    
    def pause_selected_task(self):
        """Pause selected task"""
        selection = self.tasks_tree.selection()
        if not selection:
            messagebox.showwarning("Warning", "Please select a task to pause")
            return
        
        # Get task name from selection
        item = self.tasks_tree.item(selection[0])
        task_name = item['values'][0]
        
        # Find task by name
        for task_id, task in self.task_manager.tasks.items():
            if task.name == task_name:
                if self.task_manager.pause_task(task_id):
                    messagebox.showinfo("Success", f"Paused task: {task_name}")
                else:
                    messagebox.showerror("Error", f"Failed to pause task: {task_name}")
                break
    
    def resume_selected_task(self):
        """Resume selected task"""
        selection = self.tasks_tree.selection()
        if not selection:
            messagebox.showwarning("Warning", "Please select a task to resume")
            return
        
        item = self.tasks_tree.item(selection[0])
        task_name = item['values'][0]
        
        for task_id, task in self.task_manager.tasks.items():
            if task.name == task_name:
                if self.task_manager.resume_task(task_id):
                    messagebox.showinfo("Success", f"Resumed task: {task_name}")
                else:
                    messagebox.showerror("Error", f"Failed to resume task: {task_name}")
                break
    
    def terminate_selected_task(self):
        """Terminate selected task"""
        selection = self.tasks_tree.selection()
        if not selection:
            messagebox.showwarning("Warning", "Please select a task to terminate")
            return
        
        item = self.tasks_tree.item(selection[0])
        task_name = item['values'][0]
        
        if messagebox.askyesno("Confirm", f"Are you sure you want to terminate task: {task_name}?"):
            for task_id, task in self.task_manager.tasks.items():
                if task.name == task_name:
                    if self.task_manager.terminate_task(task_id):
                        messagebox.showinfo("Success", f"Terminated task: {task_name}")
                    else:
                        messagebox.showerror("Error", f"Failed to terminate task: {task_name}")
                    break
    
    def show_performance_report(self):
        """Show performance report"""
        report = self.task_manager.get_performance_report()
        
        report_window = tk.Toplevel(self.parent)
        report_window.title("Performance Report")
        report_window.geometry("600x500")
        report_window.configure(bg='#2d2d30')
        
        report_text = scrolledtext.ScrolledText(
            report_window, wrap=tk.WORD,
            bg='#1e1e1e', fg='#ffffff', font=('Consolas', 9)
        )
        report_text.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        report_content = f"""IDE Performance Report
{'='*50}

System Performance:
  CPU Usage: {report['system_performance']['cpu_percent']:.1f}%
  Memory Usage: {report['system_performance']['memory_percent']:.1f}%
  Disk Usage: {report['system_performance']['disk_usage']:.1f}%
  Process Count: {report['system_performance']['process_count']}

Task Performance:
  Total Tasks: {report['task_performance']['total_tasks']}
  Active Tasks: {report['task_performance']['active_tasks']}
  Average CPU: {report['task_performance']['avg_cpu_percent']:.1f}%
  Average Memory: {report['task_performance']['avg_memory_mb']:.1f}MB
  Total Memory: {report['task_performance']['total_memory_mb']:.1f}MB

Recommendations:
"""
        
        for rec in report['recommendations']:
            report_content += f"  • {rec}\n"
        
        report_text.insert(tk.END, report_content)

# Demo usage
if __name__ == "__main__":
    print("📊 IDE Task Manager Demo")
    print("=" * 50)
    
    # Create task manager
    task_manager = IDETaskManager()
    
    # Create some demo tasks
    print("\n📋 Creating demo tasks...")
    
    # AI session tasks
    ai_task1 = task_manager.create_task("AI Chat Session", TaskType.AI_SESSION, TaskPriority.HIGH)
    ai_task2 = task_manager.create_task("AI Copilot Session", TaskType.AI_SESSION, TaskPriority.MEDIUM)
    
    # Compilation tasks
    compile_task1 = task_manager.create_task("EON Compilation", TaskType.COMPILATION, TaskPriority.HIGH)
    compile_task2 = task_manager.create_task("Python Compilation", TaskType.COMPILATION, TaskPriority.MEDIUM)
    
    # Testing tasks
    test_task = task_manager.create_task("Unit Tests", TaskType.TESTING, TaskPriority.LOW)
    
    # Start some tasks
    task_manager.start_task(ai_task1)
    task_manager.start_task(compile_task1)
    task_manager.start_task(test_task)
    
    # Monitor for a bit
    print("\n📊 Monitoring tasks...")
    for i in range(5):
        time.sleep(2)
        
        status = task_manager.get_all_tasks_status()
        print(f"\n--- Status Check {i+1} ---")
        print(f"Total tasks: {status['total_tasks']}")
        print(f"Running: {status['running_tasks']}")
        print(f"System CPU: {status['system_stats']['cpu_percent']:.1f}%")
        print(f"System Memory: {status['system_stats']['memory_percent']:.1f}%")
        
        # Show individual task status
        for task_id in [ai_task1, compile_task1, test_task]:
            task_status = task_manager.get_task_status(task_id)
            if task_status:
                print(f"  {task_status['name']}: {task_status['status']} ({task_status['cpu_percent']:.1f}% CPU)")
    
    # Complete some tasks
    task_manager.complete_task(ai_task1, success=True)
    task_manager.complete_task(compile_task1, success=True)
    
    # Show performance report
    print("\n📈 Performance Report:")
    report = task_manager.get_performance_report()
    print(f"System CPU: {report['system_performance']['cpu_percent']:.1f}%")
    print(f"System Memory: {report['system_performance']['memory_percent']:.1f}%")
    print(f"Total Tasks: {report['task_performance']['total_tasks']}")
    print(f"Active Tasks: {report['task_performance']['active_tasks']}")
    
    print("\n💡 Recommendations:")
    for rec in report['recommendations']:
        print(f"  • {rec}")
    
    print("\n✅ IDE Task Manager Demo Complete!")
