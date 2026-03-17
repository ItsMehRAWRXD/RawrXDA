#!/usr/bin/env python3
"""
Model Chaining Examples - Practical usage patterns for the chaining system
Shows different ways to use model chains in your agentic coding workflows
"""

import asyncio
from pathlib import Path
from swarm.chain_controller import (
    ChainController,
    QuickChainExecutor,
    get_chain_controller,
    initialize_chain_controller
)


async def example_1_quick_review():
    """Example 1: Quick code review using CLI"""
    print("\n" + "="*70)
    print("EXAMPLE 1: Quick Code Review")
    print("="*70)
    print("""
    Use CLI for quick reviews:
    
    $ python chain_cli.py review mycode.py
    $ python chain_cli.py review mycode.py -o report.json
    
    This runs: analyzer → validator → optimizer → reviewer
    """)


async def example_2_programmatic_chain():
    """Example 2: Execute chain programmatically"""
    print("\n" + "="*70)
    print("EXAMPLE 2: Programmatic Chain Execution")
    print("="*70)
    
    controller = ChainController()
    
    # Example code
    code = """
def fibonacci(n):
    if n <= 1:
        return n
    return fibonacci(n-1) + fibonacci(n-2)

result = fibonacci(30)
print(result)
""" * 20  # Repeat to create more lines
    
    print("\n📝 Executing code_review_chain on code...")
    
    execution = await controller.execute_chain_on_code(
        chain_id="code_review_chain",
        code=code,
        language="python"
    )
    
    report = controller.get_execution_report(execution.execution_id)
    print(f"\n✓ Execution complete!")
    print(f"  Status: {report['status']}")
    print(f"  Duration: {report['duration_seconds']:.1f}s")
    print(f"  Processed: {report['processed_chunks']}/{report['total_chunks']} chunks")
    print(f"  Success Rate: {report['success_rate']}")


async def example_3_custom_chain():
    """Example 3: Create and use custom chain"""
    print("\n" + "="*70)
    print("EXAMPLE 3: Custom Chain Creation")
    print("="*70)
    
    controller = ChainController()
    
    # Create custom chain
    custom_chain = controller.create_custom_chain(
        chain_id="my_review_chain",
        name="My Custom Review Chain",
        agent_roles=["Analyzer", "Optimizer", "Reviewer"],
        chunk_size=500,
        feedback_loops=1,
        tags=["custom", "review"]
    )
    
    print(f"\n✓ Created chain: {custom_chain.chain_id}")
    print(f"  Models: {' → '.join([m['role'].upper() for m in custom_chain.models])}")
    
    # Now use it
    code = "print('hello')\n" * 100
    execution = await controller.execute_chain_on_code(
        "my_review_chain",
        code,
        language="python"
    )
    
    print(f"✓ Chain executed: {execution.status}")


async def example_4_security_audit():
    """Example 4: Security audit with multiple feedback loops"""
    print("\n" + "="*70)
    print("EXAMPLE 4: Security Audit with Feedback Loops")
    print("="*70)
    
    controller = ChainController()
    executor = QuickChainExecutor(controller)
    
    # Suspicious code
    code = """
import pickle
data = pickle.loads(user_input)  # Dangerous!

password = input("Enter password: ")
exec(f"var = {password}")  # Code injection!

with open(config_file) as f:
    settings = eval(f.read())  # Eval is dangerous!
""" * 10
    
    print("\n🔒 Running security audit...")
    report = await executor.secure_code(code)
    
    print(f"\n✓ Audit complete!")
    print(f"  Status: {report['status']}")
    print(f"  Duration: {report['duration_seconds']:.1f}s")
    print(f"  Chunks analyzed: {report['processed_chunks']}")


async def example_5_documentation():
    """Example 5: Auto-generate documentation"""
    print("\n" + "="*70)
    print("EXAMPLE 5: Auto-Generate Documentation")
    print("="*70)
    
    controller = ChainController()
    executor = QuickChainExecutor(controller)
    
    # Code that needs documentation
    code = """
def calculate_hash(data, algorithm='sha256'):
    import hashlib
    h = hashlib.new(algorithm)
    h.update(data.encode())
    return h.hexdigest()

def validate_email(email):
    import re
    pattern = r'^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$'
    return re.match(pattern, email) is not None

def parse_config(config_str):
    import json
    try:
        return json.loads(config_str)
    except json.JSONDecodeError as e:
        return None
""" * 8
    
    print("\n📚 Generating documentation...")
    report = await executor.document_code(code)
    
    print(f"\n✓ Documentation generated!")
    print(f"  Duration: {report['duration_seconds']:.1f}s")
    print(f"  Chunks processed: {report['processed_chunks']}")
    
    # Extract documentation results
    doc_results = [
        r for r in report['results'] 
        if r['agent_role'] == 'documenter'
    ]
    print(f"  Documentation segments: {len(doc_results)}")


async def example_6_file_processing():
    """Example 6: Process actual file with chain"""
    print("\n" + "="*70)
    print("EXAMPLE 6: Process File with Chain")
    print("="*70)
    
    controller = ChainController()
    
    # Create example file
    example_file = Path("example_code.py")
    example_file.write_text("""
def merge_sort(arr):
    if len(arr) <= 1:
        return arr
    mid = len(arr) // 2
    left = merge_sort(arr[:mid])
    right = merge_sort(arr[mid:])
    return merge(left, right)

def merge(left, right):
    result = []
    i = j = 0
    while i < len(left) and j < len(right):
        if left[i] <= right[j]:
            result.append(left[i])
            i += 1
        else:
            result.append(right[j])
            j += 1
    return result + left[i:] + right[j:]
""" * 5)
    
    print(f"\n📄 Processing file: {example_file.name}")
    
    execution = await controller.execute_chain_on_file(
        chain_id="code_review_chain",
        file_path=str(example_file),
        language="python"
    )
    
    report = controller.get_execution_report(execution.execution_id)
    print(f"\n✓ File processed!")
    print(f"  Status: {report['status']}")
    print(f"  Chunks: {report['processed_chunks']}/{report['total_chunks']}")
    
    # Cleanup
    example_file.unlink()


async def example_7_performance_optimization():
    """Example 7: Optimize slow code"""
    print("\n" + "="*70)
    print("EXAMPLE 7: Performance Optimization")
    print("="*70)
    
    controller = ChainController()
    executor = QuickChainExecutor(controller)
    
    # Slow code
    slow_code = """
def find_duplicates(arr):
    duplicates = []
    for i in range(len(arr)):
        for j in range(i + 1, len(arr)):
            if arr[i] == arr[j]:
                duplicates.append(arr[i])
    return duplicates

def is_sorted(arr):
    for i in range(len(arr) - 1):
        if arr[i] > arr[i + 1]:
            return False
    return True

def linear_search(arr, target):
    for i in range(len(arr)):
        if arr[i] == target:
            return i
    return -1
""" * 15
    
    print("\n⚡ Running performance optimization...")
    report = await executor.optimize_performance(slow_code)
    
    print(f"\n✓ Optimization complete!")
    print(f"  Duration: {report['duration_seconds']:.1f}s")
    print(f"  Results: {report['results_count']}")
    
    # Extract optimization suggestions
    opt_results = [
        r for r in report['results']
        if r['agent_role'] == 'optimizer'
    ]
    print(f"  Optimization suggestions: {len(opt_results)}")


async def example_8_chaining_chains():
    """Example 8: Execute multiple chains in sequence"""
    print("\n" + "="*70)
    print("EXAMPLE 8: Chaining Multiple Chains")
    print("="*70)
    
    controller = ChainController()
    
    code = "def hello():\n    print('world')\n" * 50
    
    print("\n🔄 Sequential chain execution:")
    print("  1. Security analysis...")
    
    exec1 = await controller.execute_chain_on_code(
        "secure_coding_chain",
        code,
        language="python"
    )
    
    print("  2. Code optimization...")
    exec2 = await controller.execute_chain_on_code(
        "optimization_chain",
        code,
        language="python"
    )
    
    print("  3. Documentation...")
    exec3 = await controller.execute_chain_on_code(
        "documentation_chain",
        code,
        language="python"
    )
    
    print("\n✓ All chains complete!")
    print(f"  Security: {exec1.status} ({exec1.duration_seconds():.1f}s)")
    print(f"  Optimization: {exec2.status} ({exec2.duration_seconds():.1f}s)")
    print(f"  Documentation: {exec3.status} ({exec3.duration_seconds():.1f}s)")


async def example_9_batch_processing():
    """Example 9: Batch process multiple files"""
    print("\n" + "="*70)
    print("EXAMPLE 9: Batch File Processing")
    print("="*70)
    
    controller = ChainController()
    
    # Create example files
    files = []
    for i in range(3):
        path = Path(f"example_{i}.py")
        path.write_text(f"# File {i}\ndef func{i}():\n    return {i}\n" * 20)
        files.append(path)
    
    print(f"\n📦 Processing {len(files)} files...")
    
    results = []
    for file_path in files:
        execution = await controller.execute_chain_on_file(
            "code_review_chain",
            str(file_path)
        )
        results.append({
            "file": file_path.name,
            "status": execution.status,
            "duration": execution.duration_seconds()
        })
        print(f"  ✓ {file_path.name}")
    
    # Summary
    print(f"\n📊 Batch Results:")
    total_time = sum(r["duration"] for r in results)
    print(f"  Total files: {len(results)}")
    print(f"  Total time: {total_time:.1f}s")
    print(f"  Average per file: {total_time/len(results):.1f}s")
    
    # Cleanup
    for path in files:
        path.unlink()


async def example_10_report_export():
    """Example 10: Export detailed reports"""
    print("\n" + "="*70)
    print("EXAMPLE 10: Export Detailed Reports")
    print("="*70)
    
    controller = ChainController()
    
    code = "print('test')\n" * 30
    
    execution = await controller.execute_chain_on_code(
        "code_review_chain",
        code
    )
    
    # Export report
    report_path = "chain_report.json"
    controller.export_report(execution.execution_id, report_path)
    
    print(f"\n✓ Report exported to: {report_path}")
    
    report = controller.get_execution_report(execution.execution_id)
    print(f"\nReport Summary:")
    print(f"  Execution ID: {execution.execution_id}")
    print(f"  Status: {report['status']}")
    print(f"  Chunks: {report['processed_chunks']}/{report['total_chunks']}")
    print(f"  Results: {report['results_count']}")
    print(f"  Duration: {report['duration_seconds']:.1f}s")


async def main():
    """Run all examples"""
    print("\n" + "="*70)
    print("🚀 MODEL CHAINING EXAMPLES".center(70))
    print("="*70)
    
    examples = [
        ("Quick Review (CLI)", example_1_quick_review),
        ("Programmatic Execution", example_2_programmatic_chain),
        ("Custom Chains", example_3_custom_chain),
        ("Security Audit", example_4_security_audit),
        ("Documentation Generation", example_5_documentation),
        ("File Processing", example_6_file_processing),
        ("Performance Optimization", example_7_performance_optimization),
        ("Chaining Chains", example_8_chaining_chains),
        ("Batch Processing", example_9_batch_processing),
        ("Report Export", example_10_report_export),
    ]
    
    # Show menu
    print("\nAvailable Examples:")
    for i, (name, _) in enumerate(examples, 1):
        print(f"  {i}. {name}")
    print(f"  0. Run all examples")
    print(f"  q. Quit")
    
    # For automated run, execute first example
    print("\n(Executing first example as demonstration...)\n")
    await example_2_programmatic_chain()
    
    print("\n\nTo run all examples, modify the script to call each example.")
    print("Each example is ready to use!")


if __name__ == "__main__":
    asyncio.run(main())
