#!/usr/bin/env python3
"""
AI-Powered Code Completion and Suggestions
NO EMOJIS - Plain ASCII only to avoid ROE corruption
"""

import re
import json
import requests
from typing import List, Dict, Optional
from dataclasses import dataclass

@dataclass
class CodeSuggestion:
    text: str
    description: str
    confidence: float
    category: str

class AICodeAssistant:
    def __init__(self):
        self.keywords = {
            'python': ['def', 'class', 'if', 'else', 'elif', 'for', 'while', 'try', 'except', 'import', 'from', 'return'],
            'javascript': ['function', 'var', 'let', 'const', 'if', 'else', 'for', 'while', 'try', 'catch', 'return'],
            'eon': ['function', 'class', 'var', 'if', 'else', 'for', 'while', 'try', 'catch', 'import', 'module']
        }
        
        self.patterns = {
            'function_def': r'def\s+(\w+)\s*\(',
            'class_def': r'class\s+(\w+)',
            'variable_assignment': r'(\w+)\s*=\s*',
            'import_statement': r'import\s+(\w+)',
            'method_call': r'(\w+)\.(\w+)\('
        }
        
        self.common_completions = {
            'python': {
                'print(': 'print()',
                'len(': 'len()',
                'range(': 'range()',
                'open(': 'open()',
                'str(': 'str()',
                'int(': 'int()',
                'float(': 'float()',
                'list(': 'list()',
                'dict(': 'dict()'
            },
            'javascript': {
                'console.log(': 'console.log()',
                'document.': 'document.',
                'window.': 'window.',
                'function(': 'function()',
                'var ': 'var ',
                'let ': 'let ',
                'const ': 'const '
            }
        }

    def get_completions(self, code: str, cursor_pos: int, language: str = 'python') -> List[CodeSuggestion]:
        """Get AI-powered code completions"""
        suggestions = []
        
        # Get current context
        context = self.get_context(code, cursor_pos)
        current_word = self.get_current_word(code, cursor_pos)
        
        # Add keyword completions
        suggestions.extend(self.get_keyword_completions(current_word, language))
        
        # Add pattern-based completions
        suggestions.extend(self.get_pattern_completions(context, language))
        
        # Add common function completions
        suggestions.extend(self.get_common_completions(current_word, language))
        
        # Add context-aware suggestions
        suggestions.extend(self.get_context_suggestions(context, language))
        
        # Sort by confidence
        suggestions.sort(key=lambda x: x.confidence, reverse=True)
        
        return suggestions[:10]  # Return top 10

    def get_context(self, code: str, cursor_pos: int, lines_before: int = 5) -> str:
        """Get code context around cursor position"""
        lines = code[:cursor_pos].split('\n')
        start_line = max(0, len(lines) - lines_before)
        return '\n'.join(lines[start_line:])

    def get_current_word(self, code: str, cursor_pos: int) -> str:
        """Get the current word being typed"""
        if cursor_pos > len(code):
            cursor_pos = len(code)
        
        # Find word boundaries
        start = cursor_pos
        while start > 0 and code[start - 1].isalnum() or code[start - 1] == '_':
            start -= 1
        
        end = cursor_pos
        while end < len(code) and code[end].isalnum() or code[end] == '_':
            end += 1
        
        return code[start:cursor_pos]

    def get_keyword_completions(self, current_word: str, language: str) -> List[CodeSuggestion]:
        """Get keyword-based completions"""
        suggestions = []
        keywords = self.keywords.get(language, [])
        
        for keyword in keywords:
            if keyword.startswith(current_word.lower()):
                suggestions.append(CodeSuggestion(
                    text=keyword,
                    description=f"{language} keyword",
                    confidence=0.8,
                    category="keyword"
                ))
        
        return suggestions

    def get_pattern_completions(self, context: str, language: str) -> List[CodeSuggestion]:
        """Get pattern-based completions"""
        suggestions = []
        
        # Check for function definition pattern
        if re.search(r'def\s+\w*$', context):
            suggestions.append(CodeSuggestion(
                text="function_name(args):",
                description="Function definition",
                confidence=0.9,
                category="function"
            ))
        
        # Check for class definition pattern
        if re.search(r'class\s+\w*$', context):
            suggestions.append(CodeSuggestion(
                text="ClassName:",
                description="Class definition",
                confidence=0.9,
                category="class"
            ))
        
        # Check for if statement pattern
        if context.strip().endswith('if'):
            suggestions.append(CodeSuggestion(
                text="if condition:",
                description="If statement",
                confidence=0.85,
                category="control"
            ))
        
        return suggestions

    def get_common_completions(self, current_word: str, language: str) -> List[CodeSuggestion]:
        """Get common function/method completions"""
        suggestions = []
        completions = self.common_completions.get(language, {})
        
        for completion, full_text in completions.items():
            if completion.startswith(current_word.lower()):
                suggestions.append(CodeSuggestion(
                    text=full_text,
                    description=f"Common {language} function",
                    confidence=0.7,
                    category="function"
                ))
        
        return suggestions

    def get_context_suggestions(self, context: str, language: str) -> List[CodeSuggestion]:
        """Get context-aware suggestions"""
        suggestions = []
        
        # Analyze context for variables and functions
        variables = re.findall(r'(\w+)\s*=', context)
        functions = re.findall(r'def\s+(\w+)', context)
        
        # Suggest variable names
        for var in variables:
            suggestions.append(CodeSuggestion(
                text=var,
                description=f"Variable: {var}",
                confidence=0.6,
                category="variable"
            ))
        
        # Suggest function names
        for func in functions:
            suggestions.append(CodeSuggestion(
                text=f"{func}(",
                description=f"Function: {func}",
                confidence=0.6,
                category="function"
            ))
        
        return suggestions

    def analyze_code_quality(self, code: str, language: str) -> List[Dict]:
        """Analyze code quality and suggest improvements"""
        issues = []
        
        # Check for common issues
        lines = code.split('\n')
        
        for i, line in enumerate(lines, 1):
            # Long line check
            if len(line) > 120:
                issues.append({
                    'line': i,
                    'type': 'style',
                    'severity': 'warning',
                    'message': 'Line too long (>120 characters)',
                    'suggestion': 'Consider breaking this line into multiple lines'
                })
            
            # Unused variables (simple check)
            var_match = re.match(r'\s*(\w+)\s*=', line)
            if var_match and language == 'python':
                var_name = var_match.group(1)
                if var_name not in code[code.find(line) + len(line):]:
                    issues.append({
                        'line': i,
                        'type': 'warning',
                        'severity': 'info',
                        'message': f'Unused variable: {var_name}',
                        'suggestion': f'Remove unused variable or use it'
                    })
        
        return issues

    def suggest_refactoring(self, code: str, language: str) -> List[Dict]:
        """Suggest code refactoring opportunities"""
        suggestions = []
        
        # Find duplicated code blocks
        lines = code.split('\n')
        for i in range(len(lines) - 2):
            for j in range(i + 3, len(lines) - 2):
                if lines[i:i+3] == lines[j:j+3]:
                    suggestions.append({
                        'type': 'duplication',
                        'lines': [i+1, j+1],
                        'message': 'Duplicated code block detected',
                        'suggestion': 'Extract common code into a function'
                    })
        
        # Find long functions
        function_lines = []
        current_function = None
        indent_level = 0
        
        for i, line in enumerate(lines):
            if re.match(r'\s*def\s+(\w+)', line):
                if current_function:
                    function_lines.append((current_function, len(function_lines)))
                current_function = i
                function_lines = []
                indent_level = len(line) - len(line.lstrip())
            elif current_function is not None:
                line_indent = len(line) - len(line.lstrip())
                if line.strip() and line_indent <= indent_level and not line.strip().startswith('#'):
                    # Function ended
                    if len(function_lines) > 50:
                        suggestions.append({
                            'type': 'complexity',
                            'line': current_function + 1,
                            'message': f'Function is too long ({len(function_lines)} lines)',
                            'suggestion': 'Consider breaking this function into smaller functions'
                        })
                    current_function = None
                else:
                    function_lines.append(i)
        
        return suggestions

    def generate_documentation(self, code: str, language: str) -> str:
        """Generate documentation for code"""
        doc = []
        
        # Find functions and classes
        functions = re.findall(r'def\s+(\w+)\s*\([^)]*\):', code)
        classes = re.findall(r'class\s+(\w+)', code)
        
        if functions:
            doc.append("Functions:")
            for func in functions:
                doc.append(f"  - {func}(): [Add description]")
        
        if classes:
            doc.append("\nClasses:")
            for cls in classes:
                doc.append(f"  - {cls}: [Add description]")
        
        return '\n'.join(doc)

# Example usage
if __name__ == "__main__":
    assistant = AICodeAssistant()
    
    # Test code completion
    test_code = """
def hello_world():
    print("Hello, World!")

x = 10
y = 20
pri
"""
    
    suggestions = assistant.get_completions(test_code, len(test_code) - 1, 'python')
    print("Code Completions:")
    for suggestion in suggestions:
        print(f"  {suggestion.text} - {suggestion.description} ({suggestion.confidence:.2f})")
    
    # Test code quality analysis
    quality_issues = assistant.analyze_code_quality(test_code, 'python')
    print("\nCode Quality Issues:")
    for issue in quality_issues:
        print(f"  Line {issue['line']}: {issue['message']}")
    
    print("\nAI Code Assistant Ready!")
