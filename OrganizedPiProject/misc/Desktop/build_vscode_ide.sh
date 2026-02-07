#!/bin/bash

echo "Building VSCode-like Secure IDE..."

# Compile the assembly code
nasm -f elf64 vscode_secure_ide.asm -o vscode_secure_ide.o

# Link the object file
ld vscode_secure_ide.o -o vscode_secure_ide

# Make executable
chmod +x vscode_secure_ide

echo "Build complete! Run with: ./vscode_secure_ide"
echo ""
echo "Features:"
echo "- VSCode-like interface with sidebar and editor"
echo "- F1: Command palette with AI chat options"
echo "- Browser integration for ChatGPT, Claude, Kimi, Gemini"
echo "- Local AI processing for privacy"
echo "- Ctrl+N: New file"
echo "- Ctrl+O: Open file"
echo "- Ctrl+S: Save file"
echo "- Ctrl+`: Toggle terminal"
echo "- All processing stays local and secure"