#!/usr/bin/env python3
"""
PHASE 1: BEAST SWARM BASELINE PROFILING (SIMPLIFIED)
====================================================

Quick profiling without complex dependencies
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

try:
    from beast_swarm_system import BeastSwarm, MicroAgent, AgentRole, TaskPriority
    HAS_SWARM = True
except ImportError as e:
    print(f"⚠️  Could not import BeastSwarm: {e}")
    HAS_SWARM = False


def profile_memory_baseline():
    """Quick memory baseline test"""
    print("\n📊 PHASE 1.1: Memory Profiling")
    print("=" * 60)
    
    tracemalloc.start()
    
    # Create test data structures
    agents = []
    for i in range(100):
        agent_dict = {
            'id': f'beast_{i:03d}',
            'role': 'code_generation',
            'memory_mb': 300,
            'state': 'idle'
        }
        agents.append(agent_dict)
    
    current, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    
    metrics = {
        'current_mb': round(current / 1024 / 1024, 2),
        'peak_mb': round(peak / 1024 / 1024, 2),
        'agents_created': len(agents),
        'per_agent_bytes': round(peak / len(agents))
    }
    
    print(f"✅ Memory Baseline:")
    print(f"   Current: {metrics['current_mb']} MB")
    print(f"   Peak: {metrics['peak_mb']} MB")
    print(f"   Agents: {metrics['agents_created']}")
    print(f"   Per-agent: {metrics['per_agent_bytes']} bytes")
    
    return metrics


def profile_cpu_baseline():
    """Quick CPU baseline test"""
    print("\n📊 PHASE 1.2: CPU Profiling")
    print("=" * 60)
    
    profiler = cProfile.Profile()
    profiler.enable()
    
    # Simulate work
    total_ops = 0
    start_time = time.time()
    while time.time() - start_time < 5:  # 5 second test
        for i in range(1000):
            _ = i * 2
            _ = str(i)
        total_ops += 1000
    
    profiler.disable()
    elapsed = time.time() - start_time
    
    stats = pstats.Stats(profiler)
    
    metrics = {
        'elapsed_seconds': round(elapsed, 2),
        'total_operations': total_ops,
        'ops_per_second': round(total_ops / elapsed, 0),
        'total_function_calls': stats.total_calls
    }
    
    print(f"✅ CPU Baseline:")
    print(f"   Elapsed: {metrics['elapsed_seconds']}s")
    print(f"   Operations: {metrics['total_operations']}")
    print(f"   Ops/sec: {metrics['ops_per_second']}")
    print(f"   Function calls: {metrics['total_function_calls']}")
    
    return metrics


def profile_beast_swarm():
    """Profile Beast Swarm if available"""
    print("\n📊 PHASE 1.3: Beast Swarm Profiling")
    print("=" * 60)
    
    if not HAS_SWARM:
        print("⚠️  BeastSwarm not available, skipping")
        return {}
    
    try:
        swarm = BeastSwarm(max_agents=100)
        
        # Try to add agents
        for i in range(50):
            try:
                agent = MicroAgent(
                    agent_id=f"test_{i}",
                    role=AgentRole.CODE_GENERATION,
                    model_size_mb=300
                )
                swarm.add_agent(agent)
            except Exception as e:
                print(f"⚠️  Could not add agent: {e}")
                break
        
        metrics = {
            'agents_created': len(swarm.agents),
            'swarm_size': len(swarm.agents)
        }
        
        print(f"✅ Beast Swarm Profiling:")
        print(f"   Agents created: {metrics['agents_created']}")
        
        return metrics
        
    except Exception as e:
        print(f"⚠️  BeastSwarm profiling failed: {e}")
        return {}


def main():
    """Run baseline profiling"""
    print("=" * 60)
    print("🐝 BEAST SWARM - PHASE 1: BASELINE PROFILING")
    print("=" * 60)
    print(f"Start: {datetime.now().isoformat()}")
    print(f"Python: {sys.version.split()[0]}")
    
    baseline_metrics = {
        'timestamp': datetime.now().isoformat(),
        'memory': profile_memory_baseline(),
        'cpu': profile_cpu_baseline(),
        'swarm': profile_beast_swarm()
    }
    
    # Save metrics
    output_file = 'baseline_metrics.json'
    with open(output_file, 'w') as f:
        json.dump(baseline_metrics, f, indent=2)
    
    print(f"\n✅ PHASE 1 COMPLETE")
    print(f"📁 Metrics saved to: {output_file}")
    print("\n🎯 NEXT PHASE: Memory/CPU Optimization")
    print("   ├─ Object pooling (1h)")
    print("   ├─ Lazy loading (1h)")
    print("   ├─ Config compression (0.5h)")
    print("   └─ GC tuning (0.5h)")
    print("\n💾 Targets:")
    print("   Memory reduction: 15%+")
    print("   CPU reduction: 20%+")
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
