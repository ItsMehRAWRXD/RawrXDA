"""Check that the active Python installation has the modules we rely on."""

import sys
import importlib
import importlib.util
from shutil import which

REQUIRED_MODULES = [
    "asyncio",
    "logging",
    "typing",
    "dataclasses",
    "enum",
    "multiprocessing",
]
MIN_VERSION = (3, 8)

print("Checking Python environment...")
print(f"Detected Python: {sys.executable}")
print(f"Version: {sys.version}")

issues = []

if sys.version_info < MIN_VERSION:
    issues.append(f"Python {MIN_VERSION[0]}.{MIN_VERSION[1]}+ is required.")

for module in REQUIRED_MODULES:
    if importlib.util.find_spec(module) is None:
        issues.append(f"Missing core module: {module}")

launcher_present = which("py") or which("python")
if launcher_present is None:
    issues.append("Python launcher missing (py or python executable not in PATH)")

if issues:
    print("\n[ERROR] Python environment is not usable:")
    for issue in issues:
        print(f"  - {issue}")
    print("\nPlease install a standard Python 3.8+ from https://python.org and ensure it is in PATH.")
    print("Make sure the installation includes the standard library (asyncio, logging, typing, dataclasses, etc.)")
    sys.exit(1)

print("\n[OK] Python environment looks good.")
