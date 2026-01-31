#!/usr/bin/env python3
"""
Quick Start AI Models Script
Pulls and tests your first AI model for the n0mn0m IDE
"""

import subprocess
import requests
import time
import sys

def check_ollama():
    """Check if Ollama is running"""
    try:
        response = requests.get("http://localhost:11434/api/tags", timeout=5)
        if response.status_code == 200:
            data = response.json()
            models = data.get('models', [])
            print(f"✅ Ollama is running! Found {len(models)} models.")
            return models
        else:
            print("❌ Ollama not responding")
            return []
    except requests.exceptions.RequestException:
        print("❌ Ollama not running. Please start it with: ollama serve")
        return []

def pull_model(model_name):
    """Pull a model from Ollama"""
    print(f"⬇️ Pulling {model_name}... This may take several minutes.")
    print("💡 You can monitor progress in the terminal.")
    
    try:
        # Start the pull process
        process = subprocess.Popen(
            ['ollama', 'pull', model_name],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            universal_newlines=True
        )
        
        # Stream output
        for line in process.stdout:
            if line.strip():
                print(f"📦 {line.strip()}")
        
        process.wait()
        
        if process.returncode == 0:
            print(f"✅ Successfully pulled {model_name}!")
            return True
        else:
            print(f"❌ Failed to pull {model_name}")
            return False
            
    except FileNotFoundError:
        print("❌ Ollama not found. Please install Ollama first.")
        return False
    except Exception as e:
        print(f"❌ Error: {e}")
        return False

def test_model(model_name):
    """Test a model with a simple prompt"""
    print(f"🧪 Testing {model_name}...")
    
    try:
        response = requests.post(
            "http://localhost:11434/api/generate",
            json={
                "model": model_name,
                "prompt": "Write a simple Python function that calculates the factorial of a number.",
                "stream": False
            },
            timeout=30
        )
        
        if response.status_code == 200:
            result = response.json()
            response_text = result.get('response', 'No response')
            
            print(f"🤖 {model_name} response:")
            print("=" * 50)
            print(response_text)
            print("=" * 50)
            
            # Check if it's code
            if any(keyword in response_text.lower() for keyword in ['def ', 'function', 'class ', 'import ']):
                print("💡 Code detected! Your AI model is ready for coding assistance.")
            else:
                print("💡 Model responded, but no code detected. Try a more specific coding prompt.")
            
            return True
        else:
            print(f"❌ Model test failed: {response.status_code}")
            return False
            
    except Exception as e:
        print(f"❌ Error testing model: {e}")
        return False

def main():
    """Main function"""
    print("🤖 Quick Start AI Models for n0mn0m IDE")
    print("=" * 50)
    
    # Check Ollama
    models = check_ollama()
    
    if not models:
        print("\n💡 To get started:")
        print("1. Start Ollama: ollama serve")
        print("2. Run this script again")
        return
    
    # Show available models
    if models:
        print(f"\n📦 Available models:")
        for model in models:
            print(f"  • {model['name']}")
    
    # Recommend a model to pull
    print(f"\n💡 Recommended models to try:")
    recommendations = [
        ("tinyllama:1.1b", "637MB - Very fast, good for testing"),
        ("codellama:7b", "3.8GB - Excellent for code generation"),
        ("llama2:7b", "3.8GB - General purpose AI"),
        ("gemma:2b", "1.6GB - Lightweight and efficient")
    ]
    
    for model_name, description in recommendations:
        print(f"  • {model_name} - {description}")
    
    # Interactive model selection
    print(f"\n🎯 Choose a model to pull:")
    print("1. tinyllama:1.1b (Recommended for testing)")
    print("2. codellama:7b (Best for code generation)")
    print("3. llama2:7b (General purpose)")
    print("4. gemma:2b (Lightweight)")
    print("5. Enter custom model name")
    print("6. Skip (use existing models)")
    
    choice = input("\nEnter your choice (1-6): ").strip()
    
    model_map = {
        "1": "tinyllama:1.1b",
        "2": "codellama:7b", 
        "3": "llama2:7b",
        "4": "gemma:2b"
    }
    
    if choice in model_map:
        selected_model = model_map[choice]
    elif choice == "5":
        selected_model = input("Enter model name: ").strip()
    elif choice == "6":
        selected_model = None
    else:
        print("❌ Invalid choice. Using tinyllama:1.1b as default.")
        selected_model = "tinyllama:1.1b"
    
    # Pull and test model
    if selected_model:
        success = pull_model(selected_model)
        if success:
            test_model(selected_model)
    
    # Show next steps
    print(f"\n🎉 Setup complete! Next steps:")
    print("1. Run: python test_local_ai_compiler.py")
    print("2. Or run: python complete_n0mn0m_universal_ide.py")
    print("3. Use the AI Models & Compilers tab in the IDE")
    print("4. Try the AI features: Code Completion, Context Chat, Code Analysis")
    
    print(f"\n💡 Pro tips:")
    print("• Start with tinyllama:1.1b for testing")
    print("• Use codellama:7b for code generation")
    print("• Try context-aware chat for project-specific help")
    print("• Use online compilers to execute generated code")

if __name__ == "__main__":
    main()
