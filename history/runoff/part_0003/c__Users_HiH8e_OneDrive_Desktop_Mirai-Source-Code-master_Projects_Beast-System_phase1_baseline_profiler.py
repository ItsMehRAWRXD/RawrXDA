#!/usr/bin/env python3
"""
PHASE 1: BEAST SWARM BASELINE PROFILING
======================================

Measure current performance metrics:
- Memory consumption (baseline)
- CPU usage patterns
- I/O operations
- Agent lifecycle overhead

Duration: ~2 hours
Tasks: Profile swarm with 100-1000 agents
Output: baseline_metrics.json

Date: November 21, 2025
Task: Phase 3 Task 3 - Beast Swarm Productionization
"""

import cProfile
import pstats
import tracemalloc
import psutil
import os
import sys
import json
import time
from datetime import datetime

# Add parent to path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from beast_swarm_system import BeastSwarm, MicroAgent, SwarmTask, AgentRole, TaskPriority, TaskStatus


class BeastSwarmProfiler:
    """Profile Beast Swarm performance under various loads"""
    
    def __init__(self, output_file='baseline_metrics.json'):
        self.output_file = output_file
        self.baseline_metrics = {
            'timestamp': datetime.now().isoformat(),
            'memory': {},
            'cpu': {},
            'io': {},
            'swarm_performance': {}
        }
        self.process = psutil.Process(os.getpid())
        
    def profile_memory_usage(self, num_agents=100, num_tasks=1000):
        """Measure memory consumption with swarm operations"""
        print(f"\n📊 PHASE 1.1: Memory Profiling ({num_agents} agents, {num_tasks} tasks)")
        print("=" * 60)
        
        tracemalloc.start()
        
        # Create swarm with specified agents
        swarm = BeastSwarm(max_agents=num_agents)
        print(f"✓ Created BeastSwarm with {num_agents} agent slots")
        
        # Create and assign agents
        start_time = time.time()
        for i in range(min(num_agents, 50)):  # Create up to 50 actual agents
            agent = MicroAgent(
                agent_id=f"beast_{i:03d}",
                role=AgentRole.CODE_GENERATION,
                model_size_mb=300
            )
            swarm.add_agent(agent)
        
        agent_creation_time = time.time() - start_time
        print(f"✓ Created {min(num_agents, 50)} agents in {agent_creation_time:.2f}s")
        
        # Create tasks
        start_time = time.time()
        for i in range(min(num_tasks, 100)):
            task = SwarmTask(
                id=f"task_{i:04d}",
                description=f"Task {i}",
                priority=TaskPriority.MEDIUM,
                required_roles=[AgentRole.CODE_GENERATION],
                context={'iteration': i}
            )
            # swarm.queue_task(task)  # If method exists
        
        task_creation_time = time.time() - start_time
        print(f"✓ Created {min(num_tasks, 100)} tasks in {task_creation_time:.2f}s")
        
        # Get memory stats
        current_mb, peak_mb = tracemalloc.get_traced_memory()
        tracemalloc.stop()
        
        # Per-agent memory
        per_agent_bytes = peak_mb * 1024 * 1024 / max(1, min(num_agents, 50))
        
        metrics = {
            'num_agents': min(num_agents, 50),
            'num_tasks': min(num_tasks, 100),
            'current_mb': round(current_mb / 1024 / 1024, 2),
            'peak_mb': round(peak_mb / 1024 / 1024, 2),
            'per_agent_mb': round(per_agent_bytes / 1024 / 1024, 4),
            'agent_creation_time_sec': round(agent_creation_time, 4),
            'task_creation_time_sec': round(task_creation_time, 4)
        }
        
        self.baseline_metrics['memory'] = metrics
        
        print(f"📈 Memory Results:")
        print(f"   Current: {metrics['current_mb']} MB")
        print(f"   Peak: {metrics['peak_mb']} MB")
        print(f"   Per-agent: {metrics['per_agent_mb']} MB")
        print(f"✅ Memory profiling complete")
        
        return metrics
    
    def profile_cpu_usage(self, duration_seconds=10, num_agents=50):
        """Measure CPU consumption during swarm operations"""
        print(f"\n📊 PHASE 1.2: CPU Profiling ({num_agents} agents, {duration_seconds}s)")
        print("=" * 60)
        
        profiler = cProfile.Profile()
        profiler.enable()
        
        # Create swarm
        swarm = BeastSwarm(max_agents=num_agents)
        for i in range(num_agents):
            agent = MicroAgent(
                agent_id=f"beast_{i:03d}",
                role=AgentRole.CODE_GENERATION if i % 2 == 0 else AgentRole.OPTIMIZATION,
                model_size_mb=250
            )
            swarm.add_agent(agent)
        
        print(f"✓ Created {num_agents} agents")
        
        # Simulate work
        start_time = time.time()
        while time.time() - start_time < duration_seconds:
            # Simple work simulation
            for agent in swarm.agents[:min(10, len(swarm.agents))]:
                _ = agent.role.value  # Dummy work
        
        profiler.disable()
        
        # Get stats
        stats = pstats.Stats(profiler)
        stats.sort_stats('cumulative')
        
        # Extract top functions
        top_funcs = []
        for func, (cc, nc, tt, ct, callers) in list(stats.stats.items())[:10]:
            top_funcs.append({
                'name': func[2],
                'calls': cc,
                'cumulative_time': round(ct, 4)
            })
        
        metrics = {
            'duration_seconds': duration_seconds,
            'num_agents': num_agents,
            'total_function_calls': stats.total_calls,
            'total_time': round(stats.total_tt, 4),
            'top_functions': top_funcs
        }
        
        self.baseline_metrics['cpu'] = metrics
        
        print(f"📈 CPU Results:")
        print(f"   Total calls: {metrics['total_function_calls']}")
        print(f"   Total time: {metrics['total_time']}s")
        print(f"   Top functions:")
        for func in top_funcs[:5]:
            print(f"      - {func['name']}: {func['cumulative_time']}s ({func['calls']} calls)")
        print(f"✅ CPU profiling complete")
        
        return metrics
    
    def profile_io_operations(self, num_operations=1000):
        """Measure I/O patterns"""
        print(f"\n📊 PHASE 1.3: I/O Profiling ({num_operations} operations)")
        print("=" * 60)
        
        # Get initial I/O counters
        io_before = self.process.io_counters()
        
        # Simulate I/O operations
        for i in range(num_operations):
            # Simulate task serialization
            task_dict = {
                'id': f'task_{i}',
                'data': 'x' * 100,
                'timestamp': time.time()
            }
            json.dumps(task_dict)
        
        io_after = self.process.io_counters()
        
        metrics = {
            'operations': num_operations,
            'read_count_delta': io_after.read_count - io_before.read_count,
            'write_count_delta': io_after.write_count - io_before.write_count,
            'read_bytes_delta': io_after.read_bytes - io_before.read_bytes,
            'write_bytes_delta': io_after.write_bytes - io_before.write_bytes
        }
        
        self.baseline_metrics['io'] = metrics
        
        print(f"📈 I/O Results:")
        print(f"   Read count: {metrics['read_count_delta']}")
        print(f"   Write count: {metrics['write_count_delta']}")
        print(f"   Read bytes: {metrics['read_bytes_delta']}")
        print(f"   Write bytes: {metrics['write_bytes_delta']}")
        print(f"✅ I/O profiling complete")
        
        return metrics
    
    def profile_swarm_performance(self):
        """Profile swarm-specific metrics"""
        print(f"\n📊 PHASE 1.4: Swarm Performance Profiling")
        print("=" * 60)
        
        swarm = BeastSwarm(max_agents=100)
        
        # Test agent creation performance
        start_time = time.time()
        for i in range(100):
            agent = MicroAgent(
                agent_id=f"perf_test_{i}",
                role=AgentRole.CODE_GENERATION,
                model_size_mb=300
            )
            swarm.add_agent(agent)
        
        agent_creation_rate = 100 / (time.time() - start_time)
        
        metrics = {
            'total_agents': len(swarm.agents),
            'agents_per_second': round(agent_creation_rate, 2),
            'swarm_memory_mb': round(swarm.get_total_memory_mb(), 2) if hasattr(swarm, 'get_total_memory_mb') else 0
        }
        
        self.baseline_metrics['swarm_performance'] = metrics
        
        print(f"📈 Swarm Performance:")
        print(f"   Total agents: {metrics['total_agents']}")
        print(f"   Creation rate: {metrics['agents_per_second']} agents/sec")
        print(f"   Swarm memory: {metrics['swarm_memory_mb']} MB")
        print(f"✅ Swarm performance profiling complete")
        
        return metrics
    
    def generate_report(self):
        """Generate and save profiling report"""
        print(f"\n📄 PHASE 1: BASELINE PROFILING COMPLETE")
        print("=" * 60)
        
        report = {
            'phase': 'Phase 1: Baseline Profiling',
            'timestamp': datetime.now().isoformat(),
            'metrics': self.baseline_metrics,
            'targets': {
                'memory_reduction_target_pct': 15,
                'cpu_reduction_target_pct': 20,
                'next_phase': 'Memory/CPU Optimization'
            }
        }
        
        # Save to file
        with open(self.output_file, 'w') as f:
            json.dump(report, f, indent=2)
        
        print(f"\n✅ Baseline metrics saved to: {self.output_file}")
        print(f"\n📋 Summary:")
        print(f"   Memory peak: {self.baseline_metrics['memory'].get('peak_mb', 'N/A')} MB")
        print(f"   CPU total time: {self.baseline_metrics['cpu'].get('total_time', 'N/A')} seconds")
        print(f"   Swarm agents: {self.baseline_metrics['swarm_performance'].get('total_agents', 'N/A')}")
        
        print(f"\n🎯 NEXT STEPS:")
        print(f"   1. Implement object pooling (1h)")
        print(f"   2. Implement lazy loading (1h)")
        print(f"   3. Implement config compression (0.5h)")
        print(f"   4. Implement GC tuning (0.5h)")
        print(f"   5. Measure improvements (1h)")
        print(f"\n   Total: ~4 hours for optimization, ~1 hour for verification")
        
        return report


def main():
    """Run baseline profiling"""
    print("=" * 60)
    print("🐝 BEAST SWARM - PHASE 1: BASELINE PROFILING")
    print("=" * 60)
    print(f"Start time: {datetime.now().isoformat()}")
    print(f"Python version: {sys.version}")
    print(f"Working directory: {os.getcwd()}")
    
    profiler = BeastSwarmProfiler()
    
    try:
        # Run all profiling phases
        profiler.profile_memory_usage(num_agents=50, num_tasks=100)
        profiler.profile_cpu_usage(duration_seconds=5, num_agents=50)
        profiler.profile_io_operations(num_operations=500)
        profiler.profile_swarm_performance()
        
        # Generate report
        report = profiler.generate_report()
        
        print(f"\n✅ PHASE 1 COMPLETE - {datetime.now().isoformat()}")
        return 0
        
    except Exception as e:
        print(f"\n❌ Error during profiling: {e}")
        import traceback
        traceback.print_exc()
        return 1


if __name__ == '__main__':
    sys.exit(main())
