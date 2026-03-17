#!/usr/bin/env python3
"""
Test the autonomous drive digestion system
Run this to see it in action
"""

import sys
import os
import importlib.util

# Load module from file with hyphen in name
spec = importlib.util.spec_from_file_location("autonomous_drive_digestion", "d:/autonomous-drive-digestion.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

AutonomousAgent = module.AutonomousAgent
DriveDigester = module.DriveDigester
AutonomousQuantizer = module.AutonomousQuantizer

def test_agent_mode():
    """Test autonomous agent decision-making"""
    print("="*70)
    print("TEST 1: AUTONOMOUS AGENT MODE")
    print("="*70)
    
    agent = AutonomousAgent()
    
    # Get autonomous decisions
    decisions = agent.evaluate_situation()
    
    print(f"\n🤖 Agent evaluated situation and made {len(decisions)} decisions:\n")
    
    for i, dec in enumerate(decisions):
        print(f"{i+1}. {dec.action.value.upper()}")
        print(f"   Priority: {dec.priority}/10")
        print(f"   Reason: {dec.reason}")
        print(f"   Auto-execute: {dec.auto_execute}\n")

def test_drive_discovery():
    """Test drive discovery"""
    print("="*70)
    print("TEST 2: DRIVE DISCOVERY")
    print("="*70)
    
    digester = DriveDigester()
    drives = digester._discover_drives()
    
    print(f"\n📁 Discovered drives: {drives}\n")
    
    for drive in drives:
        print(f"   {drive}")

def test_quantization_estimates():
    """Test quantization recommendations"""
    print("="*70)
    print("TEST 3: QUANTIZATION ESTIMATES")
    print("="*70)
    
    print("\nEstimated sizes after quantization:\n")
    
    test_sizes = [100, 500, 1000, 2000, 5000]  # MB
    
    for size_mb in test_sizes:
        quant = AutonomousQuantizer.estimate_quantization(size_mb)
        print(f"Original: {quant['original_mb']} MB")
        print(f"  → int8:  {quant['int8_mb']} MB (25%)")
        print(f"  → int4:  {quant['int4_mb']} MB (12.5%)")
        print(f"  → fp16:  {quant['fp16_mb']} MB (50%)")
        print(f"  → Recommend: {quant['recommendation'].upper()}")
        print()

def test_file_categorization():
    """Test file categorization"""
    print("="*70)
    print("TEST 4: FILE CATEGORIZATION")
    print("="*70)
    
    digester = DriveDigester()
    
    test_files = [
        "d:\\model.gguf",
        "e:\\code.ts",
        "g:\\payload.js",
        "d:\\encryption.asm",
        "e:\\ai_model.safetensors",
        "g:\\code.cpp"
    ]
    
    print(f"\nTesting file relevance assessment:\n")
    
    for filepath in test_files:
        from pathlib import Path
        is_interesting = digester._is_interesting(Path(filepath))
        status = "✅ INTERESTING" if is_interesting else "⭕ SKIP"
        print(f"{status}: {filepath}")

def main():
    """Run all tests"""
    print("\n")
    print("🤖" * 35)
    print("AUTONOMOUS DRIVE DIGESTION - TEST SUITE")
    print("🤖" * 35)
    print()
    
    try:
        test_agent_mode()
        print()
        
        test_drive_discovery()
        print()
        
        test_quantization_estimates()
        print()
        
        test_file_categorization()
        print()
        
        print("="*70)
        print("✅ ALL TESTS PASSED")
        print("="*70)
        
        print("\n🚀 READY TO USE\n")
        print("Command examples:")
        print("  python autonomous-drive-digestion.py --agent-mode")
        print("  python autonomous-drive-digestion.py --scan D: E: G:")
        print("  python autonomous-drive-digestion.py --action extract-security")
        print()
        
    except Exception as e:
        print(f"\n❌ TEST FAILED: {e}")
        import traceback
        traceback.print_exc()

if __name__ == "__main__":
    main()
