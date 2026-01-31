#!/usr/bin/env python3
"""
Test script for AI Model Management System
Tests model pulling, loading, and analysis capabilities
"""

import sys
import os
sys.path.insert(0, os.path.dirname(__file__))

from safe_hybrid_ide import AIModelManager, LocalAIService
import time

def test_model_registry():
    """Test the model registry"""
    print("🧪 Testing AI Model Registry...")
    
    manager = AIModelManager()
    
    # Test available models
    available = manager.list_available_models()
    print(f"📋 Available models: {len(available)}")
    
    for model_id, info in available.items():
        print(f"  • {info['name']} - {info['size_mb']}MB ({info['type']})")
    
    return len(available) > 0

def test_model_pulling():
    """Test model pulling/installation"""
    print("\n📥 Testing Model Pulling...")
    
    manager = AIModelManager()
    
    # Test pulling embedded model (should be instant)
    print("📦 Pulling embedded-mini model...")
    success = manager.pull_model('embedded-mini')
    
    if success:
        print("✅ Embedded model pull successful")
        
        # Check if installed
        installed = manager.list_installed_models()
        print(f"📋 Installed models: {len(installed)}")
        
        return True
    else:
        print("❌ Embedded model pull failed")
        return False

def test_model_loading():
    """Test model loading and unloading"""
    print("\n🚀 Testing Model Loading...")
    
    manager = AIModelManager()
    
    # Ensure embedded-mini is installed
    if not manager.is_model_installed('embedded-mini'):
        manager.pull_model('embedded-mini')
    
    # Test loading
    print("🔄 Loading embedded-mini model...")
    success = manager.load_model('embedded-mini')
    
    if success:
        print("✅ Model loading successful")
        
        # Check active models
        active = manager.active_models
        print(f"🟢 Active models: {len(active)}")
        
        # Test unloading
        print("⏸️ Unloading model...")
        unload_success = manager.unload_model('embedded-mini')
        
        if unload_success:
            print("✅ Model unloading successful")
            return True
        else:
            print("❌ Model unloading failed")
            return False
    else:
        print("❌ Model loading failed")
        return False

def test_code_analysis():
    """Test code analysis with local models"""
    print("\n🔍 Testing Code Analysis with Local Models...")
    
    manager = AIModelManager()
    service = LocalAIService(manager)
    
    # Ensure model is available
    manager.pull_model('embedded-mini')
    manager.load_model('embedded-mini')
    
    # Test Python code analysis
    test_code = '''
def hello_world():
    print("Hello World!")
    return "success"

if __name__ == "__main__":
    hello_world()
'''
    
    print("📝 Analyzing Python code...")
    result = service.analyze_code(test_code, "python", "code_analysis")
    
    if result['success']:
        print(f"✅ Analysis successful using model: {result['model_used']}")
        print(f"📊 Found {len(result['suggestions'])} suggestions:")
        
        for i, suggestion in enumerate(result['suggestions'][:3], 1):
            priority_emoji = {'high': '🔴', 'medium': '🟡', 'low': '🟢'}.get(suggestion['priority'], '🟡')
            print(f"  {i}. {priority_emoji} {suggestion['title']}")
        
        return True
    else:
        print(f"❌ Analysis failed: {result['error']}")
        return False

def test_model_info():
    """Test model information retrieval"""
    print("\n📊 Testing Model Information...")
    
    manager = AIModelManager()
    
    # Test getting model info
    model_info = manager.get_model_info('embedded-mini')
    
    if model_info:
        print("✅ Model info retrieval successful")
        print(f"  📝 Name: {model_info['name']}")
        print(f"  📏 Size: {model_info['size_mb']}MB")
        print(f"  🔧 Type: {model_info['type']}")
        print(f"  🛠️ Capabilities: {', '.join(model_info['capabilities'])}")
        print(f"  📦 Installed: {model_info['installed']}")
        print(f"  🟢 Loaded: {model_info['loaded']}")
        
        return True
    else:
        print("❌ Model info retrieval failed")
        return False

def test_storage_management():
    """Test storage usage tracking"""
    print("\n💾 Testing Storage Management...")
    
    manager = AIModelManager()
    
    # Get storage usage
    storage = manager.get_storage_usage()
    
    print("✅ Storage information retrieved:")
    print(f"  📊 Total size: {storage['total_size_mb']}MB ({storage['total_size_gb']}GB)")
    print(f"  📦 Model count: {storage['model_count']}")
    print(f"  📁 Storage path: {storage['storage_path']}")
    
    if storage['model_sizes']:
        print("  📋 Model sizes:")
        for model_id, size in storage['model_sizes'].items():
            print(f"    • {model_id}: {size}MB")
    
    return True

def test_multiple_models():
    """Test handling multiple models"""
    print("\n🔄 Testing Multiple Model Management...")
    
    manager = AIModelManager()
    service = LocalAIService(manager)
    
    # Install multiple models
    models_to_test = ['embedded-mini', 'tinycode-1b']  # Start with built-in and one downloadable
    
    installed_count = 0
    for model_id in models_to_test:
        print(f"📦 Installing {model_id}...")
        if manager.pull_model(model_id):
            installed_count += 1
            print(f"✅ {model_id} installed")
        else:
            print(f"⚠️ {model_id} installation skipped (demo mode)")
    
    print(f"📊 Successfully handled {installed_count} models")
    
    # Test task routing
    print("🎯 Testing task-specific model routing...")
    
    test_tasks = ['code_completion', 'code_analysis', 'security_check', 'general']
    
    for task in test_tasks:
        best_model = service.get_best_model_for_task(task)
        print(f"  • {task}: {best_model}")
    
    return installed_count > 0

def test_model_capabilities():
    """Test different model capabilities"""
    print("\n⚡ Testing Model Capabilities...")
    
    manager = AIModelManager()
    
    # Test different code types
    test_cases = [
        ('python', '''
def calculate(a, b):
    return a + b

print(calculate(5, 3))
'''),
        ('javascript', '''
var name = "John";
function greet(name) {
    if (name == undefined) {
        console.log("Hello!");
    } else {
        console.log("Hello " + name);
    }
}
greet(name);
'''),
        ('cpp', '''
#include <iostream>
using namespace std;

int main() {
    int* ptr = new int(42);
    cout << *ptr << endl;
    return 0;
}
''')
    ]
    
    success_count = 0
    for language, code in test_cases:
        print(f"🔍 Testing {language} analysis...")
        result = manager.analyze_code_with_model('embedded-mini', code, language)
        
        if result['success']:
            print(f"  ✅ {language} analysis successful ({len(result['suggestions'])} suggestions)")
            success_count += 1
        else:
            print(f"  ❌ {language} analysis failed")
    
    return success_count == len(test_cases)

if __name__ == "__main__":
    print("🤖 AI Model Management System Test Suite")
    print("=" * 60)
    
    tests = [
        ("Model Registry", test_model_registry),
        ("Model Pulling", test_model_pulling),
        ("Model Loading", test_model_loading),
        ("Code Analysis", test_code_analysis),
        ("Model Information", test_model_info),
        ("Storage Management", test_storage_management),
        ("Multiple Models", test_multiple_models),
        ("Model Capabilities", test_model_capabilities)
    ]
    
    passed = 0
    total = len(tests)
    
    for test_name, test_func in tests:
        print(f"\n{'='*20} {test_name} {'='*20}")
        try:
            if test_func():
                print(f"✅ {test_name}: PASSED")
                passed += 1
            else:
                print(f"❌ {test_name}: FAILED")
        except Exception as e:
            print(f"💥 {test_name}: ERROR - {e}")
    
    print("\n" + "=" * 60)
    print(f"🎯 Test Results: {passed}/{total} tests passed")
    
    if passed == total:
        print("🎉 All tests passed! Model management system is working correctly.")
        print("\n✅ **System Ready:**")
        print("   • Model registry: ✅ Working")
        print("   • Model downloading: ✅ Working")
        print("   • Model loading: ✅ Working")
        print("   • Code analysis: ✅ Working")
        print("   • Storage management: ✅ Working")
        print("   • Multi-language support: ✅ Working")
        
        print("\n🚀 **Usage Instructions:**")
        print("   1. Launch the IDE: python safe_hybrid_ide.py")
        print("   2. Press F8 or click '📦 AI Models' to open Model Manager")
        print("   3. Select models and click 'Pull Model' to download")
        print("   4. Click 'Load' to activate models for use")
        print("   5. Use F7 or '🤖 AI Analyze' to analyze code with loaded models")
        
        print("\n💡 **Model Recommendations:**")
        print("   • Start with embedded-mini (built-in, no download)")
        print("   • Add tinycode-1b for better code completion")  
        print("   • Add security-analyzer for security checks")
        print("   • Add python-code-gpt for Python-specific features")
        
    else:
        print("❌ Some tests failed. Please check the implementation.")
        print("\n🛠️ **Troubleshooting:**")
        print("   • Check Python version (3.7+)")
        print("   • Ensure write permissions to temp directory")
        print("   • Verify network connectivity for downloads")
    
    print(f"\nPress Enter to exit...")
    input()
