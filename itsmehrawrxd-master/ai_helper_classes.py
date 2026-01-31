#!/usr/bin/env python3
"""
AI Helper Classes for Enhanced Local AI Compiler Manager
Provides specialized AI capabilities for code generation, debugging, optimization, etc.
"""

import requests
import json
import re
import ast
import subprocess
import os
from typing import Dict, List, Optional, Any
from pathlib import Path

class AICodeGenerator:
    """AI-powered code generation"""
    
    def __init__(self, manager):
        self.manager = manager
        self.generation_templates = {
            'python': {
                'function': 'def {name}({params}):\n    """{docstring}"""\n    {body}',
                'class': 'class {name}:\n    """{docstring}"""\n    \n    def __init__(self{params}):\n        {body}',
                'test': 'def test_{name}():\n    """Test {name}"""\n    {body}'
            },
            'javascript': {
                'function': 'function {name}({params}) {{\n    // {docstring}\n    {body}\n}}',
                'class': 'class {name} {{\n    constructor({params}) {{\n        {body}\n    }}\n}}',
                'test': 'function test{name}() {{\n    // Test {name}\n    {body}\n}}'
            },
            'cpp': {
                'function': '{return_type} {name}({params}) {{\n    // {docstring}\n    {body}\n}}',
                'class': 'class {name} {{\npublic:\n    {name}({params}) {{\n        {body}\n    }}\n}}',
                'test': 'void test{name}() {{\n    // Test {name}\n    {body}\n}}'
            }
        }
    
    def generate_code(self, prompt, language="python", context=""):
        """Generate code based on prompt"""
        try:
            # Use Ollama if available
            if self.manager.service_status.get('ollama', False):
                return self._generate_with_ollama(prompt, language, context)
            else:
                return self._generate_with_template(prompt, language, context)
        except Exception as e:
            return f"Error generating code: {str(e)}"
    
    def _generate_with_ollama(self, prompt, language, context):
        """Generate code using Ollama"""
        full_prompt = f"Generate {language} code for: {prompt}\nContext: {context}\nProvide only the code, no explanations."
        
        try:
            response = requests.post(
                f"{self.manager.ollama_url}/api/generate",
                json={
                    "model": "codellama:7b",
                    "prompt": full_prompt,
                    "stream": False
                },
                timeout=30
            )
            
            if response.status_code == 200:
                return response.json().get('response', 'No response generated')
        except requests.exceptions.RequestException:
            pass
        
        return self._generate_with_template(prompt, language, context)
    
    def _generate_with_template(self, prompt, language, context):
        """Generate code using templates"""
        if language not in self.generation_templates:
            language = 'python'
        
        # Simple template-based generation
        if 'function' in prompt.lower():
            return self.generation_templates[language]['function'].format(
                name='generated_function',
                params='param1, param2',
                docstring='Generated function',
                body='pass  # TODO: Implement'
            )
        elif 'class' in prompt.lower():
            return self.generation_templates[language]['class'].format(
                name='GeneratedClass',
                params='param1, param2',
                docstring='Generated class',
                body='pass  # TODO: Implement'
            )
        else:
            return f"# Generated {language} code\n# TODO: Implement {prompt}"

class AIDebugger:
    """AI-powered debugging assistance"""
    
    def __init__(self, manager):
        self.manager = manager
        self.common_errors = {
            'NameError': 'Variable not defined',
            'TypeError': 'Incorrect data type',
            'ValueError': 'Invalid value',
            'IndexError': 'List index out of range',
            'KeyError': 'Dictionary key not found',
            'AttributeError': 'Object has no attribute',
            'ImportError': 'Module not found',
            'SyntaxError': 'Invalid syntax'
        }
    
    def analyze_and_fix(self, code, error_message=""):
        """Analyze code and suggest fixes"""
        issues = []
        suggestions = []
        
        # Check for common Python issues
        if 'python' in code.lower() or '.py' in code:
            issues.extend(self._analyze_python_code(code))
        
        # Check for common JavaScript issues
        if 'javascript' in code.lower() or '.js' in code:
            issues.extend(self._analyze_javascript_code(code))
        
        # Generate suggestions
        for issue in issues:
            suggestions.append(self._generate_fix_suggestion(issue, error_message))
        
        return {
            'issues': issues,
            'suggestions': suggestions,
            'fixed_code': self._apply_fixes(code, suggestions)
        }
    
    def _analyze_python_code(self, code):
        """Analyze Python code for issues"""
        issues = []
        
        try:
            ast.parse(code)
        except SyntaxError as e:
            issues.append({
                'type': 'SyntaxError',
                'message': str(e),
                'line': e.lineno,
                'column': e.offset
            })
        
        # Check for common issues
        if 'eval(' in code:
            issues.append({
                'type': 'Security',
                'message': 'Use of eval() is dangerous',
                'line': code.find('eval('),
                'suggestion': 'Use safer alternatives like ast.literal_eval()'
            })
        
        if 'exec(' in code:
            issues.append({
                'type': 'Security',
                'message': 'Use of exec() is dangerous',
                'line': code.find('exec('),
                'suggestion': 'Avoid exec() for security reasons'
            })
        
        return issues
    
    def _analyze_javascript_code(self, code):
        """Analyze JavaScript code for issues"""
        issues = []
        
        # Check for common issues
        if 'var ' in code and 'let ' not in code and 'const ' not in code:
            issues.append({
                'type': 'Style',
                'message': 'Consider using let/const instead of var',
                'suggestion': 'Use let for variables that change, const for constants'
            })
        
        if '== ' in code and '=== ' not in code:
            issues.append({
                'type': 'Best Practice',
                'message': 'Consider using strict equality (===)',
                'suggestion': 'Use === instead of == for type-safe comparison'
            })
        
        return issues
    
    def _generate_fix_suggestion(self, issue, error_message):
        """Generate fix suggestion for an issue"""
        return {
            'issue': issue,
            'suggestion': issue.get('suggestion', 'Review the code for potential issues'),
            'priority': 'high' if issue['type'] in ['Security', 'SyntaxError'] else 'medium'
        }
    
    def _apply_fixes(self, code, suggestions):
        """Apply suggested fixes to code"""
        fixed_code = code
        
        for suggestion in suggestions:
            if 'eval(' in fixed_code and 'Security' in str(suggestion):
                fixed_code = fixed_code.replace('eval(', 'ast.literal_eval(')
            elif 'var ' in fixed_code and 'Style' in str(suggestion):
                fixed_code = fixed_code.replace('var ', 'let ')
        
        return fixed_code

class AIOptimizer:
    """AI-powered code optimization"""
    
    def __init__(self, manager):
        self.manager = manager
        self.optimization_patterns = {
            'python': {
                'list_comprehension': r'for\s+\w+\s+in\s+\w+:\s*\n\s*if\s+.*:\s*\n\s*\w+\.append\(',
                'generator_expression': r'list\(.*for.*in.*\)',
                'enumerate': r'for\s+\w+\s+in\s+range\(len\(\w+\)\):',
                'zip': r'for\s+\w+\s+in\s+range\(len\(\w+\)\):\s*\n\s*\w+\s*=\s*\w+\[\w+\]'
            },
            'javascript': {
                'arrow_functions': r'function\s+\w+\s*\([^)]*\)\s*{',
                'const_let': r'var\s+\w+',
                'template_literals': r'"[^"]*"\s*\+\s*"[^"]*"',
                'destructuring': r'\w+\s*=\s*\w+\.\w+'
            }
        }
    
    def optimize_code(self, code, language="python"):
        """Optimize code using AI analysis"""
        optimizations = []
        optimized_code = code
        
        if language in self.optimization_patterns:
            for pattern_name, pattern in self.optimization_patterns[language].items():
                if re.search(pattern, code, re.MULTILINE):
                    optimization = self._suggest_optimization(pattern_name, code, language)
                    if optimization:
                        optimizations.append(optimization)
                        optimized_code = self._apply_optimization(optimized_code, optimization)
        
        return {
            'original_code': code,
            'optimized_code': optimized_code,
            'optimizations': optimizations,
            'performance_improvement': self._calculate_improvement(code, optimized_code)
        }
    
    def _suggest_optimization(self, pattern_name, code, language):
        """Suggest specific optimization"""
        suggestions = {
            'list_comprehension': 'Use list comprehension for better performance',
            'generator_expression': 'Use generator expression to save memory',
            'enumerate': 'Use enumerate() instead of range(len())',
            'zip': 'Use zip() for parallel iteration',
            'arrow_functions': 'Use arrow functions for cleaner syntax',
            'const_let': 'Use const/let instead of var',
            'template_literals': 'Use template literals for string concatenation',
            'destructuring': 'Use destructuring assignment'
        }
        
        return {
            'type': pattern_name,
            'suggestion': suggestions.get(pattern_name, 'Consider optimization'),
            'impact': 'medium'
        }
    
    def _apply_optimization(self, code, optimization):
        """Apply optimization to code"""
        # Simple optimizations
        if optimization['type'] == 'const_let':
            code = re.sub(r'var\s+', 'let ', code)
        elif optimization['type'] == 'template_literals':
            code = re.sub(r'"([^"]*)"\s*\+\s*"([^"]*)"', r'`\1\2`', code)
        
        return code
    
    def _calculate_improvement(self, original, optimized):
        """Calculate performance improvement percentage"""
        # Simple heuristic based on code length and complexity
        original_lines = len(original.split('\n'))
        optimized_lines = len(optimized.split('\n'))
        
        if original_lines > 0:
            return max(0, (original_lines - optimized_lines) / original_lines * 100)
        return 0

class AISecurityAnalyzer:
    """AI-powered security analysis"""
    
    def __init__(self, manager):
        self.manager = manager
        self.security_patterns = {
            'python': {
                'eval': r'eval\s*\(',
                'exec': r'exec\s*\(',
                'pickle': r'pickle\.loads?\s*\(',
                'subprocess': r'subprocess\.(run|call|Popen)\s*\(',
                'os_system': r'os\.system\s*\(',
                'shell_injection': r'shell=True',
                'sql_injection': r'execute\s*\(\s*["\'].*%.*["\']',
                'path_traversal': r'\.\./',
                'hardcoded_secrets': r'(password|secret|key|token)\s*=\s*["\'][^"\']+["\']'
            },
            'javascript': {
                'eval': r'eval\s*\(',
                'innerHTML': r'\.innerHTML\s*=',
                'document_write': r'document\.write\s*\(',
                'xss': r'<script[^>]*>',
                'sql_injection': r'query\s*\(\s*["\'].*%.*["\']',
                'hardcoded_secrets': r'(password|secret|key|token)\s*[:=]\s*["\'][^"\']+["\']'
            }
        }
    
    def analyze_security(self, code, language="python"):
        """Analyze code for security vulnerabilities"""
        vulnerabilities = []
        
        if language in self.security_patterns:
            for vuln_type, pattern in self.security_patterns[language].items():
                matches = re.finditer(pattern, code, re.IGNORECASE)
                for match in matches:
                    vulnerabilities.append({
                        'type': vuln_type,
                        'severity': self._get_severity(vuln_type),
                        'line': code[:match.start()].count('\n') + 1,
                        'column': match.start() - code.rfind('\n', 0, match.start()) - 1,
                        'description': self._get_description(vuln_type),
                        'recommendation': self._get_recommendation(vuln_type)
                    })
        
        return {
            'vulnerabilities': vulnerabilities,
            'risk_score': self._calculate_risk_score(vulnerabilities),
            'recommendations': self._generate_recommendations(vulnerabilities)
        }
    
    def _get_severity(self, vuln_type):
        """Get severity level for vulnerability type"""
        high_severity = ['eval', 'exec', 'shell_injection', 'xss']
        medium_severity = ['subprocess', 'os_system', 'sql_injection']
        low_severity = ['hardcoded_secrets', 'path_traversal']
        
        if vuln_type in high_severity:
            return 'high'
        elif vuln_type in medium_severity:
            return 'medium'
        else:
            return 'low'
    
    def _get_description(self, vuln_type):
        """Get description for vulnerability type"""
        descriptions = {
            'eval': 'Use of eval() can lead to code injection',
            'exec': 'Use of exec() can lead to code injection',
            'pickle': 'Unsafe deserialization with pickle',
            'subprocess': 'Potential command injection via subprocess',
            'os_system': 'Potential command injection via os.system()',
            'shell_injection': 'Shell injection vulnerability',
            'sql_injection': 'Potential SQL injection vulnerability',
            'path_traversal': 'Potential path traversal vulnerability',
            'hardcoded_secrets': 'Hardcoded secrets in code',
            'innerHTML': 'Potential XSS via innerHTML',
            'document_write': 'Potential XSS via document.write',
            'xss': 'Cross-site scripting vulnerability'
        }
        return descriptions.get(vuln_type, 'Security vulnerability detected')
    
    def _get_recommendation(self, vuln_type):
        """Get recommendation for vulnerability type"""
        recommendations = {
            'eval': 'Use ast.literal_eval() or safer alternatives',
            'exec': 'Avoid exec() or use restricted execution',
            'pickle': 'Use json or other safe serialization',
            'subprocess': 'Use shell=False and validate inputs',
            'os_system': 'Use subprocess with shell=False',
            'shell_injection': 'Validate and sanitize inputs',
            'sql_injection': 'Use parameterized queries',
            'path_traversal': 'Validate file paths',
            'hardcoded_secrets': 'Use environment variables or secure storage',
            'innerHTML': 'Use textContent or safe HTML methods',
            'document_write': 'Use DOM manipulation methods',
            'xss': 'Sanitize user inputs and use CSP'
        }
        return recommendations.get(vuln_type, 'Review and fix the vulnerability')
    
    def _calculate_risk_score(self, vulnerabilities):
        """Calculate overall risk score"""
        if not vulnerabilities:
            return 0
        
        high_count = sum(1 for v in vulnerabilities if v['severity'] == 'high')
        medium_count = sum(1 for v in vulnerabilities if v['severity'] == 'medium')
        low_count = sum(1 for v in vulnerabilities if v['severity'] == 'low')
        
        return min(100, (high_count * 10 + medium_count * 5 + low_count * 2))
    
    def _generate_recommendations(self, vulnerabilities):
        """Generate overall security recommendations"""
        if not vulnerabilities:
            return ['No security issues detected']
        
        recommendations = []
        
        if any(v['type'] in ['eval', 'exec'] for v in vulnerabilities):
            recommendations.append('Remove or replace eval() and exec() usage')
        
        if any(v['type'] in ['subprocess', 'os_system'] for v in vulnerabilities):
            recommendations.append('Use subprocess with shell=False and input validation')
        
        if any(v['type'] == 'sql_injection' for v in vulnerabilities):
            recommendations.append('Use parameterized queries to prevent SQL injection')
        
        if any(v['type'] in ['xss', 'innerHTML'] for v in vulnerabilities):
            recommendations.append('Sanitize user inputs and use safe DOM methods')
        
        return recommendations

class AIDocumentationGenerator:
    """AI-powered documentation generation"""
    
    def __init__(self, manager):
        self.manager = manager
    
    def generate_docs(self, code, language="python"):
        """Generate documentation for code"""
        functions = self._extract_functions(code, language)
        classes = self._extract_classes(code, language)
        
        documentation = {
            'overview': self._generate_overview(code, language),
            'functions': [self._document_function(func) for func in functions],
            'classes': [self._document_class(cls) for cls in classes],
            'usage_examples': self._generate_usage_examples(code, language)
        }
        
        return documentation
    
    def _extract_functions(self, code, language):
        """Extract functions from code"""
        functions = []
        
        if language == 'python':
            try:
                tree = ast.parse(code)
                for node in ast.walk(tree):
                    if isinstance(node, ast.FunctionDef):
                        functions.append({
                            'name': node.name,
                            'args': [arg.arg for arg in node.args.args],
                            'docstring': ast.get_docstring(node),
                            'line_number': node.lineno
                        })
            except SyntaxError:
                pass
        
        return functions
    
    def _extract_classes(self, code, language):
        """Extract classes from code"""
        classes = []
        
        if language == 'python':
            try:
                tree = ast.parse(code)
                for node in ast.walk(tree):
                    if isinstance(node, ast.ClassDef):
                        classes.append({
                            'name': node.name,
                            'docstring': ast.get_docstring(node),
                            'line_number': node.lineno
                        })
            except SyntaxError:
                pass
        
        return classes
    
    def _generate_overview(self, code, language):
        """Generate code overview"""
        lines = code.split('\n')
        non_empty_lines = [line for line in lines if line.strip()]
        
        return {
            'language': language,
            'total_lines': len(lines),
            'non_empty_lines': len(non_empty_lines),
            'complexity': 'low' if len(non_empty_lines) < 50 else 'medium' if len(non_empty_lines) < 200 else 'high'
        }
    
    def _document_function(self, func):
        """Generate documentation for a function"""
        return {
            'name': func['name'],
            'description': func.get('docstring', 'No description available'),
            'parameters': func.get('args', []),
            'line_number': func.get('line_number', 0)
        }
    
    def _document_class(self, cls):
        """Generate documentation for a class"""
        return {
            'name': cls['name'],
            'description': cls.get('docstring', 'No description available'),
            'line_number': cls.get('line_number', 0)
        }
    
    def _generate_usage_examples(self, code, language):
        """Generate usage examples"""
        examples = []
        
        if language == 'python':
            # Look for function definitions and generate examples
            functions = self._extract_functions(code, language)
            for func in functions[:3]:  # Limit to first 3 functions
                example = f"# Example usage of {func['name']}\n"
                if func['args']:
                    args_str = ', '.join(func['args'])
                    example += f"result = {func['name']}({args_str})\n"
                else:
                    example += f"result = {func['name']}()\n"
                examples.append(example)
        
        return examples

class AITestGenerator:
    """AI-powered test generation"""
    
    def __init__(self, manager):
        self.manager = manager
    
    def generate_tests(self, code, language="python"):
        """Generate tests for code"""
        functions = self._extract_functions(code, language)
        classes = self._extract_classes(code, language)
        
        tests = {
            'unit_tests': [self._generate_unit_test(func, language) for func in functions],
            'integration_tests': self._generate_integration_tests(code, language),
            'test_framework': self._get_test_framework(language)
        }
        
        return tests
    
    def _extract_functions(self, code, language):
        """Extract functions from code"""
        functions = []
        
        if language == 'python':
            try:
                tree = ast.parse(code)
                for node in ast.walk(tree):
                    if isinstance(node, ast.FunctionDef):
                        functions.append({
                            'name': node.name,
                            'args': [arg.arg for arg in node.args.args],
                            'line_number': node.lineno
                        })
            except SyntaxError:
                pass
        
        return functions
    
    def _extract_classes(self, code, language):
        """Extract classes from code"""
        classes = []
        
        if language == 'python':
            try:
                tree = ast.parse(code)
                for node in ast.walk(tree):
                    if isinstance(node, ast.ClassDef):
                        classes.append({
                            'name': node.name,
                            'line_number': node.lineno
                        })
            except SyntaxError:
                pass
        
        return classes
    
    def _generate_unit_test(self, func, language):
        """Generate unit test for a function"""
        if language == 'python':
            test_code = f"""
def test_{func['name']}():
    \"\"\"Test {func['name']} function\"\"\"
    # Test case 1: Basic functionality
    result = {func['name']}()
    assert result is not None
    
    # Test case 2: Edge cases
    # TODO: Add specific test cases
    
    # Test case 3: Error handling
    # TODO: Add error test cases
"""
            return {
                'function_name': func['name'],
                'test_code': test_code.strip(),
                'test_framework': 'pytest'
            }
        
        return None
    
    def _generate_integration_tests(self, code, language):
        """Generate integration tests"""
        if language == 'python':
            return {
                'test_code': """
def test_integration():
    \"\"\"Integration test for the entire module\"\"\"
    # Test the complete workflow
    # TODO: Implement integration test
""",
                'description': 'Integration test for the complete functionality'
            }
        
        return None
    
    def _get_test_framework(self, language):
        """Get recommended test framework for language"""
        frameworks = {
            'python': 'pytest',
            'javascript': 'jest',
            'java': 'junit',
            'cpp': 'gtest',
            'csharp': 'nunit'
        }
        return frameworks.get(language, 'unittest')

class AIRefactoringEngine:
    """AI-powered code refactoring"""
    
    def __init__(self, manager):
        self.manager = manager
        self.refactoring_patterns = {
            'python': {
                'extract_method': r'def\s+\w+\([^)]*\):\s*\n\s+.*\n\s+.*\n\s+.*',
                'extract_variable': r'(\w+)\s*=\s*([^=]+)\s*\n\s*\1\s*=\s*\1\s*[+\-*/]\s*\1',
                'inline_method': r'def\s+(\w+)\([^)]*\):\s*\n\s+return\s+([^;]+)',
                'rename_variable': r'(\w+)\s*=\s*[^=]+',
                'remove_dead_code': r'#.*\n\s*pass\s*$'
            }
        }
    
    def refactor_code(self, code, language="python", refactor_type="general"):
        """Refactor code using AI analysis"""
        refactorings = []
        refactored_code = code
        
        if language in self.refactoring_patterns:
            for pattern_name, pattern in self.refactoring_patterns[language].items():
                if re.search(pattern, code, re.MULTILINE):
                    refactoring = self._suggest_refactoring(pattern_name, code, language)
                    if refactoring:
                        refactorings.append(refactoring)
                        refactored_code = self._apply_refactoring(refactored_code, refactoring)
        
        return {
            'original_code': code,
            'refactored_code': refactored_code,
            'refactorings': refactorings,
            'improvements': self._calculate_improvements(code, refactored_code)
        }
    
    def _suggest_refactoring(self, pattern_name, code, language):
        """Suggest specific refactoring"""
        suggestions = {
            'extract_method': 'Extract repeated code into a method',
            'extract_variable': 'Extract complex expressions into variables',
            'inline_method': 'Inline simple methods',
            'rename_variable': 'Rename variables for clarity',
            'remove_dead_code': 'Remove unused code'
        }
        
        return {
            'type': pattern_name,
            'suggestion': suggestions.get(pattern_name, 'Consider refactoring'),
            'impact': 'medium'
        }
    
    def _apply_refactoring(self, code, refactoring):
        """Apply refactoring to code"""
        # Simple refactorings
        if refactoring['type'] == 'remove_dead_code':
            code = re.sub(r'#.*\n\s*pass\s*$', '', code, flags=re.MULTILINE)
        
        return code
    
    def _calculate_improvements(self, original, refactored):
        """Calculate code improvements"""
        original_lines = len(original.split('\n'))
        refactored_lines = len(refactored.split('\n'))
        
        return {
            'lines_reduced': max(0, original_lines - refactored_lines),
            'readability_improved': True,
            'maintainability_improved': True
        }

class AIPatternRecognizer:
    """AI-powered pattern recognition"""
    
    def __init__(self, manager):
        self.manager = manager
        self.patterns = {
            'python': {
                'singleton': r'class\s+\w+:\s*\n\s*_instance\s*=\s*None',
                'factory': r'def\s+create_\w+\s*\(',
                'observer': r'def\s+notify\s*\(',
                'decorator': r'def\s+\w+\(func\):\s*\n\s*def\s+wrapper',
                'generator': r'yield\s+',
                'context_manager': r'def\s+__enter__\s*\(',
                'iterator': r'def\s+__iter__\s*\(',
                'descriptor': r'def\s+__get__\s*\('
            },
            'javascript': {
                'module': r'module\.exports\s*=\s*',
                'promise': r'new\s+Promise\s*\(',
                'async_await': r'async\s+function',
                'callback': r'function\s*\(\s*err\s*,\s*data\s*\)',
                'closure': r'function\s+\w+\s*\([^)]*\)\s*{\s*return\s+function',
                'prototype': r'\.prototype\.',
                'class': r'class\s+\w+'
            }
        }
    
    def recognize_patterns(self, code, language="python"):
        """Recognize design patterns in code"""
        recognized_patterns = []
        
        if language in self.patterns:
            for pattern_name, pattern in self.patterns[language].items():
                if re.search(pattern, code, re.MULTILINE):
                    recognized_patterns.append({
                        'pattern': pattern_name,
                        'description': self._get_pattern_description(pattern_name),
                        'confidence': self._calculate_confidence(pattern, code),
                        'suggestions': self._get_pattern_suggestions(pattern_name)
                    })
        
        return {
            'patterns': recognized_patterns,
            'design_quality': self._assess_design_quality(recognized_patterns),
            'recommendations': self._generate_pattern_recommendations(recognized_patterns)
        }
    
    def _get_pattern_description(self, pattern_name):
        """Get description for design pattern"""
        descriptions = {
            'singleton': 'Singleton pattern ensures only one instance exists',
            'factory': 'Factory pattern creates objects without specifying exact classes',
            'observer': 'Observer pattern notifies multiple objects about changes',
            'decorator': 'Decorator pattern adds behavior to objects dynamically',
            'generator': 'Generator pattern provides iterator functionality',
            'context_manager': 'Context manager pattern manages resources',
            'iterator': 'Iterator pattern provides sequential access to elements',
            'descriptor': 'Descriptor pattern controls attribute access',
            'module': 'Module pattern organizes code into logical units',
            'promise': 'Promise pattern handles asynchronous operations',
            'async_await': 'Async/await pattern simplifies asynchronous code',
            'callback': 'Callback pattern handles asynchronous operations',
            'closure': 'Closure pattern creates private variables',
            'prototype': 'Prototype pattern creates objects from existing objects',
            'class': 'Class pattern defines object blueprints'
        }
        return descriptions.get(pattern_name, 'Design pattern detected')
    
    def _calculate_confidence(self, pattern, code):
        """Calculate confidence level for pattern recognition"""
        matches = len(re.findall(pattern, code, re.MULTILINE))
        if matches > 3:
            return 'high'
        elif matches > 1:
            return 'medium'
        else:
            return 'low'
    
    def _get_pattern_suggestions(self, pattern_name):
        """Get suggestions for pattern usage"""
        suggestions = {
            'singleton': 'Consider if singleton is really needed, can make testing difficult',
            'factory': 'Good choice for creating objects based on conditions',
            'observer': 'Excellent for decoupling components',
            'decorator': 'Great for adding functionality without modifying original code',
            'generator': 'Perfect for memory-efficient iteration',
            'context_manager': 'Best practice for resource management',
            'iterator': 'Good for providing consistent iteration interface',
            'descriptor': 'Advanced pattern for attribute control',
            'module': 'Good for organizing code and avoiding global namespace pollution',
            'promise': 'Modern approach to handling asynchronous operations',
            'async_await': 'Cleaner syntax for asynchronous operations',
            'callback': 'Traditional approach, consider promises or async/await',
            'closure': 'Great for creating private variables and functions',
            'prototype': 'Traditional JavaScript inheritance',
            'class': 'Modern JavaScript class syntax'
        }
        return suggestions.get(pattern_name, 'Consider the implications of this pattern')
    
    def _assess_design_quality(self, patterns):
        """Assess overall design quality based on patterns"""
        if not patterns:
            return 'basic'
        
        high_confidence_patterns = [p for p in patterns if p['confidence'] == 'high']
        if len(high_confidence_patterns) >= 3:
            return 'excellent'
        elif len(high_confidence_patterns) >= 2:
            return 'good'
        else:
            return 'fair'
    
    def _generate_pattern_recommendations(self, patterns):
        """Generate recommendations based on recognized patterns"""
        if not patterns:
            return ['Consider implementing design patterns to improve code structure']
        
        recommendations = []
        
        if any(p['pattern'] == 'singleton' for p in patterns):
            recommendations.append('Review singleton usage - consider dependency injection')
        
        if any(p['pattern'] == 'callback' for p in patterns):
            recommendations.append('Consider modernizing to promises or async/await')
        
        if any(p['pattern'] == 'factory' for p in patterns):
            recommendations.append('Factory pattern is well implemented')
        
        return recommendations

if __name__ == "__main__":
    print("🤖 AI Helper Classes loaded successfully!")
    print("Available classes:")
    print("- AICodeGenerator: Generate code with AI")
    print("- AIDebugger: Debug code with AI assistance")
    print("- AIOptimizer: Optimize code performance")
    print("- AISecurityAnalyzer: Analyze security vulnerabilities")
    print("- AIDocumentationGenerator: Generate documentation")
    print("- AITestGenerator: Generate tests automatically")
    print("- AIRefactoringEngine: Refactor code intelligently")
    print("- AIPatternRecognizer: Recognize design patterns")
