#!/usr/bin/env python3
"""
PHASE 1: BEAST SWARM BASELINE - MINIMAL VERSION
===============================================
"""

import tracemalloc
import psutil
import os
import sys
import json
import time
from datetime import datetime

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

def profile_memory():
    """Memory baseline"""
    print("\n📊 Memory Profiling")
    print("-" * 40)
    
    tracemalloc.start()
    
    # Create 100 agent dictionaries
    agents = [{'id': f'agent_{i}', 'role': 'code', 'size_mb': 300} for i in range(100)]
    
    current, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    
    print(f"✅ Peak memory: {peak / 1024 / 1024:.2f} MB")
    print(f"   Per-agent: {peak / 100 / 1024:.2f} KB")
    return {'peak_mb': round(peak / 1024 / 1024, 2)}

def profile_cpu():
    """CPU baseline"""
    print("\n📊 CPU Profiling")
    print("-" * 40)
    
    start = time.time()
    count = 0
    while time.time() - start < 3:
        for _ in range(10000):
            count += 1
    
    elapsed = time.time() - start
    ops_sec = count / elapsed
    print(f"✅ Operations/sec: {ops_sec:.0f}")
    return {'ops_per_sec': round(ops_sec)}

def profile_swarm():
    """Beast Swarm baseline"""
    print("\n📊 Beast Swarm Profiling")
    print("-" * 40)
    
    try:
        from beast_swarm_system import BeastSwarm, MicroAgent, AgentRole
        
        swarm = BeastSwarm(max_agents=100)
        for i in range(50):
            agent = MicroAgent(f"agent_{i}", AgentRole.CODE_GENERATION, 300)
            swarm.add_agent(agent)
        
        print(f"✅ Agents created: {len(swarm.agents)}")
        return {'agents': len(swarm.agents)}
    except Exception as e:
        print(f"⚠️  Error: {e}")
        return {}

def main():
    print("=" * 60)
    print("🐝 PHASE 1: BASELINE PROFILING")
    print("=" * 60)
    
    metrics = {
        'timestamp': datetime.now().isoformat(),
        'memory': profile_memory(),
        'cpu': profile_cpu(),
        'swarm': profile_swarm()
    }
    
    with open('baseline_metrics.json', 'w') as f:
        json.dump(metrics, f, indent=2)
    
    print("\n" + "=" * 60)
    print("✅ PHASE 1 COMPLETE")
    print(f"📁 Saved: baseline_metrics.json")
    print("=" * 60)
    print("\n🎯 Next: Phase 1.1 Memory Optimization")
    
    return 0

if __name__ == '__main__':
    sys.exit(main())
