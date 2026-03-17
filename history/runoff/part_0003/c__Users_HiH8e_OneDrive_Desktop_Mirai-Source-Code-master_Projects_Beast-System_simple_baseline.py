#!/usr/bin/env python3
"""
SIMPLIFIED Beast Swarm Baseline Profiling
=========================================
Focused profiling without complex dependencies
"""

import time
import json
import sys
import os

# Add the current directory to path for imports
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

try:
    from beast_swarm_system import BeastSwarm, MicroAgent, AgentRole
    BEAST_SWARM_AVAILABLE = True
except ImportError as e:
    print(f"⚠️  Beast Swarm import error: {e}")
    BEAST_SWARM_AVAILABLE = False

def measure_memory_simple():
    """Simple memory measurement"""
    try:
        import psutil
        process = psutil.Process(os.getpid())
        return process.memory_info().rss / 1024 / 1024  # MB
    except ImportError:
        return 0

def run_baseline_profiling():
    """Run simplified baseline profiling"""
    print("🔥 BEAST SWARM - SIMPLIFIED BASELINE PROFILING")
    print("=" * 60)
    
    baseline_results = {
        'timestamp': time.strftime('%Y-%m-%d %H:%M:%S'),
        'python_version': sys.version,
        'beast_swarm_available': BEAST_SWARM_AVAILABLE
    }
    
    # Test 1: Basic memory usage
    print("\n📊 Test 1: Basic Memory Usage")
    print("-" * 40)
    
    initial_memory = measure_memory_simple()
    
    # Create some data structures (simulating agents)
    agents_data = []
    for i in range(1000):
        agent_data = {
            'id': f'agent_{i:04d}',
            'role': 'code_generation' if i % 2 == 0 else 'optimization',
            'size_mb': 250 + (i % 100),
            'created': time.time()
        }
        agents_data.append(agent_data)
    
    post_creation_memory = measure_memory_simple()
    memory_increase = post_creation_memory - initial_memory
    
    baseline_results['memory'] = {
        'initial_mb': round(initial_memory, 2),
        'post_creation_mb': round(post_creation_memory, 2),
        'increase_mb': round(memory_increase, 2),
        'agents_created': len(agents_data),
        'memory_per_agent_kb': round((memory_increase * 1024) / len(agents_data), 2)
    }
    
    print(f"✅ Memory Test Results:")
    print(f"   Initial: {baseline_results['memory']['initial_mb']} MB")
    print(f"   After 1000 agents: {baseline_results['memory']['post_creation_mb']} MB")
    print(f"   Increase: {baseline_results['memory']['increase_mb']} MB")
    print(f"   Per agent: {baseline_results['memory']['memory_per_agent_kb']} KB")
    
    # Test 2: CPU Performance (operations per second)
    print("\n📊 Test 2: CPU Performance")
    print("-" * 40)
    
    start_time = time.time()
    operations = 0
    
    # Run for 2 seconds
    while time.time() - start_time < 2.0:
        # Simulate processing
        for agent in agents_data[:100]:  # Process first 100 agents
            _ = agent['id'].split('_')
            _ = agent['role'].upper()
            _ = agent['size_mb'] * 2
        operations += 100
    
    total_time = time.time() - start_time
    ops_per_second = operations / total_time
    
    baseline_results['cpu'] = {
        'duration_seconds': round(total_time, 2),
        'total_operations': operations,
        'ops_per_second': round(ops_per_second, 0)
    }
    
    print(f"✅ CPU Test Results:")
    print(f"   Duration: {baseline_results['cpu']['duration_seconds']}s")
    print(f"   Operations: {baseline_results['cpu']['total_operations']}")
    print(f"   Rate: {baseline_results['cpu']['ops_per_second']} ops/sec")
    
    # Test 3: Beast Swarm Integration (if available)
    print("\n📊 Test 3: Beast Swarm Integration")
    print("-" * 40)
    
    if BEAST_SWARM_AVAILABLE:
        try:
            swarm_start_time = time.time()
            
            # Create a small swarm
            swarm = BeastSwarm(max_agents=10)
            
            for i in range(10):
                agent = MicroAgent(
                    agent_id=f"baseline_agent_{i}",
                    role=AgentRole.CODE_GENERATION,
                    model_size_mb=250
                )
                swarm.add_agent(agent)
            
            swarm_creation_time = time.time() - swarm_start_time
            
            baseline_results['beast_swarm'] = {
                'agents_created': 10,
                'creation_time_seconds': round(swarm_creation_time, 4),
                'agents_per_second': round(10 / swarm_creation_time, 2) if swarm_creation_time > 0 else 0,
                'total_agents_in_swarm': len(swarm.agents) if hasattr(swarm, 'agents') else 'unknown'
            }
            
            print(f"✅ Beast Swarm Test Results:")
            print(f"   Agents created: {baseline_results['beast_swarm']['agents_created']}")
            print(f"   Creation time: {baseline_results['beast_swarm']['creation_time_seconds']}s")
            print(f"   Rate: {baseline_results['beast_swarm']['agents_per_second']} agents/sec")
            
        except Exception as e:
            baseline_results['beast_swarm'] = {'error': str(e)}
            print(f"❌ Beast Swarm test failed: {e}")
    else:
        baseline_results['beast_swarm'] = {'status': 'not_available'}
        print("⚠️  Beast Swarm not available for testing")
    
    # Save results
    print("\n📈 Saving Baseline Results")
    print("-" * 40)
    
    output_file = 'simplified_baseline.json'
    with open(output_file, 'w') as f:
        json.dump(baseline_results, f, indent=2)
    
    print(f"✅ Results saved to: {output_file}")
    
    # Summary
    print(f"\n🎯 BASELINE SUMMARY")
    print("=" * 60)
    print(f"   Memory efficiency: {baseline_results['memory']['memory_per_agent_kb']} KB per agent")
    print(f"   CPU performance: {baseline_results['cpu']['ops_per_second']} ops/sec")
    
    if 'error' not in baseline_results.get('beast_swarm', {}):
        swarm_rate = baseline_results.get('beast_swarm', {}).get('agents_per_second', 0)
        print(f"   Swarm creation: {swarm_rate} agents/sec")
    
    print(f"\n✅ OPTIMIZATION TARGETS:")
    print(f"   🎯 Memory: < {baseline_results['memory']['memory_per_agent_kb'] * 0.85:.1f} KB per agent (15% reduction)")
    print(f"   🎯 CPU: > {baseline_results['cpu']['ops_per_second'] * 1.2:.0f} ops/sec (20% improvement)")
    print(f"   🎯 Swarm: > {baseline_results.get('beast_swarm', {}).get('agents_per_second', 0) * 1.1:.1f} agents/sec (10% improvement)")
    
    print(f"\n🚀 Ready for Phase 2: Memory Optimization!")
    return baseline_results

if __name__ == '__main__':
    try:
        results = run_baseline_profiling()
        print(f"\n✅ Baseline profiling completed successfully!")
    except Exception as e:
        print(f"\n❌ Error during profiling: {e}")
        sys.exit(1)