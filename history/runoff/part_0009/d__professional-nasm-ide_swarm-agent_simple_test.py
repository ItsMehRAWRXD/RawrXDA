#!/usr/bin/env python3
"""Simple test to check what Python can do"""

print("=" * 50)
print("Python Installation Test")
print("=" * 50)

import sys
print(f"Python version: {sys.version}")
print(f"Python executable: {sys.executable}")

# Test standard library imports
modules_to_test = [
    'os', 'sys', 'json', 'time', 'datetime',
    'pathlib', 'typing', 'dataclasses', 'enum',
    'asyncio', 'logging', 'multiprocessing'
]

print("\nModule availability:")
for module in modules_to_test:
    try:
        __import__(module)
        print(f"  ✓ {module}")
    except Exception as e:
        print(f"  ✗ {module} - {e}")

print("\n" + "=" * 50)
print("Test complete")
print("=" * 50)
