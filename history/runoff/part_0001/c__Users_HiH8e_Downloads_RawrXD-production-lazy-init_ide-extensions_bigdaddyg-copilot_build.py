#!/usr/bin/env python3
import subprocess
import os
import shutil
import sys

src_dir = r"C:\Users\HiH8e\Downloads\RawrXD-production-lazy-init\ide-extensions\bigdaddyg-copilot"
dest_dir = r"E:\Everything\cursor\extensions\bigdaddyg-copilot-1.0.0"

print("=" * 60)
print("BigDaddyG Extension - Build & Deploy")
print("=" * 60)

# Step 1: Compile TypeScript
print("\n[1] Compiling TypeScript...")
os.chdir(src_dir)
result = subprocess.run(["npx", "tsc", "-p", "./"], capture_output=True, text=True)
if result.returncode != 0:
    print(f"Error: {result.stderr}")
    sys.exit(1)
print("✓ Compilation successful")

# Step 2: Copy to Cursor
print(f"\n[2] Copying to Cursor ({dest_dir})...")
if os.path.exists(dest_dir):
    shutil.rmtree(dest_dir)
shutil.copytree(src_dir, dest_dir)
print("✓ Files copied")

# Step 3: Generate VSIX
print("\n[3] Packaging as VSIX...")
os.chdir(dest_dir)
result = subprocess.run(
    ["npx", "-y", "@vscode/vsce", "package", "--allow-missing-repository"],
    capture_output=True,
    text=True
)
if result.returncode != 0:
    print(f"Warning: {result.stderr}")
else:
    print("✓ VSIX packaged")

# Check for VSIX
vsix_path = os.path.join(dest_dir, "bigdaddyg-copilot-1.0.0.vsix")
if os.path.exists(vsix_path):
    size = os.path.getsize(vsix_path) / 1024
    print(f"\n✓ SUCCESS!")
    print(f"  VSIX: {vsix_path}")
    print(f"  Size: {size:.1f} KB")
    print(f"\nTo install:")
    print(f'  "E:\\Everything\\cursor\\Cursor.exe" --install-extension "{vsix_path}"')
    print(f"\nThen restart Cursor and test: Ctrl+Shift+P > BigDaddyG: Open AI Chat")
else:
    print("⚠ VSIX not found")

print()
