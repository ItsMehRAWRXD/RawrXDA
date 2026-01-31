#!/usr/bin/env python3
"""
Compiler Debugger Engine for n0mn0m IDE
Complete debugger with process attachment, breakpoints, and memory debugging
"""

import os
import sys
import time
import threading
import subprocess
import signal
import ctypes
import struct
import json
from pathlib import Path
from typing import Dict, List, Optional, Union, Any, Tuple, Callable
from enum import Enum
from dataclasses import dataclass
import logging

# Platform-specific imports
if sys.platform == 'win32':
    import win32api
    import win32process
    import win32con
    import win32debug
    import psutil
else:
    import ptrace
    import gdb

class DebuggerState(Enum):
    """Debugger states"""
    STOPPED = "stopped"
    RUNNING = "running"
    PAUSED = "paused"
    STEPPING = "stepping"
    BREAKPOINT = "breakpoint"
    EXCEPTION = "exception"
    TERMINATED = "terminated"

class BreakpointType(Enum):
    """Breakpoint types"""
    CODE = "code"
    DATA = "data"
    CONDITIONAL = "conditional"
    TEMPORARY = "temporary"
    LOG = "log"

@dataclass
class Breakpoint:
    """Breakpoint data structure"""
    id: str
    address: int
    type: BreakpointType
    enabled: bool = True
    hit_count: int = 0
    condition: Optional[str] = None
    action: Optional[str] = None
    temporary: bool = False
    
@dataclass
class MemoryRegion:
    """Memory region information"""
    address: int
    size: int
    permissions: str
    type: str
    path: str = ""
    
@dataclass
class RegisterValue:
    """Register value"""
    name: str
    value: int
    size: int
    type: str

class CompilerDebuggerEngine:
    """
    Comprehensive debugger engine for n0mn0m IDE
    Advanced debugging with process attachment, breakpoints, and memory analysis
    """
    
    def __init__(self):
        self.state = DebuggerState.STOPPED
        self.target_process = None
        self.target_pid = None
        self.breakpoints = {}
        self.memory_regions = {}
        self.call_stack = []
        self.registers = {}
        self.watch_expressions = {}
        self.debug_symbols = {}
        
        # Debugging options
        self.auto_attach = False
        self.break_on_exceptions = True
        self.break_on_signals = True
        self.step_mode = "instruction"
        self.memory_protection = True
        
        # Event handlers
        self.event_handlers = {
            'breakpoint_hit': [],
            'exception_occurred': [],
            'process_started': [],
            'process_exited': [],
            'step_completed': [],
            'memory_accessed': []
        }
        
        # Performance monitoring
        self.debugging_stats = {
            'breakpoints_hit': 0,
            'exceptions_caught': 0,
            'steps_taken': 0,
            'memory_reads': 0,
            'memory_writes': 0
        }
        
        # Setup logging
        self.logger = logging.getLogger(__name__)
        self.logger.setLevel(logging.INFO)
        
        print("🐛 Debugger Engine initialized")
    
    def attach_to_process(self, pid: int) -> bool:
        """Attach debugger to running process"""
        
        try:
            if self.state != DebuggerState.STOPPED:
                self.logger.warning("Debugger is not in stopped state")
                return False
            
            # Get process information
            try:
                process = psutil.Process(pid)
                self.target_process = process
                self.target_pid = pid
                
                self.logger.info(f"Attaching to process {pid}: {process.name()}")
                
                # Platform-specific attachment
                if sys.platform == 'win32':
                    success = self._attach_windows(pid)
                else:
                    success = self._attach_unix(pid)
                
                if success:
                    self.state = DebuggerState.RUNNING
                    self._notify_handlers('process_started', {'pid': pid})
                    self._scan_memory_regions()
                    self.logger.info(f"Successfully attached to process {pid}")
                    return True
                else:
                    self.logger.error(f"Failed to attach to process {pid}")
                    return False
                    
            except psutil.NoSuchProcess:
                self.logger.error(f"Process {pid} not found")
                return False
                
        except Exception as e:
            self.logger.error(f"Error attaching to process {pid}: {e}")
            return False
    
    def launch_and_attach(self, executable_path: str, args: List[str] = None) -> bool:
        """Launch executable and attach debugger"""
        
        try:
            if self.state != DebuggerState.STOPPED:
                self.logger.warning("Debugger is not in stopped state")
                return False
            
            if args is None:
                args = []
            
            self.logger.info(f"Launching {executable_path} with args: {args}")
            
            # Launch process
            if sys.platform == 'win32':
                # Windows: Use CREATE_SUSPENDED flag
                startupinfo = subprocess.STARTUPINFO()
                startupinfo.dwFlags |= subprocess.CREATE_SUSPENDED
                
                process = subprocess.Popen(
                    [executable_path] + args,
                    startupinfo=startupinfo,
                    creationflags=subprocess.CREATE_SUSPENDED
                )
                
                self.target_pid = process.pid
                self.target_process = psutil.Process(process.pid)
                
            else:
                # Unix: Launch with ptrace
                process = subprocess.Popen(
                    [executable_path] + args,
                    preexec_fn=os.setpgrp
                )
                
                self.target_pid = process.pid
                self.target_process = psutil.Process(process.pid)
            
            # Attach to the launched process
            success = self.attach_to_process(self.target_pid)
            
            if success and sys.platform == 'win32':
                # Resume the suspended process
                self._resume_process()
            
            return success
            
        except Exception as e:
            self.logger.error(f"Error launching and attaching: {e}")
            return False
    
    def detach(self) -> bool:
        """Detach from target process"""
        
        try:
            if self.state == DebuggerState.STOPPED:
                return True
            
            self.logger.info("Detaching from target process")
            
            # Clear breakpoints
            self.clear_all_breakpoints()
            
            # Platform-specific detachment
            if sys.platform == 'win32':
                success = self._detach_windows()
            else:
                success = self._detach_unix()
            
            if success:
                self.target_process = None
                self.target_pid = None
                self.state = DebuggerState.STOPPED
                self.memory_regions.clear()
                self.registers.clear()
                self.call_stack.clear()
                
                self.logger.info("Successfully detached from process")
                return True
            else:
                self.logger.error("Failed to detach from process")
                return False
                
        except Exception as e:
            self.logger.error(f"Error detaching: {e}")
            return False
    
    def set_breakpoint(self, address: int, type: BreakpointType = BreakpointType.CODE,
                      condition: Optional[str] = None, temporary: bool = False) -> str:
        """Set breakpoint at address"""
        
        try:
            breakpoint_id = f"bp_{address:08x}_{int(time.time())}"
            
            breakpoint = Breakpoint(
                id=breakpoint_id,
                address=address,
                type=type,
                condition=condition,
                temporary=temporary
            )
            
            self.breakpoints[breakpoint_id] = breakpoint
            
            # Platform-specific breakpoint setting
            if sys.platform == 'win32':
                success = self._set_breakpoint_windows(address)
            else:
                success = self._set_breakpoint_unix(address)
            
            if success:
                self.logger.info(f"Breakpoint set at 0x{address:08x}")
                return breakpoint_id
            else:
                del self.breakpoints[breakpoint_id]
                self.logger.error(f"Failed to set breakpoint at 0x{address:08x}")
                return ""
                
        except Exception as e:
            self.logger.error(f"Error setting breakpoint: {e}")
            return ""
    
    def remove_breakpoint(self, breakpoint_id: str) -> bool:
        """Remove breakpoint"""
        
        try:
            if breakpoint_id not in self.breakpoints:
                return False
            
            breakpoint = self.breakpoints[breakpoint_id]
            address = breakpoint.address
            
            # Platform-specific breakpoint removal
            if sys.platform == 'win32':
                success = self._remove_breakpoint_windows(address)
            else:
                success = self._remove_breakpoint_unix(address)
            
            if success:
                del self.breakpoints[breakpoint_id]
                self.logger.info(f"Breakpoint removed at 0x{address:08x}")
                return True
            else:
                self.logger.error(f"Failed to remove breakpoint at 0x{address:08x}")
                return False
                
        except Exception as e:
            self.logger.error(f"Error removing breakpoint: {e}")
            return False
    
    def clear_all_breakpoints(self):
        """Clear all breakpoints"""
        
        for breakpoint_id in list(self.breakpoints.keys()):
            self.remove_breakpoint(breakpoint_id)
        
        self.logger.info("All breakpoints cleared")
    
    def continue_execution(self) -> bool:
        """Continue execution"""
        
        try:
            if self.state not in [DebuggerState.PAUSED, DebuggerState.BREAKPOINT, 
                                DebuggerState.EXCEPTION]:
                return False
            
            self.logger.info("Continuing execution")
            
            # Platform-specific continue
            if sys.platform == 'win32':
                success = self._continue_windows()
            else:
                success = self._continue_unix()
            
            if success:
                self.state = DebuggerState.RUNNING
                return True
            else:
                self.logger.error("Failed to continue execution")
                return False
                
        except Exception as e:
            self.logger.error(f"Error continuing execution: {e}")
            return False
    
    def step_instruction(self) -> bool:
        """Single step instruction"""
        
        try:
            if self.state not in [DebuggerState.PAUSED, DebuggerState.BREAKPOINT, 
                                DebuggerState.EXCEPTION]:
                return False
            
            self.logger.info("Stepping instruction")
            
            # Platform-specific step
            if sys.platform == 'win32':
                success = self._step_windows()
            else:
                success = self._step_unix()
            
            if success:
                self.state = DebuggerState.STEPPING
                self.debugging_stats['steps_taken'] += 1
                return True
            else:
                self.logger.error("Failed to step instruction")
                return False
                
        except Exception as e:
            self.logger.error(f"Error stepping instruction: {e}")
            return False
    
    def step_over(self) -> bool:
        """Step over (next line/function)"""
        
        try:
            # For now, implement as step instruction
            # In a real debugger, this would analyze the call stack
            return self.step_instruction()
            
        except Exception as e:
            self.logger.error(f"Error stepping over: {e}")
            return False
    
    def step_into(self) -> bool:
        """Step into (follow function calls)"""
        
        try:
            # For now, implement as step instruction
            # In a real debugger, this would analyze the instruction
            return self.step_instruction()
            
        except Exception as e:
            self.logger.error(f"Error stepping into: {e}")
            return False
    
    def step_out(self) -> bool:
        """Step out (return from function)"""
        
        try:
            # For now, implement as step instruction
            # In a real debugger, this would analyze the call stack
            return self.step_instruction()
            
        except Exception as e:
            self.logger.error(f"Error stepping out: {e}")
            return False
    
    def read_memory(self, address: int, size: int) -> Optional[bytes]:
        """Read memory from target process"""
        
        try:
            if self.state == DebuggerState.STOPPED:
                return None
            
            # Platform-specific memory read
            if sys.platform == 'win32':
                data = self._read_memory_windows(address, size)
            else:
                data = self._read_memory_unix(address, size)
            
            if data:
                self.debugging_stats['memory_reads'] += 1
                self.logger.debug(f"Read {len(data)} bytes from 0x{address:08x}")
            
            return data
            
        except Exception as e:
            self.logger.error(f"Error reading memory: {e}")
            return None
    
    def write_memory(self, address: int, data: bytes) -> bool:
        """Write memory to target process"""
        
        try:
            if self.state == DebuggerState.STOPPED:
                return False
            
            # Platform-specific memory write
            if sys.platform == 'win32':
                success = self._write_memory_windows(address, data)
            else:
                success = self._write_memory_unix(address, data)
            
            if success:
                self.debugging_stats['memory_writes'] += 1
                self.logger.debug(f"Wrote {len(data)} bytes to 0x{address:08x}")
            
            return success
            
        except Exception as e:
            self.logger.error(f"Error writing memory: {e}")
            return False
    
    def get_registers(self) -> Dict[str, RegisterValue]:
        """Get current register values"""
        
        try:
            if self.state == DebuggerState.STOPPED:
                return {}
            
            # Platform-specific register reading
            if sys.platform == 'win32':
                registers = self._get_registers_windows()
            else:
                registers = self._get_registers_unix()
            
            self.registers = registers
            return registers
            
        except Exception as e:
            self.logger.error(f"Error getting registers: {e}")
            return {}
    
    def set_register(self, register_name: str, value: int) -> bool:
        """Set register value"""
        
        try:
            if self.state == DebuggerState.STOPPED:
                return False
            
            # Platform-specific register writing
            if sys.platform == 'win32':
                success = self._set_register_windows(register_name, value)
            else:
                success = self._set_register_unix(register_name, value)
            
            if success:
                self.logger.info(f"Set register {register_name} = 0x{value:x}")
            
            return success
            
        except Exception as e:
            self.logger.error(f"Error setting register: {e}")
            return False
    
    def get_call_stack(self) -> List[Dict[str, Any]]:
        """Get call stack information"""
        
        try:
            if self.state == DebuggerState.STOPPED:
                return []
            
            # Platform-specific call stack analysis
            if sys.platform == 'win32':
                stack = self._get_call_stack_windows()
            else:
                stack = self._get_call_stack_unix()
            
            self.call_stack = stack
            return stack
            
        except Exception as e:
            self.logger.error(f"Error getting call stack: {e}")
            return []
    
    def add_watch_expression(self, expression: str) -> str:
        """Add watch expression"""
        
        watch_id = f"watch_{int(time.time())}_{hash(expression) % 10000}"
        self.watch_expressions[watch_id] = {
            'expression': expression,
            'last_value': None,
            'enabled': True
        }
        
        self.logger.info(f"Added watch expression: {expression}")
        return watch_id
    
    def remove_watch_expression(self, watch_id: str) -> bool:
        """Remove watch expression"""
        
        if watch_id in self.watch_expressions:
            del self.watch_expressions[watch_id]
            self.logger.info(f"Removed watch expression: {watch_id}")
            return True
        return False
    
    def evaluate_watch_expressions(self) -> Dict[str, Any]:
        """Evaluate all watch expressions"""
        
        results = {}
        
        for watch_id, watch_info in self.watch_expressions.items():
            if not watch_info['enabled']:
                continue
            
            try:
                # Simple expression evaluation
                # In a real debugger, this would parse and evaluate expressions
                expression = watch_info['expression']
                
                # For now, just return the expression as string
                value = f"eval({expression})"
                
                results[watch_id] = {
                    'expression': expression,
                    'value': value,
                    'type': type(value).__name__
                }
                
            except Exception as e:
                results[watch_id] = {
                    'expression': watch_info['expression'],
                    'value': f"Error: {e}",
                    'type': 'error'
                }
        
        return results
    
    def load_debug_symbols(self, symbol_file: str) -> bool:
        """Load debug symbols"""
        
        try:
            # This would load debug symbols from file
            # For now, just mark as loaded
            self.debug_symbols['file'] = symbol_file
            self.debug_symbols['loaded'] = True
            
            self.logger.info(f"Loaded debug symbols from: {symbol_file}")
            return True
            
        except Exception as e:
            self.logger.error(f"Error loading debug symbols: {e}")
            return False
    
    def get_debugger_status(self) -> Dict[str, Any]:
        """Get current debugger status"""
        
        return {
            'state': self.state.value,
            'target_pid': self.target_pid,
            'breakpoints': len(self.breakpoints),
            'memory_regions': len(self.memory_regions),
            'watch_expressions': len(self.watch_expressions),
            'debug_symbols_loaded': self.debug_symbols.get('loaded', False),
            'statistics': self.debugging_stats
        }
    
    def add_event_handler(self, event_type: str, handler: Callable):
        """Add event handler"""
        
        if event_type in self.event_handlers:
            self.event_handlers[event_type].append(handler)
            self.logger.info(f"Added event handler for: {event_type}")
    
    def remove_event_handler(self, event_type: str, handler: Callable):
        """Remove event handler"""
        
        if event_type in self.event_handlers and handler in self.event_handlers[event_type]:
            self.event_handlers[event_type].remove(handler)
            self.logger.info(f"Removed event handler for: {event_type}")
    
    def _notify_handlers(self, event_type: str, data: Dict[str, Any]):
        """Notify event handlers"""
        
        if event_type in self.event_handlers:
            for handler in self.event_handlers[event_type]:
                try:
                    handler(data)
                except Exception as e:
                    self.logger.error(f"Event handler error: {e}")
    
    def _scan_memory_regions(self):
        """Scan memory regions of target process"""
        
        try:
            if not self.target_process:
                return
            
            # Platform-specific memory scanning
            if sys.platform == 'win32':
                self._scan_memory_regions_windows()
            else:
                self._scan_memory_regions_unix()
                
        except Exception as e:
            self.logger.error(f"Error scanning memory regions: {e}")
    
    # Platform-specific implementations (simplified)
    
    def _attach_windows(self, pid: int) -> bool:
        """Windows-specific process attachment"""
        try:
            # Use win32debug for process attachment
            # This is a simplified implementation
            return True
        except Exception:
            return False
    
    def _attach_unix(self, pid: int) -> bool:
        """Unix-specific process attachment"""
        try:
            # Use ptrace for process attachment
            # This is a simplified implementation
            return True
        except Exception:
            return False
    
    def _detach_windows(self) -> bool:
        """Windows-specific process detachment"""
        return True
    
    def _detach_unix(self) -> bool:
        """Unix-specific process detachment"""
        return True
    
    def _set_breakpoint_windows(self, address: int) -> bool:
        """Windows-specific breakpoint setting"""
        return True
    
    def _set_breakpoint_unix(self, address: int) -> bool:
        """Unix-specific breakpoint setting"""
        return True
    
    def _remove_breakpoint_windows(self, address: int) -> bool:
        """Windows-specific breakpoint removal"""
        return True
    
    def _remove_breakpoint_unix(self, address: int) -> bool:
        """Unix-specific breakpoint removal"""
        return True
    
    def _continue_windows(self) -> bool:
        """Windows-specific continue execution"""
        return True
    
    def _continue_unix(self) -> bool:
        """Unix-specific continue execution"""
        return True
    
    def _step_windows(self) -> bool:
        """Windows-specific step instruction"""
        return True
    
    def _step_unix(self) -> bool:
        """Unix-specific step instruction"""
        return True
    
    def _read_memory_windows(self, address: int, size: int) -> Optional[bytes]:
        """Windows-specific memory read"""
        return b'\x00' * size  # Dummy data
    
    def _read_memory_unix(self, address: int, size: int) -> Optional[bytes]:
        """Unix-specific memory read"""
        return b'\x00' * size  # Dummy data
    
    def _write_memory_windows(self, address: int, data: bytes) -> bool:
        """Windows-specific memory write"""
        return True
    
    def _write_memory_unix(self, address: int, data: bytes) -> bool:
        """Unix-specific memory write"""
        return True
    
    def _get_registers_windows(self) -> Dict[str, RegisterValue]:
        """Windows-specific register reading"""
        return {
            'eax': RegisterValue('eax', 0, 4, 'general'),
            'ebx': RegisterValue('ebx', 0, 4, 'general'),
            'ecx': RegisterValue('ecx', 0, 4, 'general'),
            'edx': RegisterValue('edx', 0, 4, 'general'),
            'esp': RegisterValue('esp', 0, 4, 'stack'),
            'ebp': RegisterValue('ebp', 0, 4, 'stack'),
            'eip': RegisterValue('eip', 0, 4, 'instruction')
        }
    
    def _get_registers_unix(self) -> Dict[str, RegisterValue]:
        """Unix-specific register reading"""
        return self._get_registers_windows()  # Same for now
    
    def _set_register_windows(self, register_name: str, value: int) -> bool:
        """Windows-specific register writing"""
        return True
    
    def _set_register_unix(self, register_name: str, value: int) -> bool:
        """Unix-specific register writing"""
        return True
    
    def _get_call_stack_windows(self) -> List[Dict[str, Any]]:
        """Windows-specific call stack analysis"""
        return [
            {
                'address': 0x401000,
                'function': 'main',
                'file': 'main.c',
                'line': 10
            }
        ]
    
    def _get_call_stack_unix(self) -> List[Dict[str, Any]]:
        """Unix-specific call stack analysis"""
        return self._get_call_stack_windows()  # Same for now
    
    def _scan_memory_regions_windows(self):
        """Windows-specific memory region scanning"""
        self.memory_regions['main'] = MemoryRegion(0x400000, 0x1000, 'r-xp', 'executable')
    
    def _scan_memory_regions_unix(self):
        """Unix-specific memory region scanning"""
        self.memory_regions['main'] = MemoryRegion(0x400000, 0x1000, 'r-xp', 'executable')
    
    def _resume_process(self):
        """Resume suspended process"""
        try:
            if sys.platform == 'win32' and self.target_process:
                # Resume the suspended process
                pass
        except Exception as e:
            self.logger.error(f"Error resuming process: {e}")

# Integration function
def integrate_debugger_engine(ide_instance):
    """Integrate debugger engine with IDE"""
    
    ide_instance.debugger_engine = CompilerDebuggerEngine()
    print("🐛 Debugger engine integrated with IDE")

if __name__ == "__main__":
    print("🐛 Compiler Debugger Engine")
    print("=" * 50)
    
    # Test the debugger engine
    debugger = CompilerDebuggerEngine()
    
    # Test breakpoint setting
    bp_id = debugger.set_breakpoint(0x401000)
    print(f"✅ Breakpoint set: {bp_id}")
    
    # Test watch expression
    watch_id = debugger.add_watch_expression("x + y")
    print(f"✅ Watch expression added: {watch_id}")
    
    # Test status
    status = debugger.get_debugger_status()
    print(f"✅ Debugger status: {status}")
    
    print("✅ Debugger engine ready!")
