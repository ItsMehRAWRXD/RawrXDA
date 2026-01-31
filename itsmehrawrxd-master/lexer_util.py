#!/usr/bin/env python3
"""
Lexer Utility Functions
Reusable tokenization utilities for all language lexers
"""

import re
from typing import List, Any, Tuple

def get_simple_token_specs(keywords: List[str], operators: List[str], delimiters: List[str]) -> List[Tuple[str, str]]:
    """Generates token specifications from lists of keywords, operators, and delimiters."""
    specs = []
    
    # Keywords - use word boundaries to avoid partial matches
    for kw in keywords:
        specs.append((rf'\b{re.escape(kw)}\b', f'KEYWORD_{kw.upper()}'))
    
    # Identifiers - must start with letter or underscore
    specs.append((r'[a-zA-Z_][a-zA-Z0-9_]*', 'IDENTIFIER'))
    
    # Numbers - integers and floats
    specs.append((r'\d+(\.\d*)?([eE][+-]?\d+)?', 'NUMBER'))
    
    # String literals - handle both single and double quotes
    specs.append((r'"(?:[^"\\]|\\.)*"', 'STRING'))
    specs.append((r"'(?:[^'\\]|\\.)*'", 'STRING'))
    
    # Operators - sort by length to match longer ones first (e.g., '==' before '=')
    for op in sorted(operators, key=len, reverse=True):
        specs.append((re.escape(op), f'OPERATOR_{op}'))
    
    # Delimiters - sort by length for same reason
    for delim in sorted(delimiters, key=len, reverse=True):
        specs.append((re.escape(delim), f'DELIMITER_{delim}'))
    
    return specs

def simple_tokenize(source: str, specs: List[Tuple[str, str]]) -> List[Any]:
    """A simple tokenization function that uses regex."""
    tokens = []
    current_pos = 0
    
    while current_pos < len(source):
        # Skip whitespace
        whitespace_match = re.match(r'\s+', source[current_pos:])
        if whitespace_match:
            current_pos += whitespace_match.end()
            continue
        
        # Check for token matches
        match = None
        for pattern, tag in specs:
            regex = re.compile(pattern)
            match = regex.match(source, current_pos)
            if match:
                tokens.append({
                    'type': tag, 
                    'value': match.group(0), 
                    'start': match.start(), 
                    'end': match.end(),
                    'line': source[:match.start()].count('\n') + 1,
                    'column': match.start() - source.rfind('\n', 0, match.start()) - 1
                })
                current_pos = match.end()
                break
        
        if not match:
            # Handle unknown token
            raise ValueError(f"Unknown token at position {current_pos}: '{source[current_pos:current_pos+10]}...'")

    return tokens

def tokenize_with_comments(source: str, specs: List[Tuple[str, str]], comment_patterns: List[Tuple[str, str]] = None) -> List[Any]:
    """Enhanced tokenization that handles comments."""
    if comment_patterns is None:
        comment_patterns = [
            (r'//.*$', 'SINGLE_LINE_COMMENT'),
            (r'/\*.*?\*/', 'MULTI_LINE_COMMENT'),
            (r'#.*$', 'HASH_COMMENT')
        ]
    
    # Add comment patterns to specs
    all_specs = specs + comment_patterns
    
    tokens = []
    current_pos = 0
    
    while current_pos < len(source):
        # Skip whitespace
        whitespace_match = re.match(r'\s+', source[current_pos:])
        if whitespace_match:
            current_pos += whitespace_match.end()
            continue
        
        # Check for token matches
        match = None
        for pattern, tag in all_specs:
            regex = re.compile(pattern, re.MULTILINE | re.DOTALL)
            match = regex.match(source, current_pos)
            if match:
                # Skip comments
                if 'COMMENT' not in tag:
                    tokens.append({
                        'type': tag, 
                        'value': match.group(0), 
                        'start': match.start(), 
                        'end': match.end(),
                        'line': source[:match.start()].count('\n') + 1,
                        'column': match.start() - source.rfind('\n', 0, match.start()) - 1
                    })
                current_pos = match.end()
                break
        
        if not match:
            # Handle unknown token
            raise ValueError(f"Unknown token at position {current_pos}: '{source[current_pos:current_pos+10]}...'")

    return tokens

def validate_tokens(tokens: List[Any]) -> List[Any]:
    """Validate and clean up token list."""
    validated = []
    
    for token in tokens:
        # Ensure required fields
        if 'type' not in token or 'value' not in token:
            continue
            
        # Clean up string literals
        if token['type'] == 'STRING':
            value = token['value']
            if len(value) >= 2:
                # Remove quotes and handle escape sequences
                if value.startswith('"') and value.endswith('"'):
                    token['value'] = value[1:-1]
                elif value.startswith("'") and value.endswith("'"):
                    token['value'] = value[1:-1]
        
        validated.append(token)
    
    return validated

def get_token_info(token: Any) -> str:
    """Get human-readable information about a token."""
    return f"{token['type']}: '{token['value']}' at line {token.get('line', '?')}, column {token.get('column', '?')}"

def find_token_by_position(tokens: List[Any], position: int) -> Any:
    """Find token at a specific character position."""
    for token in tokens:
        if token['start'] <= position < token['end']:
            return token
    return None

def get_tokens_in_range(tokens: List[Any], start_pos: int, end_pos: int) -> List[Any]:
    """Get all tokens within a character range."""
    return [token for token in tokens if start_pos <= token['start'] < end_pos]
