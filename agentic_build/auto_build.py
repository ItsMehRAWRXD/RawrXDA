#!/usr/bin/env python3
"""
Autonomous Build Wrapper for RawrXD
Automatically detects and runs the best available build system.
"""

import os
import sys
import subprocess
import json
from pathlib import Path

def detect_build_tools():
    """Detect available build tools."""
    tools = {}
    
    # Check for Ninja
    try:
        subprocess.run(["ninja", "--version"], capture_output=True, check=True)
        tools["ninja"] = True
    except (subprocess.CalledProcessError, FileNotFoundError):
        tools["ninja"] = False
    
    # Check for Make (nmake on Windows)
    try:
        subprocess.run(["nmake", "/?"], capture_output=True, check=True)
        tools["nmake"] = True
    except (subprocess.CalledProcessError, FileNotFoundError):
        tools["nmake"] = False
    
    # Check for PowerShell
    try:
        subprocess.run(["pwsh", "--version"], capture_output=True, check=True)
        tools["pwsh"] = True
    except (subprocess.CalledProcessError, FileNotFoundError):
        tools["pwsh"] = False
    
    return tools

def run_build(build_tool: str, target: str = "all"):
    """Run the build with the specified tool."""
    print(f"🔨 Building with {build_tool}...")
    
    if build_tool == "ninja":
        cmd = ["ninja"]
        if target != "all":
            cmd.append(target)
    elif build_tool == "nmake":
        cmd = ["nmake"]
        if target != "all":
            cmd.append(target)
    elif build_tool == "pwsh":
        cmd = ["pwsh", "-File", "build.ps1"]
        if target != "all":
            cmd.extend(["-Target", target])
    else:
        raise ValueError(f"Unknown build tool: {build_tool}")
    
    result = subprocess.run(cmd, capture_output=False)
    return result.returncode == 0

def main():
    """Main autonomous build function."""
    print("🤖 RawrXD Autonomous Build System")
    print("=" * 40)
    
    # Load configuration
    config_file = Path("build_config.json")
    if config_file.exists():
        with open(config_file) as f:
            config = json.load(f)
        print(f"📋 Project: {config['project']['name']} v{config['project']['version']}")
    
    # Detect available tools
    tools = detect_build_tools()
    print(f"🔍 Available build tools: {[k for k, v in tools.items() if v]}")
    
    # Choose best available tool
    if tools["ninja"]:
        build_tool = "ninja"
    elif tools["nmake"]:
        build_tool = "nmake"
    elif tools["pwsh"]:
        build_tool = "pwsh"
    else:
        print("❌ No supported build tools found!")
        print("Install one of: ninja, nmake, or PowerShell")
        return 1
    
    # Run the build
    target = sys.argv[1] if len(sys.argv) > 1 else "all"
    success = run_build(build_tool, target)
    
    if success:
        print("✅ Build completed successfully!")
        return 0
    else:
        print("❌ Build failed!")
        return 1

if __name__ == "__main__":
    sys.exit(main())
