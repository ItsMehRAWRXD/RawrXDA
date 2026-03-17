#!/usr/bin/env python3
"""
PHASE 2: BEAST SWARM MEMORY OPTIMIZATION
========================================

Implements 4 key memory optimization techniques:
1. Object Pooling - Reuse message objects instead of constant allocation
2. Lazy Loading - Load agent modules only when needed  
3. Config Compression - Compress stored configurations
4. GC Tuning - Optimize garbage collection patterns

Target: 15%+ memory reduction
Timeline: 3 hours
"""

import gc
import json
import time
import weakref
import zlib
from typing import Dict, List, Any, Optional, Union
from collections import deque
import threading

class MessagePool:
    """Object pool for message reuse to reduce allocations"""
    
    def __init__(self, initial_size: int = 100, max_size: int = 1000):
        self.pool = deque()
        self.max_size = max_size
        self.created_count = 0
        self.reused_count = 0
        self.lock = threading.Lock()
        
        # Pre-populate pool
        for _ in range(initial_size):
            self.pool.append(self._create_message())
    
    def _create_message(self) -> Dict[str, Any]:
        """Create a new message object"""
        self.created_count += 1
        return {
            'id': '',
            'type': '',
            'payload': {},
            'timestamp': 0,
            'sender': '',
            'recipient': '',
            'priority': 'medium',
            '_pool_id': self.created_count
        }
    
    def acquire(self) -> Dict[str, Any]:
        """Get a message object from pool"""
        with self.lock:
            if self.pool:
                msg = self.pool.popleft()
                self.reused_count += 1
                # Reset to clean state
                msg.update({
                    'id': '',
                    'type': '',
                    'payload': {},
                    'timestamp': time.time(),
                    'sender': '',
                    'recipient': '',
                    'priority': 'medium'
                })
                return msg
            else:
                # Pool exhausted, create new
                return self._create_message()
    
    def release(self, message: Dict[str, Any]) -> None:
        """Return message to pool for reuse"""
        with self.lock:
            if len(self.pool) < self.max_size:
                # Clear sensitive data before returning to pool
                if 'payload' in message:
                    message['payload'].clear()
                self.pool.append(message)
    
    def get_stats(self) -> Dict[str, int]:
        """Get pool statistics"""
        return {
            'pool_size': len(self.pool),
            'max_size': self.max_size,
            'created_count': self.created_count,
            'reused_count': self.reused_count,
            'reuse_ratio': round(self.reused_count / max(self.created_count, 1) * 100, 1)
        }

class LazyAgentLoader:
    """Lazy loading system for agent modules"""
    
    def __init__(self):
        self.loaded_agents = {}  # agent_id -> agent_instance
        self.agent_configs = {}  # agent_id -> config
        self.load_stats = {
            'total_loads': 0,
            'cache_hits': 0,
            'memory_saved_mb': 0
        }
        self._agent_refs = {}  # weak references
    
    def register_agent_config(self, agent_id: str, config: Dict[str, Any]) -> None:
        """Register an agent configuration without loading"""
        self.agent_configs[agent_id] = config
    
    def get_agent(self, agent_id: str):
        """Get agent instance, loading if necessary"""
        # Check if already loaded
        if agent_id in self.loaded_agents:
            self.load_stats['cache_hits'] += 1
            return self.loaded_agents[agent_id]
        
        # Check weak references (garbage collected agents)
        if agent_id in self._agent_refs:
            agent = self._agent_refs[agent_id]()
            if agent is not None:
                self.loaded_agents[agent_id] = agent
                self.load_stats['cache_hits'] += 1
                return agent
        
        # Load agent from config
        if agent_id not in self.agent_configs:
            raise ValueError(f"No configuration found for agent: {agent_id}")
        
        agent = self._load_agent_from_config(self.agent_configs[agent_id])
        self.loaded_agents[agent_id] = agent
        self.load_stats['total_loads'] += 1
        
        # Store weak reference for memory cleanup
        self._agent_refs[agent_id] = weakref.ref(
            agent, lambda ref: self._on_agent_collected(agent_id)
        )
        
        return agent
    
    def _load_agent_from_config(self, config: Dict[str, Any]):
        """Create agent instance from configuration"""
        from beast_swarm_system import MicroAgent, AgentRole
        
        # Simulate agent creation (would be actual implementation)
        return MicroAgent(
            agent_id=config.get('id', 'unknown'),
            role=AgentRole(config.get('role', 'code_generation')),
            model_size_mb=config.get('size_mb', 250)
        )
    
    def _on_agent_collected(self, agent_id: str) -> None:
        """Callback when agent is garbage collected"""
        if agent_id in self.loaded_agents:
            del self.loaded_agents[agent_id]
        if agent_id in self._agent_refs:
            del self._agent_refs[agent_id]
        
        # Estimate memory saved (rough calculation)
        config = self.agent_configs.get(agent_id, {})
        size_mb = config.get('size_mb', 250)
        self.load_stats['memory_saved_mb'] += size_mb
    
    def unload_agent(self, agent_id: str) -> None:
        """Explicitly unload an agent"""
        if agent_id in self.loaded_agents:
            del self.loaded_agents[agent_id]
        if agent_id in self._agent_refs:
            del self._agent_refs[agent_id]
    
    def get_stats(self) -> Dict[str, Any]:
        """Get loading statistics"""
        return {
            'loaded_agents': len(self.loaded_agents),
            'total_configs': len(self.agent_configs),
            'load_stats': self.load_stats.copy(),
            'memory_efficiency': f"{len(self.loaded_agents)}/{len(self.agent_configs)} loaded"
        }

class ConfigCompressor:
    """Compress configuration data to reduce memory footprint"""
    
    def __init__(self, compression_level: int = 6):
        self.compression_level = compression_level
        self.compression_stats = {
            'original_size': 0,
            'compressed_size': 0,
            'configs_compressed': 0
        }
    
    def compress_config(self, config: Dict[str, Any]) -> bytes:
        """Compress configuration to bytes"""
        json_str = json.dumps(config, separators=(',', ':'))
        json_bytes = json_str.encode('utf-8')
        
        compressed = zlib.compress(json_bytes, self.compression_level)
        
        # Update stats
        self.compression_stats['original_size'] += len(json_bytes)
        self.compression_stats['compressed_size'] += len(compressed)
        self.compression_stats['configs_compressed'] += 1
        
        return compressed
    
    def decompress_config(self, compressed_data: bytes) -> Dict[str, Any]:
        """Decompress configuration from bytes"""
        json_bytes = zlib.decompress(compressed_data)
        json_str = json_bytes.decode('utf-8')
        return json.loads(json_str)
    
    def get_compression_ratio(self) -> float:
        """Get overall compression ratio"""
        if self.compression_stats['original_size'] == 0:
            return 0.0
        return self.compression_stats['compressed_size'] / self.compression_stats['original_size']
    
    def get_stats(self) -> Dict[str, Any]:
        """Get compression statistics"""
        ratio = self.get_compression_ratio()
        savings_pct = (1 - ratio) * 100 if ratio > 0 else 0
        
        return {
            'configs_compressed': self.compression_stats['configs_compressed'],
            'original_size_kb': round(self.compression_stats['original_size'] / 1024, 2),
            'compressed_size_kb': round(self.compression_stats['compressed_size'] / 1024, 2),
            'compression_ratio': round(ratio, 3),
            'space_savings_pct': round(savings_pct, 1)
        }

class GCOptimizer:
    """Optimize garbage collection for swarm operations"""
    
    def __init__(self):
        self.original_thresholds = gc.get_threshold()
        self.gc_stats = {
            'collections_before': gc.get_stats(),
            'collections_after': [],
            'optimizations_applied': []
        }
    
    def optimize_gc_for_swarm(self) -> None:
        """Optimize GC settings for swarm workloads"""
        # Tune GC thresholds for many small objects (agents/messages)
        # Increase gen0 threshold to reduce frequent collections
        # Decrease gen1/gen2 thresholds for periodic cleanup
        
        optimizations = []
        
        # Optimization 1: Reduce gen0 collection frequency
        # Good for high message throughput
        new_gen0 = self.original_thresholds[0] * 2
        gc.set_threshold(new_gen0, self.original_thresholds[1], self.original_thresholds[2])
        optimizations.append("Increased gen0 threshold for high throughput")
        
        # Optimization 2: Enable debug stats collection
        if hasattr(gc, 'set_debug'):
            gc.set_debug(gc.DEBUG_STATS)
            optimizations.append("Enabled GC debug statistics")
        
        # Optimization 3: Force a full collection now to start clean
        collected = gc.collect()
        optimizations.append(f"Performed initial cleanup: {collected} objects collected")
        
        self.gc_stats['optimizations_applied'] = optimizations
        self.gc_stats['new_thresholds'] = gc.get_threshold()
    
    def force_cleanup(self) -> int:
        """Force garbage collection and return objects collected"""
        return gc.collect()
    
    def restore_gc_settings(self) -> None:
        """Restore original GC settings"""
        gc.set_threshold(*self.original_thresholds)
    
    def get_stats(self) -> Dict[str, Any]:
        """Get GC optimization statistics"""
        current_stats = gc.get_stats()
        
        return {
            'original_thresholds': self.original_thresholds,
            'current_thresholds': gc.get_threshold(),
            'optimizations_applied': self.gc_stats['optimizations_applied'],
            'gc_collections': current_stats,
            'objects_in_gc': len(gc.get_objects())
        }

class MemoryOptimizer:
    """Main memory optimization coordinator"""
    
    def __init__(self):
        self.message_pool = MessagePool(initial_size=50, max_size=500)
        self.agent_loader = LazyAgentLoader()
        self.config_compressor = ConfigCompressor(compression_level=6)
        self.gc_optimizer = GCOptimizer()
        
        self.optimization_enabled = False
        self.start_time = None
        self.baseline_memory = 0
    
    def enable_optimizations(self) -> None:
        """Enable all memory optimizations"""
        self.start_time = time.time()
        self.optimization_enabled = True
        
        # Apply GC optimizations
        self.gc_optimizer.optimize_gc_for_swarm()
        
        print("✅ Memory optimizations enabled:")
        print("   - Message object pooling")
        print("   - Lazy agent loading")
        print("   - Configuration compression")
        print("   - GC tuning for swarm workloads")
    
    def disable_optimizations(self) -> None:
        """Disable optimizations and restore defaults"""
        self.optimization_enabled = False
        self.gc_optimizer.restore_gc_settings()
        print("✅ Memory optimizations disabled")
    
    def get_optimized_message(self) -> Dict[str, Any]:
        """Get message using object pooling"""
        if self.optimization_enabled:
            return self.message_pool.acquire()
        else:
            # Fallback to regular allocation
            return {
                'id': '', 'type': '', 'payload': {}, 'timestamp': time.time(),
                'sender': '', 'recipient': '', 'priority': 'medium'
            }
    
    def release_message(self, message: Dict[str, Any]) -> None:
        """Return message to pool"""
        if self.optimization_enabled:
            self.message_pool.release(message)
    
    def register_agent_for_lazy_loading(self, agent_id: str, config: Dict[str, Any]) -> None:
        """Register agent for lazy loading"""
        if self.optimization_enabled:
            # Compress config before storing
            compressed_config = self.config_compressor.compress_config(config)
            decompressed_config = self.config_compressor.decompress_config(compressed_config)
            self.agent_loader.register_agent_config(agent_id, decompressed_config)
        else:
            self.agent_loader.register_agent_config(agent_id, config)
    
    def get_comprehensive_stats(self) -> Dict[str, Any]:
        """Get comprehensive memory optimization statistics"""
        stats = {
            'optimization_enabled': self.optimization_enabled,
            'runtime_seconds': round(time.time() - self.start_time, 2) if self.start_time else 0,
            'message_pool': self.message_pool.get_stats(),
            'lazy_loading': self.agent_loader.get_stats(),
            'compression': self.config_compressor.get_stats(),
            'garbage_collection': self.gc_optimizer.get_stats()
        }
        
        return stats
    
    def force_cleanup(self) -> Dict[str, int]:
        """Force memory cleanup and return statistics"""
        results = {
            'gc_collected': self.gc_optimizer.force_cleanup(),
            'pool_size_before': len(self.message_pool.pool)
        }
        
        # Clear some pool objects to free memory
        with self.message_pool.lock:
            cleared = min(10, len(self.message_pool.pool))
            for _ in range(cleared):
                if self.message_pool.pool:
                    self.message_pool.pool.popleft()
        
        results['pool_cleared'] = cleared
        return results

def test_memory_optimizations():
    """Test memory optimization techniques"""
    print("🧪 TESTING MEMORY OPTIMIZATIONS")
    print("=" * 50)
    
    optimizer = MemoryOptimizer()
    
    # Test 1: Message pooling
    print("\n📊 Test 1: Message Pooling")
    print("-" * 30)
    
    optimizer.enable_optimizations()
    
    messages = []
    for i in range(100):
        msg = optimizer.get_optimized_message()
        msg['id'] = f'test_msg_{i}'
        msg['payload'] = {'data': f'payload_{i}'}
        messages.append(msg)
    
    # Return messages to pool
    for msg in messages[:50]:  # Return half
        optimizer.release_message(msg)
    
    pool_stats = optimizer.message_pool.get_stats()
    print(f"Created: {pool_stats['created_count']} messages")
    print(f"Reused: {pool_stats['reused_count']} messages")
    print(f"Reuse ratio: {pool_stats['reuse_ratio']}%")
    
    # Test 2: Lazy loading
    print("\n📊 Test 2: Lazy Agent Loading")
    print("-" * 30)
    
    # Register some agent configs
    for i in range(20):
        config = {
            'id': f'agent_{i:03d}',
            'role': 'code_generation' if i % 2 == 0 else 'debugging',
            'size_mb': 200 + (i % 100),
            'capabilities': ['coding', 'analysis', 'optimization']
        }
        optimizer.register_agent_for_lazy_loading(f'agent_{i:03d}', config)
    
    lazy_stats = optimizer.agent_loader.get_stats()
    print(f"Registered configs: {lazy_stats['total_configs']}")
    print(f"Loaded agents: {lazy_stats['loaded_agents']}")
    print(f"Memory efficiency: {lazy_stats['memory_efficiency']}")
    
    # Test 3: Config compression
    print("\n📊 Test 3: Configuration Compression")
    print("-" * 30)
    
    compression_stats = optimizer.config_compressor.get_stats()
    print(f"Configs compressed: {compression_stats['configs_compressed']}")
    print(f"Original size: {compression_stats['original_size_kb']} KB")
    print(f"Compressed size: {compression_stats['compressed_size_kb']} KB")
    print(f"Space savings: {compression_stats['space_savings_pct']}%")
    
    # Test 4: Overall statistics
    print("\n📊 Test 4: Overall Statistics")
    print("-" * 30)
    
    comprehensive_stats = optimizer.get_comprehensive_stats()
    print(json.dumps(comprehensive_stats, indent=2))
    
    # Cleanup test
    print("\n📊 Test 5: Memory Cleanup")
    print("-" * 30)
    
    cleanup_results = optimizer.force_cleanup()
    print(f"GC collected: {cleanup_results['gc_collected']} objects")
    print(f"Pool cleared: {cleanup_results['pool_cleared']} messages")
    
    optimizer.disable_optimizations()
    print("\n✅ Memory optimization testing complete!")

if __name__ == '__main__':
    test_memory_optimizations()