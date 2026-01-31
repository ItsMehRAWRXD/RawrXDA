#!/usr/bin/env python3
"""
Quick AI Test - Test if AI actually works
"""

import requests
import json

def test_ollama_models():
    """Test what models are available"""
    try:
        r = requests.get('http://localhost:11434/api/tags', timeout=5)
        if r.status_code == 200:
            models = r.json().get('models', [])
            print("✅ Ollama is running!")
            print(f"📊 Available models: {len(models)}")
            for model in models:
                print(f"  - {model['name']}")
            return models
        else:
            print(f"❌ Ollama error: {r.status_code}")
            return []
    except Exception as e:
        print(f"❌ Ollama connection failed: {e}")
        return []

def test_ai_generation():
    """Test if AI can actually generate code"""
    try:
        # First get available models
        models_r = requests.get('http://localhost:11434/api/tags', timeout=5)
        if models_r.status_code == 200:
            models = models_r.json().get('models', [])
            if models:
                model_name = models[0]['name']  # Use the first available model
                print(f"Testing with model: {model_name}")
                
                # Try with the available model
                r = requests.post('http://localhost:11434/api/generate', 
                                 json={
                                     'model': model_name,
                                     'prompt': 'Write a simple Python function to add two numbers',
                                     'stream': False
                                 }, 
                                 timeout=30)
                
                print(f"Status code: {r.status_code}")
                response_data = r.json()
                print(f"Full response: {response_data}")
                
                if r.status_code == 200:
                    response = response_data.get('response', '')
                    if response:
                        print("✅ AI code generation works!")
                        print(f"Generated code:\n{response[:300]}...")
                        return True
                    else:
                        print("❌ AI returned empty response")
                        return False
                else:
                    print(f"❌ AI generation failed: {r.status_code}")
                    print(f"Error: {response_data.get('error', 'Unknown error')}")
                    return False
            else:
                print("❌ No models available")
                return False
        else:
            print(f"❌ Failed to get models: {models_r.status_code}")
            return False
    except Exception as e:
        print(f"❌ AI generation error: {e}")
        return False

def test_compilation():
    """Test if we can actually compile code"""
    try:
        # Test Python compilation
        test_code = """
def add_numbers(a, b):
    return a + b

print(add_numbers(5, 3))
"""
        
        # Try to compile it
        compile(test_code, '<string>', 'exec')
        print("✅ Python code compilation works!")
        
        # Try to execute it
        exec(test_code)
        print("✅ Python code execution works!")
        return True
    except Exception as e:
        print(f"❌ Compilation failed: {e}")
        return False

if __name__ == "__main__":
    print("🧪 Testing AI and Compilation Systems")
    print("=" * 50)
    
    # Test Ollama
    models = test_ollama_models()
    
    if models:
        # Test AI generation
        test_ai_generation()
    
    # Test compilation
    test_compilation()
    
    print("\n🎯 Test Results:")
    print("✅ Ollama: Working" if models else "❌ Ollama: Not working")
    print("✅ AI Generation: Working" if test_ai_generation() else "❌ AI Generation: Not working")
    print("✅ Compilation: Working" if test_compilation() else "❌ Compilation: Not working")
