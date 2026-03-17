#!/usr/bin/env python3
"""
NASM IDE Swarm Agent Runner
Quick start script for the Professional NASM IDE Swarm system
"""

import os
import sys
import subprocess
import time
import json
from pathlib import Path

def check_dependencies():
    """Check if required dependencies are installed"""
    required_packages = [
        'asyncio',
        'websockets',
        'aiohttp',
        'threading',
        'queue'
    ]
    
    missing = []
    for package in required_packages:
        try:
            __import__(package)
        except ImportError:
            missing.append(package)
    
    if missing:
        print(f"Missing required packages: {', '.join(missing)}")
        print("Install with: pip install " + " ".join(missing))
        return False
    
    return True

def create_directory_structure():
    """Create necessary directories"""
    directories = [
        'models',
        'logs',
        'frontend/static',
        'frontend/css',
        'frontend/js',
        'tools',
        'examples',
        'docs'
    ]
    
    for directory in directories:
        Path(directory).mkdir(parents=True, exist_ok=True)
        print(f"Created directory: {directory}")

def download_example_models():
    """Download or create example models (placeholders)"""
    models_dir = Path('models')
    
    example_models = [
        'code_completion_400mb.gguf',
        'text_generation_300mb.gguf',
        'syntax_analysis_250mb.gguf',
        'error_detection_200mb.gguf',
        'documentation_350mb.gguf',
        'collaboration_280mb.gguf',
        'marketplace_ai_320mb.gguf',
        'optimization_290mb.gguf',
        'debugging_310mb.gguf',
        'integration_270mb.gguf'
    ]
    
    for model in example_models:
        model_path = models_dir / model
        if not model_path.exists():
            # Create placeholder files
            with open(model_path, 'w') as f:
                f.write(f"# Placeholder for {model}\n")
                f.write("# Replace with actual AI model file\n")
            print(f"Created placeholder: {model}")

def start_swarm_controller():
    """Start the swarm controller"""
    print("Starting Professional NASM IDE Swarm Controller...")
    
    # Check if config exists
    if not Path('swarm_config.json').exists():
        print("Error: swarm_config.json not found!")
        return False
    
    # Start the swarm controller
    try:
        subprocess.run([sys.executable, 'swarm_controller.py'], check=True)
    except subprocess.CalledProcessError as e:
        print(f"Error starting swarm controller: {e}")
        return False
    except KeyboardInterrupt:
        print("\nShutting down swarm controller...")
        return True
    
    return True

def open_ide():
    """Open the IDE in default browser"""
    import webbrowser
    
    time.sleep(2)  # Wait for server to start
    
    ide_url = "http://localhost:8080"
    print(f"Opening Professional NASM IDE at {ide_url}")
    
    try:
        webbrowser.open(ide_url)
    except Exception as e:
        print(f"Could not open browser automatically: {e}")
        print(f"Please open {ide_url} manually in your browser")

def show_banner():
    """Show startup banner"""
    banner = """
╔══════════════════════════════════════════════════════════════╗
║                Professional NASM IDE                        ║
║                   Swarm Edition v1.0                        ║
║                                                              ║
║  🤖 AI-Powered Assembly Development Environment              ║
║  👥 Team Collaboration (up to 25 users)                     ║
║  🏪 Extension Marketplace                                    ║
║  ⚡ Real-time Code Completion & Analysis                     ║
║                                                              ║
║  10 Specialized AI Agents:                                   ║
║  • Code Completion    • Text Generation                      ║
║  • Syntax Analysis    • Error Detection                      ║
║  • Documentation      • Collaboration                       ║
║  • Marketplace AI     • Optimization                        ║
║  • Debugging          • Integration                          ║
╚══════════════════════════════════════════════════════════════╝
"""
    print(banner)

def main():
    """Main entry point"""
    show_banner()
    
    print("Initializing Professional NASM IDE Swarm...")
    
    # Check dependencies
    if not check_dependencies():
        print("Please install missing dependencies and try again.")
        return 1
    
    # Create directory structure
    print("\nCreating directory structure...")
    create_directory_structure()
    
    # Download example models
    print("\nSetting up example models...")
    download_example_models()
    
    # Check if we're in the right directory
    required_files = ['swarm_controller.py', 'swarm_config.json']
    missing_files = [f for f in required_files if not Path(f).exists()]
    
    if missing_files:
        print(f"\nError: Missing required files: {', '.join(missing_files)}")
        print("Make sure you're running this script from the Professional NASM IDE directory.")
        return 1
    
    print("\n" + "="*60)
    print("🚀 Starting Professional NASM IDE Swarm System...")
    print("="*60)
    print()
    print("Features available:")
    print("  ✅ Text Editor with AI Code Completion")
    print("  ✅ Team Collaboration (WebSocket-based)")
    print("  ✅ Extension Marketplace")
    print("  ✅ 10 Specialized AI Agents")
    print("  ✅ Real-time Syntax Analysis")
    print("  ✅ Build System Integration")
    print()
    print("Servers starting:")
    print("  🌐 WebSocket Server: ws://localhost:8765")
    print("  🌐 HTTP Server: http://localhost:8080")
    print()
    print("Press Ctrl+C to stop the server")
    print("="*60)
    
    # Start IDE in browser (in background)
    import threading
    browser_thread = threading.Thread(target=open_ide)
    browser_thread.daemon = True
    browser_thread.start()
    
    # Start swarm controller
    try:
        start_swarm_controller()
    except KeyboardInterrupt:
        print("\n" + "="*60)
        print("Professional NASM IDE Swarm stopped.")
        print("Thank you for using Professional NASM IDE!")
        print("="*60)
        return 0
    
    return 0

if __name__ == "__main__":
    sys.exit(main())