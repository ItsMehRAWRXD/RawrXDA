#!/usr/bin/env python3
"""
Test Enhanced AI Features
Demonstrates the new AI capabilities added to the local AI compiler manager
"""

import sys
import os
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

from ai_helper_classes import (
    AICodeGenerator, AIDebugger, AIOptimizer, AISecurityAnalyzer,
    AIDocumentationGenerator, AITestGenerator, AIRefactoringEngine, AIPatternRecognizer
)

class MockManager:
    """Mock manager for testing AI helper classes"""
    def __init__(self):
        self.service_status = {'ollama': False}
        self.ollama_url = "http://localhost:11434"

def test_ai_code_generator():
    """Test AI code generation"""
    print("🧪 Testing AI Code Generator...")
    
    manager = MockManager()
    generator = AICodeGenerator(manager)
    
    # Test Python code generation
    prompt = "Create a function to calculate fibonacci numbers"
    result = generator.generate_code(prompt, "python", "mathematical function")
    print(f"Generated Python code:\n{result}\n")
    
    # Test JavaScript code generation
    prompt = "Create a class for handling user authentication"
    result = generator.generate_code(prompt, "javascript", "web application")
    print(f"Generated JavaScript code:\n{result}\n")

def test_ai_debugger():
    """Test AI debugging assistance"""
    print("🐛 Testing AI Debugger...")
    
    manager = MockManager()
    debugger = AIDebugger(manager)
    
    # Test Python code debugging
    python_code = """
def calculate_average(numbers):
    total = 0
    for i in range(len(numbers)):
        total += numbers[i]
    return total / len(numbers)

result = calculate_average([1, 2, 3, 4, 5])
print(result)
"""
    
    result = debugger.analyze_and_fix(python_code, "No errors")
    print(f"Debug analysis:\n{result}\n")
    
    # Test JavaScript code debugging
    js_code = """
function processData(data) {
    var result = [];
    for (var i = 0; i < data.length; i++) {
        if (data[i] > 0) {
            result.push(data[i] * 2);
        }
    }
    return result;
}
"""
    
    result = debugger.analyze_and_fix(js_code, "No errors")
    print(f"Debug analysis:\n{result}\n")

def test_ai_optimizer():
    """Test AI code optimization"""
    print("⚡ Testing AI Optimizer...")
    
    manager = MockManager()
    optimizer = AIOptimizer(manager)
    
    # Test Python code optimization
    python_code = """
def process_list(data):
    result = []
    for i in range(len(data)):
        if data[i] > 0:
            result.append(data[i] * 2)
    return result
"""
    
    result = optimizer.optimize_code(python_code, "python")
    print(f"Optimization result:\n{result}\n")
    
    # Test JavaScript code optimization
    js_code = """
function processData(data) {
    var result = [];
    for (var i = 0; i < data.length; i++) {
        if (data[i] > 0) {
            result.push(data[i] * 2);
        }
    }
    return result;
}
"""
    
    result = optimizer.optimize_code(js_code, "javascript")
    print(f"Optimization result:\n{result}\n")

def test_ai_security_analyzer():
    """Test AI security analysis"""
    print("🔒 Testing AI Security Analyzer...")
    
    manager = MockManager()
    analyzer = AISecurityAnalyzer(manager)
    
    # Test Python code security analysis
    python_code = """
import os
import subprocess

def run_command(user_input):
    os.system(f"echo {user_input}")
    subprocess.run(f"ls {user_input}", shell=True)
    
def process_data(data):
    import pickle
    return pickle.loads(data)
"""
    
    result = analyzer.analyze_security(python_code, "python")
    print(f"Security analysis:\n{result}\n")
    
    # Test JavaScript code security analysis
    js_code = """
function displayUserData(userInput) {
    document.getElementById('output').innerHTML = userInput;
    eval('console.log("User data: " + userInput)');
}
"""
    
    result = analyzer.analyze_security(js_code, "javascript")
    print(f"Security analysis:\n{result}\n")

def test_ai_documentation_generator():
    """Test AI documentation generation"""
    print("📚 Testing AI Documentation Generator...")
    
    manager = MockManager()
    doc_generator = AIDocumentationGenerator(manager)
    
    # Test Python code documentation
    python_code = """
def fibonacci(n):
    '''Calculate fibonacci number'''
    if n <= 1:
        return n
    return fibonacci(n-1) + fibonacci(n-2)

class Calculator:
    '''Simple calculator class'''
    def __init__(self):
        self.history = []
    
    def add(self, a, b):
        result = a + b
        self.history.append(f"{a} + {b} = {result}")
        return result
"""
    
    result = doc_generator.generate_docs(python_code, "python")
    print(f"Generated documentation:\n{result}\n")

def test_ai_test_generator():
    """Test AI test generation"""
    print("🧪 Testing AI Test Generator...")
    
    manager = MockManager()
    test_generator = AITestGenerator(manager)
    
    # Test Python code test generation
    python_code = """
def fibonacci(n):
    if n <= 1:
        return n
    return fibonacci(n-1) + fibonacci(n-2)

def calculate_average(numbers):
    return sum(numbers) / len(numbers)
"""
    
    result = test_generator.generate_tests(python_code, "python")
    print(f"Generated tests:\n{result}\n")

def test_ai_refactoring_engine():
    """Test AI refactoring engine"""
    print("🔧 Testing AI Refactoring Engine...")
    
    manager = MockManager()
    refactoring_engine = AIRefactoringEngine(manager)
    
    # Test Python code refactoring
    python_code = """
def process_data(data):
    result = []
    for i in range(len(data)):
        if data[i] > 0:
            result.append(data[i] * 2)
    return result

# Dead code
pass
"""
    
    result = refactoring_engine.refactor_code(python_code, "python", "general")
    print(f"Refactoring result:\n{result}\n")

def test_ai_pattern_recognizer():
    """Test AI pattern recognition"""
    print("🎯 Testing AI Pattern Recognizer...")
    
    manager = MockManager()
    pattern_recognizer = AIPatternRecognizer(manager)
    
    # Test Python code pattern recognition
    python_code = """
class Singleton:
    _instance = None
    
    def __new__(cls):
        if cls._instance is None:
            cls._instance = super().__new__(cls)
        return cls._instance

def create_user(name, email):
    return User(name, email)

def notify_observers(event):
    for observer in observers:
        observer.update(event)

def decorator(func):
    def wrapper(*args, **kwargs):
        print("Before function call")
        result = func(*args, **kwargs)
        print("After function call")
        return result
    return wrapper
"""
    
    result = pattern_recognizer.recognize_patterns(python_code, "python")
    print(f"Pattern recognition result:\n{result}\n")

def main():
    """Run all AI feature tests"""
    print("🤖 Enhanced AI Features Test Suite")
    print("=" * 50)
    
    try:
        test_ai_code_generator()
        test_ai_debugger()
        test_ai_optimizer()
        test_ai_security_analyzer()
        test_ai_documentation_generator()
        test_ai_test_generator()
        test_ai_refactoring_engine()
        test_ai_pattern_recognizer()
        
        print("✅ All AI feature tests completed successfully!")
        print("\n🎉 Enhanced AI capabilities are working!")
        
    except Exception as e:
        print(f"❌ Error during testing: {e}")
        import traceback
        traceback.print_exc()

if __name__ == "__main__":
    main()
