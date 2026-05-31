#!/usr/bin/env python3
"""
Remove fprintf(stderr, ...) statements from agentic_copilot_bridge.cpp
"""
import re

input_file = r'd:\rawrxd\src\agent\agentic_copilot_bridge.cpp'
output_file = r'd:\rawrxd\src\agent\agentic_copilot_bridge.cpp'

with open(input_file, 'r', encoding='utf-8') as f:
    content = f.read()

# Pattern to match fprintf(stderr, ...) statements including multi-line
# Matches: fprintf(stderr, "..." ...); with potential line continuations
pattern = r'\s*fprintf\(stderr,\s*"[^"]*"[^)]*\);\s*\n'

# Replace with empty string (removes the line)
content = re.sub(pattern, '\n', content)

# Handle multi-line fprintf statements (lines ending with \n or just continuation)
# Pattern for fprintf that spans multiple lines
multiline_pattern = r'\s*fprintf\(stderr,\s*"[^"]*"[^)]*\n[^)]*\);\s*\n'
content = re.sub(multiline_pattern, '\n', content)

# More aggressive pattern - match fprintf(stderr, ...); with any content between parens
# Using non-greedy matching
aggressive_pattern = r'\s*fprintf\(stderr,.*?\);\s*\n'
content = re.sub(aggressive_pattern, '\n', content, flags=re.DOTALL)

with open(output_file, 'w', encoding='utf-8') as f:
    f.write(content)

print(f"Removed fprintf statements from {input_file}")
