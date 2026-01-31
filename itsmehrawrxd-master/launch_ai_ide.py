#!/usr/bin/env python3
"""
Smart launcher for AI-Enhanced Safe Hybrid IDE
Handles missing dependencies gracefully and starts with available features
"""

import sys
import os
import importlib

def check_dependencies():
    """Check which dependencies are available"""
    dependencies = {
        'aiohttp': False,
        'requests': False,
        'typing_extensions': False
    }
    
    # Check each dependency
    for dep in dependencies:
        try:
            importlib.import_module(dep)
            dependencies[dep] = True
        except ImportError:
            dependencies[dep] = False
    
    return dependencies

def print_status(deps):
    """Print dependency status"""
    print("🔍 Checking AI IDE Dependencies...")
    print("=" * 50)
    
    # Core features (always available)
    print("✅ Embedded AI Engine     - Built-in (Python standard library)")
    print("✅ AI Model Manager       - Built-in (download/manage local models)")
    print("✅ Internal Docker Engine - Built-in (no external Docker required)")
    print("✅ Container Management   - Built-in (run/stop AI services)")
    print("✅ Code Compiler          - Built-in (Python standard library)")  
    print("✅ GUI Interface          - Built-in (tkinter)")
    print("✅ File Management        - Built-in (Python standard library)")
    
    print("\n🌐 Cloud AI Services:")
    if deps['aiohttp'] and deps['requests']:
        print("✅ OpenAI GPT Integration - Available")
        print("✅ Claude Integration     - Available")
        print("✅ External AI Services   - Available")
    elif deps['requests']:
        print("🟡 Basic AI Services      - Limited (install aiohttp for full features)")
        print("❌ Advanced AI Features   - Missing aiohttp dependency")
    else:
        print("❌ Cloud AI Services      - Missing dependencies")
        print("💡 Tip: Run 'pip install aiohttp requests' for cloud AI features")
    
    print(f"\n📊 Status Summary:")
    available_features = 5  # Core features always available (including model manager)
    if deps['aiohttp'] and deps['requests']:
        available_features += 3
        status = "🎉 Full AI IDE with model management and cloud AI!"
    elif deps['requests']:
        available_features += 1  
        status = "🟡 AI IDE with model management + partial cloud features"
    else:
        status = "🔧 AI IDE with local model management (very powerful!)"
    
    print(f"   {status}")
    print(f"   Features available: {available_features}/8")

def start_ide_safe_mode():
    """Start IDE with embedded AI only (no external dependencies)"""
    print("\n🚀 Starting IDE in Safe Mode (Embedded AI only)...")
    
    try:
        # Import with fallback handling
        sys.path.insert(0, os.path.dirname(__file__))
        
        # Modify the safe_hybrid_ide to work without external dependencies
        import safe_hybrid_ide
        
        # Monkey patch to disable cloud AI if dependencies missing
        original_init = safe_hybrid_ide.AIServiceManager.__init__
        
        def safe_init(self):
            # Initialize base components
            self.session_manager = safe_hybrid_ide.SessionManager()
            self.rate_limiter = safe_hybrid_ide.RateLimiter()
            
            # Start embedded AI server
            self.embedded_server = safe_hybrid_ide.EmbeddedAIServer(port=11435)
            self.embedded_server.start_server()
            
            # Only initialize embedded AI connector
            self.connectors = {
                'embedded-ai': safe_hybrid_ide.EmbeddedAIConnector(
                    self.session_manager, 
                    self.rate_limiter, 
                    self.embedded_server
                )
            }
            
            # Service priorities
            self.service_priorities = {'embedded-ai': 100}
            self.suggestion_queue = safe_hybrid_ide.queue.Queue()
            
            print("🤖 AI Service Manager initialized (Embedded AI only)")
            self.check_available_services()
        
        safe_hybrid_ide.AIServiceManager.__init__ = safe_init
        
        # Start the IDE
        ide = safe_hybrid_ide.SafeHybridIDE()
        ide.run()
        
    except Exception as e:
        print(f"❌ Error starting IDE: {e}")
        print("Please check your Python installation and try again.")
        input("Press Enter to exit...")

def start_ide_full_mode():
    """Start IDE with full features"""
    print("\n🚀 Starting IDE with Full AI Features...")
    
    try:
        sys.path.insert(0, os.path.dirname(__file__))
        import safe_hybrid_ide
        
        ide = safe_hybrid_ide.SafeHybridIDE()
        ide.run()
        
    except Exception as e:
        print(f"❌ Error starting IDE: {e}")
        print("Falling back to safe mode...")
        start_ide_safe_mode()

def main():
    """Main launcher function"""
    print("🤖 AI-Enhanced Safe Hybrid IDE Launcher")
    print("=" * 50)
    
    # Check dependencies
    deps = check_dependencies()
    print_status(deps)
    
    print("\n" + "=" * 50)
    
    # Determine launch mode
    if deps['aiohttp'] and deps['requests']:
        print("🎯 Launching in FULL MODE with all AI services!")
        start_ide_full_mode()
    else:
        print("🛡️ Launching in SAFE MODE with local AI models and internal Docker!")
        print("\n💡 Note: Local AI capabilities include:")
        print("   - 📦 AI Model Manager - Download and manage AI models")
        print("   - 🔧 Embedded AI Engine - Built-in code analysis")
        print("   - 🐳 Internal Docker Engine - No external Docker required!")
        print("   - 🤖 Local Model Support - Run AI models locally")
        print("   - 🚀 Container Services - ChatGPT, Claude, Ollama, Analyzer, Copilot")
        print("   - 🔍 Multi-language analysis (Python, JS, C++, Eiffel)")
        print("   - 🛡️ Security vulnerability detection")
        print("   - 💡 Best practice recommendations")
        print("   - 🚀 No internet required for basic features")
        print(f"\n   🌐 To enable cloud AI features:")
        print(f"   pip install aiohttp requests")
        
        input("\nPress Enter to continue with embedded AI...")
        start_ide_safe_mode()

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n👋 Goodbye!")
    except Exception as e:
        print(f"\n❌ Unexpected error: {e}")
        input("Press Enter to exit...")
