#!/usr/bin/env python3
"""
Memory Allocation System
Advanced memory management for unlimited AI sessions
Optimizes memory allocation, garbage collection, and resource management

⚠️  DO NOT DISTRIBUTE - PROPRIETARY SOFTWARE ⚠️
This software is proprietary and confidential. Unauthorized distribution,
copying, or modification is strictly prohibited. All rights reserved.

Copyright (c) 2024 - All Rights Reserved
"""

import threading
import time
import gc
import psutil
import os
import sys
from typing import Dict, List, Any, Optional, Tuple
from dataclasses import dataclass, asdict
from enum import Enum
import queue
import weakref
from collections import defaultdict
import tracemalloc
import resource

class MemoryPoolType(Enum):
    """Types of memory pools"""
    AI_SESSION = "ai_session"
    MESSAGE_QUEUE = "message_queue"
    CONTEXT_CACHE = "context_cache"
    RESPONSE_BUFFER = "response_buffer"
    TEMPORARY = "temporary"

class MemoryPriority(Enum):
    """Memory allocation priorities"""
    CRITICAL = 1      # AI responses, user input
    HIGH = 2          # Active sessions, real-time data
    MEDIUM = 3        # Cached data, background tasks
    LOW = 4           # Historical data, logs
    GARBAGE = 5       # Temporary objects, cleanup

@dataclass
class MemoryBlock:
    """Individual memory block"""
    block_id: str
    pool_type: MemoryPoolType
    priority: MemoryPriority
    size_bytes: int
    allocated_at: float
    last_accessed: float
    access_count: int = 0
    is_locked: bool = False
    data: Any = None

@dataclass
class MemoryPool:
    """Memory pool for specific allocation types"""
    pool_type: MemoryPoolType
    max_size: int
    current_size: int = 0
    blocks: Dict[str, MemoryBlock] = None
    allocation_count: int = 0
    deallocation_count: int = 0
    
    def __post_init__(self):
        if self.blocks is None:
            self.blocks = {}

class AdvancedMemoryManager:
    """Advanced memory management system"""
    
    def __init__(self, total_memory_limit_mb: int = 2048):
        self.total_memory_limit = total_memory_limit_mb * 1024 * 1024  # Convert to bytes
        self.memory_pools: Dict[MemoryPoolType, MemoryPool] = {}
        self.memory_blocks: Dict[str, MemoryBlock] = {}
        self.allocation_lock = threading.RLock()
        self.cleanup_thread = None
        self.monitoring_thread = None
        
        # Memory tracking
        self.total_allocated = 0
        self.total_freed = 0
        self.peak_memory_usage = 0
        self.allocation_history = []
        
        # Performance metrics
        self.allocation_times = []
        self.gc_times = []
        self.cleanup_times = []
        
        # Initialize memory pools
        self._initialize_memory_pools()
        
        # Start background services
        self._start_memory_services()
        
        # Enable memory tracing
        tracemalloc.start()
    
    def _initialize_memory_pools(self):
        """Initialize memory pools for different allocation types"""
        pool_configs = {
            MemoryPoolType.AI_SESSION: {
                'max_size': 512 * 1024 * 1024,  # 512MB
                'priority': MemoryPriority.HIGH
            },
            MemoryPoolType.MESSAGE_QUEUE: {
                'max_size': 256 * 1024 * 1024,  # 256MB
                'priority': MemoryPriority.CRITICAL
            },
            MemoryPoolType.CONTEXT_CACHE: {
                'max_size': 1024 * 1024 * 1024,  # 1GB
                'priority': MemoryPriority.MEDIUM
            },
            MemoryPoolType.RESPONSE_BUFFER: {
                'max_size': 128 * 1024 * 1024,  # 128MB
                'priority': MemoryPriority.HIGH
            },
            MemoryPoolType.TEMPORARY: {
                'max_size': 64 * 1024 * 1024,   # 64MB
                'priority': MemoryPriority.LOW
            }
        }
        
        for pool_type, config in pool_configs.items():
            self.memory_pools[pool_type] = MemoryPool(
                pool_type=pool_type,
                max_size=config['max_size']
            )
    
    def _start_memory_services(self):
        """Start background memory management services"""
        # Memory monitoring thread
        self.monitoring_thread = threading.Thread(
            target=self._memory_monitoring_loop, 
            daemon=True
        )
        self.monitoring_thread.start()
        
        # Memory cleanup thread
        self.cleanup_thread = threading.Thread(
            target=self._memory_cleanup_loop, 
            daemon=True
        )
        self.cleanup_thread.start()
    
    def allocate_memory(self, size_bytes: int, pool_type: MemoryPoolType, 
                       priority: MemoryPriority = MemoryPriority.MEDIUM,
                       data: Any = None) -> Optional[str]:
        """Allocate memory block with advanced management"""
        start_time = time.time()
        
        with self.allocation_lock:
            # Check if we have enough memory
            if not self._check_memory_availability(size_bytes, pool_type):
                # Try to free memory
                self._emergency_cleanup(size_bytes)
                
                # Check again
                if not self._check_memory_availability(size_bytes, pool_type):
                    return None  # Allocation failed
            
            # Create memory block
            block_id = f"{pool_type.value}_{int(time.time() * 1000)}_{id(data)}"
            
            block = MemoryBlock(
                block_id=block_id,
                pool_type=pool_type,
                priority=priority,
                size_bytes=size_bytes,
                allocated_at=time.time(),
                last_accessed=time.time(),
                data=data
            )
            
            # Add to pools
            self.memory_blocks[block_id] = block
            self.memory_pools[pool_type].blocks[block_id] = block
            self.memory_pools[pool_type].current_size += size_bytes
            self.memory_pools[pool_type].allocation_count += 1
            
            # Update totals
            self.total_allocated += size_bytes
            self.peak_memory_usage = max(self.peak_memory_usage, self.total_allocated - self.total_freed)
            
            # Record allocation time
            allocation_time = time.time() - start_time
            self.allocation_times.append(allocation_time)
            
            return block_id
    
    def deallocate_memory(self, block_id: str) -> bool:
        """Deallocate memory block"""
        with self.allocation_lock:
            if block_id not in self.memory_blocks:
                return False
            
            block = self.memory_blocks[block_id]
            
            # Remove from pools
            if block_id in self.memory_pools[block.pool_type].blocks:
                del self.memory_pools[block.pool_type].blocks[block_id]
                self.memory_pools[block.pool_type].current_size -= block.size_bytes
                self.memory_pools[block.pool_type].deallocation_count += 1
            
            # Update totals
            self.total_freed += block.size_bytes
            
            # Remove from tracking
            del self.memory_blocks[block_id]
            
            return True
    
    def _check_memory_availability(self, size_bytes: int, pool_type: MemoryPoolType) -> bool:
        """Check if memory is available for allocation"""
        pool = self.memory_pools[pool_type]
        
        # Check pool-specific limit
        if pool.current_size + size_bytes > pool.max_size:
            return False
        
        # Check total memory limit
        current_usage = self.total_allocated - self.total_freed
        if current_usage + size_bytes > self.total_memory_limit:
            return False
        
        # Check system memory
        system_memory = psutil.virtual_memory()
        if system_memory.available < size_bytes * 2:  # Keep 2x buffer
            return False
        
        return True
    
    def _emergency_cleanup(self, required_bytes: int):
        """Emergency memory cleanup to free required bytes"""
        print(f"🚨 Emergency memory cleanup: need {required_bytes} bytes")
        
        # Sort blocks by priority and age
        blocks_to_clean = []
        for block in self.memory_blocks.values():
            if not block.is_locked:
                score = self._calculate_cleanup_score(block)
                blocks_to_clean.append((score, block))
        
        # Sort by cleanup score (higher = more likely to be cleaned)
        blocks_to_clean.sort(key=lambda x: x[0], reverse=True)
        
        freed_bytes = 0
        for score, block in blocks_to_clean:
            if freed_bytes >= required_bytes:
                break
            
            if self.deallocate_memory(block.block_id):
                freed_bytes += block.size_bytes
                print(f"   Freed {block.size_bytes} bytes from {block.block_id}")
        
        print(f"   Emergency cleanup freed {freed_bytes} bytes")
    
    def _calculate_cleanup_score(self, block: MemoryBlock) -> float:
        """Calculate cleanup score for memory block"""
        age = time.time() - block.allocated_at
        last_access = time.time() - block.last_accessed
        
        # Higher score = more likely to be cleaned
        score = 0.0
        
        # Priority factor (lower priority = higher score)
        priority_scores = {
            MemoryPriority.CRITICAL: 0.0,
            MemoryPriority.HIGH: 0.2,
            MemoryPriority.MEDIUM: 0.5,
            MemoryPriority.LOW: 0.8,
            MemoryPriority.GARBAGE: 1.0
        }
        score += priority_scores.get(block.priority, 0.5)
        
        # Age factor (older = higher score)
        score += min(1.0, age / 3600)  # 1 hour = full score
        
        # Access factor (less accessed = higher score)
        score += min(1.0, last_access / 1800)  # 30 minutes = full score
        
        # Size factor (larger = higher score for cleanup)
        score += min(0.5, block.size_bytes / (1024 * 1024))  # 1MB = 0.5 score
        
        return score
    
    def _memory_monitoring_loop(self):
        """Background memory monitoring"""
        while True:
            try:
                # Get current memory usage
                process = psutil.Process()
                memory_info = process.memory_info()
                system_memory = psutil.virtual_memory()
                
                # Update metrics
                current_usage = self.total_allocated - self.total_freed
                memory_pressure = system_memory.percent
                
                # Log memory status
                if current_usage > self.total_memory_limit * 0.8:  # 80% threshold
                    print(f"⚠️ High memory usage: {current_usage / (1024*1024):.1f}MB / {self.total_memory_limit / (1024*1024):.1f}MB")
                
                # Record allocation history
                self.allocation_history.append({
                    'timestamp': time.time(),
                    'allocated': self.total_allocated,
                    'freed': self.total_freed,
                    'current': current_usage,
                    'system_percent': memory_pressure
                })
                
                # Keep only last 1000 records
                if len(self.allocation_history) > 1000:
                    self.allocation_history = self.allocation_history[-1000:]
                
                time.sleep(5)  # Monitor every 5 seconds
                
            except Exception as e:
                print(f"Memory monitoring error: {e}")
                time.sleep(10)
    
    def _memory_cleanup_loop(self):
        """Background memory cleanup"""
        while True:
            try:
                # Perform garbage collection
                start_time = time.time()
                collected = gc.collect()
                gc_time = time.time() - start_time
                self.gc_times.append(gc_time)
                
                # Clean up old blocks
                self._cleanup_old_blocks()
                
                # Clean up temporary blocks
                self._cleanup_temporary_blocks()
                
                time.sleep(30)  # Cleanup every 30 seconds
                
            except Exception as e:
                print(f"Memory cleanup error: {e}")
                time.sleep(60)
    
    def _cleanup_old_blocks(self):
        """Clean up old memory blocks"""
        current_time = time.time()
        blocks_to_remove = []
        
        for block_id, block in self.memory_blocks.items():
            if block.is_locked:
                continue
            
            # Clean up blocks older than 1 hour with low priority
            if (current_time - block.allocated_at > 3600 and 
                block.priority in [MemoryPriority.LOW, MemoryPriority.GARBAGE]):
                blocks_to_remove.append(block_id)
        
        # Remove old blocks
        for block_id in blocks_to_remove:
            self.deallocate_memory(block_id)
        
        if blocks_to_remove:
            print(f"🧹 Cleaned up {len(blocks_to_remove)} old memory blocks")
    
    def _cleanup_temporary_blocks(self):
        """Clean up temporary memory blocks"""
        current_time = time.time()
        temp_blocks = [b for b in self.memory_blocks.values() 
                      if b.pool_type == MemoryPoolType.TEMPORARY and not b.is_locked]
        
        # Clean up temp blocks older than 5 minutes
        for block in temp_blocks:
            if current_time - block.allocated_at > 300:  # 5 minutes
                self.deallocate_memory(block.block_id)
    
    def get_memory_status(self) -> Dict[str, Any]:
        """Get comprehensive memory status"""
        process = psutil.Process()
        memory_info = process.memory_info()
        system_memory = psutil.virtual_memory()
        
        current_usage = self.total_allocated - self.total_freed
        
        return {
            'total_allocated': self.total_allocated,
            'total_freed': self.total_freed,
            'current_usage': current_usage,
            'peak_usage': self.peak_memory_usage,
            'memory_limit': self.total_memory_limit,
            'usage_percent': (current_usage / self.total_memory_limit) * 100,
            'system_memory': {
                'total': system_memory.total,
                'available': system_memory.available,
                'percent': system_memory.percent
            },
            'process_memory': {
                'rss': memory_info.rss,
                'vms': memory_info.vms
            },
            'pools': {
                pool_type.value: {
                    'current_size': pool.current_size,
                    'max_size': pool.max_size,
                    'usage_percent': (pool.current_size / pool.max_size) * 100,
                    'block_count': len(pool.blocks),
                    'allocations': pool.allocation_count,
                    'deallocations': pool.deallocation_count
                }
                for pool_type, pool in self.memory_pools.items()
            },
            'performance': {
                'avg_allocation_time': sum(self.allocation_times[-100:]) / max(len(self.allocation_times[-100:]), 1),
                'avg_gc_time': sum(self.gc_times[-100:]) / max(len(self.gc_times[-100:]), 1),
                'total_blocks': len(self.memory_blocks)
            }
        }
    
    def optimize_memory(self):
        """Optimize memory usage"""
        print("🔧 Optimizing memory usage...")
        
        # Force garbage collection
        start_time = time.time()
        collected = gc.collect()
        gc_time = time.time() - start_time
        
        # Clean up all temporary blocks
        temp_blocks = [b for b in self.memory_blocks.values() 
                      if b.pool_type == MemoryPoolType.TEMPORARY and not b.is_locked]
        
        for block in temp_blocks:
            self.deallocate_memory(block.block_id)
        
        # Clean up low priority blocks
        low_priority_blocks = [b for b in self.memory_blocks.values() 
                              if b.priority == MemoryPriority.LOW and not b.is_locked]
        
        for block in low_priority_blocks:
            self.deallocate_memory(block.block_id)
        
        print(f"   Garbage collection: {collected} objects, {gc_time:.3f}s")
        print(f"   Cleaned up {len(temp_blocks)} temporary blocks")
        print(f"   Cleaned up {len(low_priority_blocks)} low priority blocks")
    
    def lock_memory_block(self, block_id: str) -> bool:
        """Lock memory block to prevent cleanup"""
        if block_id in self.memory_blocks:
            self.memory_blocks[block_id].is_locked = True
            return True
        return False
    
    def unlock_memory_block(self, block_id: str) -> bool:
        """Unlock memory block to allow cleanup"""
        if block_id in self.memory_blocks:
            self.memory_blocks[block_id].is_locked = False
            return True
        return False
    
    def get_memory_recommendations(self) -> List[str]:
        """Get memory optimization recommendations"""
        recommendations = []
        status = self.get_memory_status()
        
        # Check memory usage
        if status['usage_percent'] > 80:
            recommendations.append("High memory usage detected - consider optimizing")
        
        # Check pool usage
        for pool_type, pool_info in status['pools'].items():
            if pool_info['usage_percent'] > 90:
                recommendations.append(f"{pool_type} pool is nearly full ({pool_info['usage_percent']:.1f}%)")
        
        # Check system memory
        if status['system_memory']['percent'] > 85:
            recommendations.append("System memory pressure detected")
        
        # Check allocation performance
        if status['performance']['avg_allocation_time'] > 0.1:
            recommendations.append("Slow memory allocation detected - consider pool optimization")
        
        return recommendations

# Integration with unlimited AI system
class AIMemoryManager:
    """Memory manager specifically for AI sessions"""
    
    def __init__(self, memory_manager: AdvancedMemoryManager):
        self.memory_manager = memory_manager
        self.ai_session_blocks: Dict[str, List[str]] = {}  # session_id -> block_ids
        self.message_blocks: Dict[str, List[str]] = {}    # session_id -> message_block_ids
    
    def allocate_ai_session_memory(self, session_id: str, initial_size: int = 1024*1024) -> str:
        """Allocate memory for AI session"""
        block_id = self.memory_manager.allocate_memory(
            size_bytes=initial_size,
            pool_type=MemoryPoolType.AI_SESSION,
            priority=MemoryPriority.HIGH,
            data=f"AI_Session_{session_id}"
        )
        
        if block_id:
            if session_id not in self.ai_session_blocks:
                self.ai_session_blocks[session_id] = []
            self.ai_session_blocks[session_id].append(block_id)
            
            # Lock the block to prevent cleanup
            self.memory_manager.lock_memory_block(block_id)
        
        return block_id
    
    def allocate_message_memory(self, session_id: str, message_size: int) -> str:
        """Allocate memory for AI message"""
        block_id = self.memory_manager.allocate_memory(
            size_bytes=message_size,
            pool_type=MemoryPoolType.MESSAGE_QUEUE,
            priority=MemoryPriority.CRITICAL,
            data=f"Message_{session_id}"
        )
        
        if block_id:
            if session_id not in self.message_blocks:
                self.message_blocks[session_id] = []
            self.message_blocks[session_id].append(block_id)
        
        return block_id
    
    def deallocate_session_memory(self, session_id: str):
        """Deallocate all memory for AI session"""
        # Deallocate session blocks
        if session_id in self.ai_session_blocks:
            for block_id in self.ai_session_blocks[session_id]:
                self.memory_manager.deallocate_memory(block_id)
            del self.ai_session_blocks[session_id]
        
        # Deallocate message blocks
        if session_id in self.message_blocks:
            for block_id in self.message_blocks[session_id]:
                self.memory_manager.deallocate_memory(block_id)
            del self.message_blocks[session_id]
    
    def get_session_memory_usage(self, session_id: str) -> Dict[str, Any]:
        """Get memory usage for specific AI session"""
        session_blocks = self.ai_session_blocks.get(session_id, [])
        message_blocks = self.message_blocks.get(session_id, [])
        
        total_size = 0
        for block_id in session_blocks + message_blocks:
            if block_id in self.memory_manager.memory_blocks:
                block = self.memory_manager.memory_blocks[block_id]
                total_size += block.size_bytes
        
        return {
            'session_id': session_id,
            'total_size': total_size,
            'session_blocks': len(session_blocks),
            'message_blocks': len(message_blocks),
            'memory_mb': total_size / (1024 * 1024)
        }

# Demo usage
if __name__ == "__main__":
    print("🧠 Advanced Memory Allocation System Demo")
    print("=" * 50)
    
    # Create memory manager
    memory_manager = AdvancedMemoryManager(total_memory_limit_mb=1024)  # 1GB limit
    
    # Create AI memory manager
    ai_memory = AIMemoryManager(memory_manager)
    
    # Simulate AI session memory allocation
    print("\n🤖 Simulating AI session memory allocation...")
    
    session_blocks = []
    for i in range(5):
        session_id = f"ai_session_{i}"
        block_id = ai_memory.allocate_ai_session_memory(session_id, 10*1024*1024)  # 10MB
        if block_id:
            session_blocks.append((session_id, block_id))
            print(f"   Allocated 10MB for {session_id}")
    
    # Simulate message allocation
    print("\n📨 Simulating message memory allocation...")
    
    for session_id, _ in session_blocks:
        for j in range(3):
            message_size = 1024 * 100  # 100KB per message
            block_id = ai_memory.allocate_message_memory(session_id, message_size)
            if block_id:
                print(f"   Allocated {message_size/1024:.1f}KB for message in {session_id}")
    
    # Show memory status
    print("\n📊 Memory Status:")
    status = memory_manager.get_memory_status()
    print(f"   Current usage: {status['current_usage'] / (1024*1024):.1f}MB")
    print(f"   Peak usage: {status['peak_usage'] / (1024*1024):.1f}MB")
    print(f"   Usage percent: {status['usage_percent']:.1f}%")
    
    # Show pool status
    print("\n🏊 Memory Pools:")
    for pool_type, pool_info in status['pools'].items():
        print(f"   {pool_type}: {pool_info['current_size'] / (1024*1024):.1f}MB / {pool_info['max_size'] / (1024*1024):.1f}MB ({pool_info['usage_percent']:.1f}%)")
    
    # Show session memory usage
    print("\n🤖 AI Session Memory Usage:")
    for session_id, _ in session_blocks:
        usage = ai_memory.get_session_memory_usage(session_id)
        print(f"   {session_id}: {usage['memory_mb']:.1f}MB ({usage['session_blocks']} session blocks, {usage['message_blocks']} message blocks)")
    
    # Get recommendations
    print("\n💡 Memory Recommendations:")
    recommendations = memory_manager.get_memory_recommendations()
    for rec in recommendations:
        print(f"   • {rec}")
    
    # Optimize memory
    print("\n🔧 Optimizing memory...")
    memory_manager.optimize_memory()
    
    # Final status
    final_status = memory_manager.get_memory_status()
    print(f"\n✅ Final memory usage: {final_status['current_usage'] / (1024*1024):.1f}MB")
    
    print("\n🎉 Memory allocation system demo complete!")
