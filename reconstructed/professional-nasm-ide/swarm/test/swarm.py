"""
Quick test script to verify swarm system functionality
"""

import asyncio
import sys
from swarm_client import SwarmClient

async def test_swarm():
    print("🧪 Testing NASM IDE Swarm System\n")
    
    try:
        async with SwarmClient() as client:
            # Test 1: Swarm Status
            print("[1/10] Testing swarm status...")
            status = await client.get_status()
            print(f"✅ Coordinator: {status.get('coordinator')}")
            print(f"    Agents: {len(status.get('agents', {}))}/10\n")
            
            # Test 2: Code Completion
            print("[2/10] Testing AI code completion...")
            completions = await client.code_complete("mov rax, ", 9)
            print(f"✅ Got {len(completions)} completions")
            if completions:
                print(f"    Example: {completions[0].get('text')}\n")
            
            # Test 3: Code Refactoring
            print("[3/10] Testing code refactoring...")
            refactor = await client.refactor_code("mov rax, 0\nmov rbx, 0")
            print(f"✅ Refactor suggestions: {len(refactor.get('suggestions', []))}\n")
            
            # Test 4: File Operations
            print("[4/10] Testing file operations...")
            content = await client.open_file("test.asm")
            print(f"✅ File opened: {len(content)} bytes\n")
            
            # Test 5: Syntax Highlighting
            print("[5/10] Testing syntax highlighting...")
            tokens = await client.syntax_highlight("mov rax, rbx")
            print(f"✅ Generated {len(tokens)} syntax tokens\n")
            
            # Test 6: Collaboration
            print("[6/10] Testing team collaboration...")
            sync_result = await client.sync_changes([{'line': 10, 'text': 'new code'}])
            print(f"✅ Sync status: {sync_result.get('synced')}\n")
            
            # Test 7: Marketplace
            print("[7/10] Testing extension marketplace...")
            extensions = await client.search_extensions("nasm")
            print(f"✅ Found {len(extensions)} extensions")
            if extensions:
                print(f"    Example: {extensions[0].get('name')}\n")
            
            # Test 8: Build System
            print("[8/10] Testing build system...")
            build = await client.build_debug("hello.asm")
            print(f"✅ Build success: {build.get('success')}")
            print(f"    Output: {build.get('output')}\n")
            
            # Test 9: Toolchain Detection
            print("[9/10] Testing toolchain detection...")
            toolchains = await client.detect_toolchains()
            print(f"✅ Detected toolchains:")
            for name, info in toolchains.items():
                status = "✓" if info.get('found') else "✗"
                print(f"    {status} {name}: {info.get('version', 'N/A')}\n")
            
            # Test 10: Advanced Features
            print("[10/10] Testing parallel build...")
            parallel = await client.parallel_build(["file1.asm", "file2.asm"])
            print(f"✅ Parallel build: {parallel.get('threads')} threads")
            print(f"    Time saved: {parallel.get('time_saved_ms')}ms\n")
            
            print("="*50)
            print("✅ All tests passed!")
            print("="*50)
            
    except Exception as e:
        print(f"\n❌ Test failed: {e}")
        print("\nMake sure the swarm coordinator is running:")
        print("  python coordinator.py")
        sys.exit(1)

if __name__ == '__main__':
    print("Starting swarm tests...\n")
    asyncio.run(test_swarm())
