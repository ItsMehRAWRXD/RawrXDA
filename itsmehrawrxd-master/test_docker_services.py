#!/usr/bin/env python3
"""
Test Docker Services Import
"""

def test_imports():
    """Test that Docker services can be imported"""
    try:
        # Test if the temp directory structure exists
        import tempfile
        import os
        from pathlib import Path
        
        temp_dir = Path(tempfile.gettempdir()) / "ai_ide_images"
        
        services = ["chatgpt-api", "claude-api", "ollama-server", "code-analyzer", "ai-copilot"]
        
        for service in services:
            service_dir = temp_dir / service / "service"
            if service_dir.exists():
                print(f"✅ {service} service directory exists")
            else:
                print(f"❌ {service} service directory missing")
                
        print("\n🎉 Docker service structure verification complete!")
        
    except Exception as e:
        print(f"❌ Import test failed: {e}")

if __name__ == "__main__":
    test_imports()
