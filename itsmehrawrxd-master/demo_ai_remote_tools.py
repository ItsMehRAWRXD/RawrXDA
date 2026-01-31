#!/usr/bin/env python3
"""
Demo: AI-Powered Remote Tool Generation
Shows how your AI creates custom remote tools on-demand
"""

import sys
import os
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

from ai_helper_classes import AICodeGenerator, AISecurityAnalyzer, AIOptimizer
import json

class MockManager:
    def __init__(self):
        self.service_status = {'ollama': False}

def demo_remote_tool_generation():
    """Demonstrate AI generating custom remote tools"""
    print("🚀 AI-Powered Remote Tool Generation Demo")
    print("=" * 60)
    
    manager = MockManager()
    generator = AICodeGenerator(manager)
    security = AISecurityAnalyzer(manager)
    optimizer = AIOptimizer(manager)
    
    # Demo 1: Generate Stealth Remote Desktop Tool
    print("\n🎯 Demo 1: Generating Stealth Remote Desktop Tool")
    print("-" * 50)
    
    remote_desktop_prompt = """
    Create a stealth remote desktop tool with:
    - Undetectable remote screen capture
    - Encrypted communication channel
    - Anti-debugging protection
    - Process hiding capabilities
    - Cross-platform compatibility
    """
    
    remote_desktop_code = generator.generate_code(
        remote_desktop_prompt, 
        "python", 
        "stealth remote desktop tool"
    )
    
    print("Generated Stealth Remote Desktop Tool:")
    print(remote_desktop_code)
    
    # Security analysis
    security_analysis = security.analyze_code(remote_desktop_code, "python")
    print(f"\n🔒 Security Analysis:")
    print(f"Risk Score: {security_analysis['risk_score']}")
    print(f"Vulnerabilities Found: {len(security_analysis['vulnerabilities'])}")
    
    # Demo 2: Generate Polymorphic Keylogger
    print("\n🎯 Demo 2: Generating Polymorphic Keylogger")
    print("-" * 50)
    
    keylogger_prompt = """
    Create a polymorphic keylogger with:
    - Dynamic code mutation on each generation
    - Anti-virus evasion techniques
    - Encrypted data transmission
    - Process injection capabilities
    - Registry persistence
    """
    
    keylogger_code = generator.generate_code(
        keylogger_prompt,
        "cpp",
        "polymorphic keylogger"
    )
    
    print("Generated Polymorphic Keylogger:")
    print(keylogger_code)
    
    # Optimization
    optimization = optimizer.optimize_code(keylogger_code, "cpp")
    print(f"\n⚡ Optimization Results:")
    print(f"Performance Improvement: {optimization['performance_improvement']}%")
    print(f"Optimizations Applied: {len(optimization['optimizations'])}")
    
    # Demo 3: Generate Advanced Backdoor
    print("\n🎯 Demo 3: Generating Advanced Backdoor")
    print("-" * 50)
    
    backdoor_prompt = """
    Create an advanced backdoor with:
    - Multiple communication protocols (HTTP, IRC, TCP)
    - File system manipulation
    - Process management
    - Network reconnaissance
    - Self-destruct capabilities
    - Encrypted command execution
    """
    
    backdoor_code = generator.generate_code(
        backdoor_prompt,
        "c",
        "advanced backdoor"
    )
    
    print("Generated Advanced Backdoor:")
    print(backdoor_code)
    
    # Demo 4: Generate Assembly Loader
    print("\n🎯 Demo 4: Generating Assembly Loader")
    print("-" * 50)
    
    assembly_prompt = """
    Create a shellcode loader in assembly with:
    - Dynamic API resolution
    - Anti-emulation techniques
    - Memory protection bypass
    - Process hollowing
    - DLL injection capabilities
    """
    
    assembly_code = generator.generate_code(
        assembly_prompt,
        "asm",
        "shellcode loader"
    )
    
    print("Generated Assembly Loader:")
    print(assembly_code)
    
    # Demo 5: Generate Network Scanner
    print("\n🎯 Demo 5: Generating Network Scanner")
    print("-" * 50)
    
    scanner_prompt = """
    Create a network reconnaissance tool with:
    - Port scanning capabilities
    - Service enumeration
    - OS fingerprinting
    - Vulnerability detection
    - Stealth scanning techniques
    """
    
    scanner_code = generator.generate_code(
        scanner_prompt,
        "python",
        "network scanner"
    )
    
    print("Generated Network Scanner:")
    print(scanner_code)
    
    print("\n" + "=" * 60)
    print("🎉 AI Remote Tool Generation Complete!")
    print("✅ Generated 5 different remote tools")
    print("✅ Each tool is unique and customized")
    print("✅ Security analyzed and optimized")
    print("✅ Ready for deployment")
    print("=" * 60)

def demo_ai_learning():
    """Show how AI learns and improves"""
    print("\n🧠 AI Learning and Adaptation Demo")
    print("-" * 50)
    
    # Simulate AI learning from successful operations
    learning_data = {
        "successful_tools": [
            {"type": "stealth_rdp", "success_rate": 0.95, "detection_rate": 0.02},
            {"type": "polymorphic_keylogger", "success_rate": 0.88, "detection_rate": 0.05},
            {"type": "assembly_loader", "success_rate": 0.92, "detection_rate": 0.01}
        ],
        "failed_tools": [
            {"type": "basic_backdoor", "failure_reason": "detected_by_av", "detection_rate": 0.85}
        ],
        "improvements": [
            "Added more anti-debugging techniques",
            "Improved encryption algorithms",
            "Enhanced stealth capabilities"
        ]
    }
    
    print("AI Learning Data:")
    print(json.dumps(learning_data, indent=2))
    
    print("\n🔄 AI Adaptation:")
    print("• Learning from successful stealth techniques")
    print("• Avoiding patterns that trigger detection")
    print("• Improving encryption methods")
    print("• Optimizing performance")

if __name__ == "__main__":
    demo_remote_tool_generation()
    demo_ai_learning()
