#!/usr/bin/env python3
"""
Performance Optimization Engine
Buffer pools and optimization system
"""

import os
import sys
import time
import threading
import psutil
from collections import deque
import gc

class PerformanceOptimizer:
    """Performance optimization engine with buffer pools"""
    
    def __init__(self):
        self.initialized = False
        self.buffer_pools = {}
        self.optimization_thread = None
        self.monitoring_active = False
        self.performance_metrics = {
            'cpu_usage': 0,
            'memory_usage': 0,
            'buffer_hits': 0,
            'buffer_misses': 0,
            'optimization_count': 0
        }
        
        print("Performance Optimization Engine created")
    
    def initialize(self):
        """Initialize the performance optimizer"""
        try:
            # Create buffer pools
            self.create_buffer_pools()
            
            # Start monitoring thread
            self.start_monitoring()
            
            self.initialized = True
            print("Performance Optimization Engine initialized successfully")
            return True
            
        except Exception as e:
            print(f"Failed to initialize Performance Optimizer: {e}")
            return False
    
    def create_buffer_pools(self):
        """Create buffer pools for different data types"""
        self.buffer_pools = {
            'string_buffers': deque(maxlen=100),
            'list_buffers': deque(maxlen=50),
            'dict_buffers': deque(maxlen=30),
            'file_buffers': deque(maxlen=20),
            'network_buffers': deque(maxlen=15)
        }
        
        # Pre-allocate some buffers
        for _ in range(10):
            self.buffer_pools['string_buffers'].append("")
            self.buffer_pools['list_buffers'].append([])
            self.buffer_pools['dict_buffers'].append({})
        
        print("Buffer pools created and pre-allocated")
    
    def get_buffer(self, buffer_type):
        """Get a buffer from the pool"""
        if buffer_type in self.buffer_pools and self.buffer_pools[buffer_type]:
            buffer = self.buffer_pools[buffer_type].popleft()
            self.performance_metrics['buffer_hits'] += 1
            return buffer
        else:
            self.performance_metrics['buffer_misses'] += 1
            return self.create_new_buffer(buffer_type)
    
    def return_buffer(self, buffer_type, buffer):
        """Return a buffer to the pool"""
        if buffer_type in self.buffer_pools:
            # Clear the buffer
            if isinstance(buffer, list):
                buffer.clear()
            elif isinstance(buffer, dict):
                buffer.clear()
            elif isinstance(buffer, str):
                buffer = ""
            
            self.buffer_pools[buffer_type].append(buffer)
    
    def create_new_buffer(self, buffer_type):
        """Create a new buffer of the specified type"""
        if buffer_type == 'string_buffers':
            return ""
        elif buffer_type == 'list_buffers':
            return []
        elif buffer_type == 'dict_buffers':
            return {}
        elif buffer_type == 'file_buffers':
            return bytearray(4096)
        elif buffer_type == 'network_buffers':
            return bytearray(8192)
        else:
            return None
    
    def start_monitoring(self):
        """Start performance monitoring thread"""
        if self.monitoring_active:
            return
        
        self.monitoring_active = True
        self.optimization_thread = threading.Thread(target=self.monitor_performance)
        self.optimization_thread.daemon = True
        self.optimization_thread.start()
        
        print("Performance monitoring started")
    
    def stop_monitoring(self):
        """Stop performance monitoring"""
        self.monitoring_active = False
        if self.optimization_thread:
            self.optimization_thread.join(timeout=1)
        
        print("Performance monitoring stopped")
    
    def monitor_performance(self):
        """Monitor system performance and optimize"""
        while self.monitoring_active:
            try:
                # Get system metrics
                self.performance_metrics['cpu_usage'] = psutil.cpu_percent()
                self.performance_metrics['memory_usage'] = psutil.virtual_memory().percent
                
                # Perform optimizations based on metrics
                if self.performance_metrics['cpu_usage'] > 80:
                    self.optimize_cpu_usage()
                
                if self.performance_metrics['memory_usage'] > 85:
                    self.optimize_memory_usage()
                
                # Clean up buffer pools periodically
                if self.performance_metrics['optimization_count'] % 10 == 0:
                    self.cleanup_buffer_pools()
                
                self.performance_metrics['optimization_count'] += 1
                
                time.sleep(5)  # Monitor every 5 seconds
                
            except Exception as e:
                print(f"Performance monitoring error: {e}")
                time.sleep(5)
    
    def optimize_cpu_usage(self):
        """Optimize CPU usage"""
        print("Optimizing CPU usage...")
        
        # Force garbage collection
        gc.collect()
        
        # Reduce buffer pool sizes if CPU is high
        for pool_name, pool in self.buffer_pools.items():
            if len(pool) > 5:
                # Remove some buffers to reduce overhead
                for _ in range(min(3, len(pool) - 5)):
                    try:
                        pool.pop()
                    except IndexError:
                        break
    
    def optimize_memory_usage(self):
        """Optimize memory usage"""
        print("Optimizing memory usage...")
        
        # Force garbage collection
        gc.collect()
        
        # Clear buffer pools if memory is high
        for pool_name, pool in self.buffer_pools.items():
            pool.clear()
        
        # Recreate minimal buffer pools
        self.create_buffer_pools()
    
    def cleanup_buffer_pools(self):
        """Clean up buffer pools"""
        for pool_name, pool in self.buffer_pools.items():
            # Remove empty or unused buffers
            if len(pool) > 10:
                # Keep only the most recent buffers
                while len(pool) > 10:
                    try:
                        pool.pop()
                    except IndexError:
                        break
    
    def get_performance_metrics(self):
        """Get current performance metrics"""
        return self.performance_metrics.copy()
    
    def get_buffer_pool_status(self):
        """Get buffer pool status"""
        status = {}
        for pool_name, pool in self.buffer_pools.items():
            status[pool_name] = {
                'size': len(pool),
                'max_size': pool.maxlen if hasattr(pool, 'maxlen') else 'unlimited'
            }
        return status
    
    def optimize_for_workload(self, workload_type):
        """Optimize for specific workload type"""
        if workload_type == 'cpu_intensive':
            # Reduce buffer pool sizes for CPU-intensive tasks
            for pool in self.buffer_pools.values():
                if len(pool) > 3:
                    while len(pool) > 3:
                        try:
                            pool.pop()
                        except IndexError:
                            break
        
        elif workload_type == 'memory_intensive':
            # Increase buffer pool sizes for memory-intensive tasks
            for pool_name, pool in self.buffer_pools.items():
                target_size = min(20, pool.maxlen if hasattr(pool, 'maxlen') else 20)
                while len(pool) < target_size:
                    pool.append(self.create_new_buffer(pool_name))
        
        elif workload_type == 'io_intensive':
            # Optimize for I/O operations
            self.optimize_io_buffers()
    
    def optimize_io_buffers(self):
        """Optimize I/O buffers"""
        # Increase file and network buffer sizes
        if 'file_buffers' in self.buffer_pools:
            file_pool = self.buffer_pools['file_buffers']
            while len(file_pool) < 10:
                file_pool.append(bytearray(8192))  # Larger buffers for I/O
        
        if 'network_buffers' in self.buffer_pools:
            network_pool = self.buffer_pools['network_buffers']
            while len(network_pool) < 10:
                network_pool.append(bytearray(16384))  # Larger buffers for network
    
    def benchmark_performance(self):
        """Benchmark current performance"""
        start_time = time.time()
        
        # Test buffer operations
        for _ in range(1000):
            buffer = self.get_buffer('string_buffers')
            buffer = "test string"
            self.return_buffer('string_buffers', buffer)
        
        end_time = time.time()
        duration = end_time - start_time
        
        return {
            'buffer_operations_per_second': 1000 / duration,
            'average_operation_time': duration / 1000,
            'buffer_hit_rate': self.performance_metrics['buffer_hits'] / 
                             (self.performance_metrics['buffer_hits'] + self.performance_metrics['buffer_misses'])
        }
    
    def shutdown(self):
        """Shutdown the performance optimizer"""
        self.stop_monitoring()
        self.buffer_pools.clear()
        self.initialized = False
        print("Performance Optimizer shutdown complete")

def main():
    """Test the performance optimizer"""
    print("Testing Performance Optimization Engine...")
    
    optimizer = PerformanceOptimizer()
    
    if optimizer.initialize():
        print("✅ Performance Optimizer initialized")
        
        # Test buffer operations
        print("\nTesting buffer operations...")
        for i in range(5):
            buffer = optimizer.get_buffer('string_buffers')
            buffer = f"test string {i}"
            optimizer.return_buffer('string_buffers', buffer)
        
        # Get metrics
        metrics = optimizer.get_performance_metrics()
        print(f"Performance metrics: {metrics}")
        
        # Get buffer pool status
        status = optimizer.get_buffer_pool_status()
        print(f"Buffer pool status: {status}")
        
        # Benchmark
        benchmark = optimizer.benchmark_performance()
        print(f"Benchmark results: {benchmark}")
        
        # Test workload optimization
        optimizer.optimize_for_workload('cpu_intensive')
        print("✅ CPU-intensive optimization applied")
        
        optimizer.optimize_for_workload('memory_intensive')
        print("✅ Memory-intensive optimization applied")
        
        optimizer.optimize_for_workload('io_intensive')
        print("✅ I/O-intensive optimization applied")
        
        # Shutdown
        optimizer.shutdown()
        print("✅ Performance Optimizer test complete")
    else:
        print("❌ Failed to initialize Performance Optimizer")

if __name__ == "__main__":
    main()
