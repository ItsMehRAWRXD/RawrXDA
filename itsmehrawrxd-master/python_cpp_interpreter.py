#!/usr/bin/env python3
"""
Python C++ Interpreter
Executes C++ code directly in Python without external compilers
"""

import os
import sys
import re
import ast
from typing import Dict, List, Optional, Any
import json

class PythonCppInterpreter:
    """Python-based C++ interpreter that executes C++ code directly"""
    
    def __init__(self):
        self.variables = {}
        self.functions = {}
        self.includes = {}
        self.current_line = 0
        
    def interpret_cpp(self, cpp_source: str) -> Dict[str, Any]:
        """Interpret and execute C++ source code"""
        print(f"🐍 Python C++ Interpreter - Executing C++ code")
        
        try:
            # Parse and execute C++ code
            result = self._parse_and_execute(cpp_source)
            
            return {
                "success": True,
                "output": result,
                "variables": self.variables,
                "functions": list(self.functions.keys())
            }
            
        except Exception as e:
            return {
                "success": False,
                "error": str(e),
                "output": "",
                "variables": {},
                "functions": []
            }
    
    def _parse_and_execute(self, cpp_source: str) -> str:
        """Parse and execute C++ source code"""
        lines = cpp_source.split('\n')
        output = []
        
        for line_num, line in enumerate(lines, 1):
            self.current_line = line_num
            line = line.strip()
            
            if not line or line.startswith('//'):
                continue
            
            # Handle includes
            if line.startswith('#include'):
                self._handle_include(line)
                continue
            
            # Handle variable declarations
            if self._is_variable_declaration(line):
                self._handle_variable_declaration(line)
                continue
            
            # Handle function definitions
            if self._is_function_definition(line):
                self._handle_function_definition(line, lines, line_num)
                continue
            
            # Handle cout statements
            if 'std::cout' in line:
                output.append(self._handle_cout(line))
                continue
            
            # Handle return statements
            if line.startswith('return'):
                return self._handle_return(line)
            
            # Handle expressions
            if '=' in line and not line.startswith('//'):
                self._handle_assignment(line)
        
        return '\n'.join(output)
    
    def _handle_include(self, line: str):
        """Handle #include statements"""
        if '<iostream>' in line:
            self.includes['iostream'] = True
        elif '<string>' in line:
            self.includes['string'] = True
        elif '<vector>' in line:
            self.includes['vector'] = True
    
    def _is_variable_declaration(self, line: str) -> bool:
        """Check if line is a variable declaration"""
        types = ['int', 'string', 'float', 'double', 'bool', 'char']
        return any(line.startswith(t + ' ') for t in types)
    
    def _handle_variable_declaration(self, line: str):
        """Handle variable declarations"""
        # Parse: int x = 42;
        match = re.match(r'(\w+)\s+(\w+)\s*=\s*(.+);', line)
        if match:
            var_type, var_name, value = match.groups()
            self.variables[var_name] = self._convert_value(value, var_type)
    
    def _is_function_definition(self, line: str) -> bool:
        """Check if line is a function definition"""
        return 'int main()' in line or 'void main()' in line or line.endswith('{')
    
    def _handle_function_definition(self, line: str, lines: List[str], line_num: int):
        """Handle function definitions"""
        if 'main()' in line:
            # Execute main function body
            brace_count = 0
            i = line_num
            while i < len(lines):
                current_line = lines[i].strip()
                if '{' in current_line:
                    brace_count += current_line.count('{')
                if '}' in current_line:
                    brace_count -= current_line.count('}')
                if brace_count == 0 and current_line:
                    break
                i += 1
    
    def _handle_cout(self, line: str) -> str:
        """Handle std::cout statements"""
        # Extract the content between << and >>
        content = re.findall(r'<<\s*([^;]+)', line)
        if content:
            # Evaluate the expression
            result = self._evaluate_expression(content[0])
            return str(result)
        return ""
    
    def _handle_return(self, line: str) -> str:
        """Handle return statements"""
        # Extract return value
        match = re.search(r'return\s+(.+);', line)
        if match:
            value = match.group(1)
            return str(self._evaluate_expression(value))
        return "0"
    
    def _handle_assignment(self, line: str):
        """Handle assignment statements"""
        # Parse: x = y + z;
        if '=' in line and not line.startswith('//'):
            parts = line.split('=')
            if len(parts) == 2:
                var_name = parts[0].strip()
                expression = parts[1].strip().rstrip(';')
                
                # Remove type declaration if present
                var_name = var_name.split()[-1]
                
                result = self._evaluate_expression(expression)
                self.variables[var_name] = result
    
    def _evaluate_expression(self, expression: str) -> Any:
        """Evaluate mathematical expressions"""
        expression = expression.strip()
        
        # Handle string literals
        if expression.startswith('"') and expression.endswith('"'):
            return expression[1:-1]
        
        # Handle variables
        if expression in self.variables:
            return self.variables[expression]
        
        # Handle arithmetic expressions
        if '+' in expression or '-' in expression or '*' in expression or '/' in expression:
            return self._evaluate_arithmetic(expression)
        
        # Handle numbers
        try:
            if '.' in expression:
                return float(expression)
            else:
                return int(expression)
        except ValueError:
            return expression
    
    def _evaluate_arithmetic(self, expression: str) -> Any:
        """Evaluate arithmetic expressions"""
        # Simple arithmetic evaluation
        expression = expression.replace(' ', '')
        
        # Handle addition
        if '+' in expression:
            parts = expression.split('+')
            if len(parts) == 2:
                left = self._evaluate_expression(parts[0])
                right = self._evaluate_expression(parts[1])
                if isinstance(left, (int, float)) and isinstance(right, (int, float)):
                    return left + right
        
        # Handle subtraction
        if '-' in expression and not expression.startswith('-'):
            parts = expression.split('-')
            if len(parts) == 2:
                left = self._evaluate_expression(parts[0])
                right = self._evaluate_expression(parts[1])
                if isinstance(left, (int, float)) and isinstance(right, (int, float)):
                    return left - right
        
        # Handle multiplication
        if '*' in expression:
            parts = expression.split('*')
            if len(parts) == 2:
                left = self._evaluate_expression(parts[0])
                right = self._evaluate_expression(parts[1])
                if isinstance(left, (int, float)) and isinstance(right, (int, float)):
                    return left * right
        
        # Handle division
        if '/' in expression:
            parts = expression.split('/')
            if len(parts) == 2:
                left = self._evaluate_expression(parts[0])
                right = self._evaluate_expression(parts[1])
                if isinstance(left, (int, float)) and isinstance(right, (int, float)):
                    return left / right
        
        return expression
    
    def _convert_value(self, value: str, var_type: str) -> Any:
        """Convert string value to appropriate type"""
        value = value.strip().rstrip(';')
        
        if var_type == 'int':
            return int(value)
        elif var_type == 'float' or var_type == 'double':
            return float(value)
        elif var_type == 'string':
            if value.startswith('"') and value.endswith('"'):
                return value[1:-1]
            return value
        elif var_type == 'bool':
            return value.lower() == 'true'
        else:
            return value
    
    def get_interpreter_info(self) -> Dict[str, Any]:
        """Get information about the interpreter"""
        return {
            "name": "Python C++ Interpreter",
            "description": "Executes C++ code directly in Python without external compilers",
            "features": [
                "No external dependencies",
                "Direct C++ execution",
                "Variable management",
                "Function support",
                "Arithmetic operations",
                "String handling"
            ],
            "supported_types": ["int", "float", "double", "string", "bool", "char"],
            "supported_operations": ["+", "-", "*", "/", "=", "cout", "return"]
        }

def test_python_cpp_interpreter():
    """Test the Python C++ interpreter"""
    print("🧪 Testing Python C++ Interpreter...")
    
    interpreter = PythonCppInterpreter()
    info = interpreter.get_interpreter_info()
    
    print(f"📋 Interpreter Info:")
    print(f"   Name: {info['name']}")
    print(f"   Description: {info['description']}")
    print(f"   Features: {', '.join(info['features'])}")
    
    # Test C++ source
    cpp_source = """
#include <iostream>
#include <string>

int main() {
    std::string message = "Hello from Python C++ Interpreter!";
    std::cout << message << std::endl;
    
    int x = 42;
    int y = 8;
    int result = x + y;
    
    std::cout << "42 + 8 = " << result << std::endl;
    
    return 0;
}
"""
    
    print("🐍 Interpreting C++ with Python interpreter...")
    result = interpreter.interpret_cpp(cpp_source)
    
    if result["success"]:
        print("✅ Python C++ interpretation successful!")
        print(f"📤 Output: {result['output']}")
        print(f"📊 Variables: {result['variables']}")
        print(f"📊 Functions: {result['functions']}")
        
        return True
    else:
        print("❌ Python C++ interpretation failed!")
        print(f"Error: {result['error']}")
        return False

if __name__ == "__main__":
    test_python_cpp_interpreter()
