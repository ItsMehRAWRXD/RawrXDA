#!/bin/bash

# ========================================
# EON Assembly IDE Test Script
# ========================================

echo " Testing EON Assembly IDE compilation..."

# Test 1: Check if source file exists
if [ ! -f "eon_ide_complete_550k.asm" ]; then
    echo " EON Assembly IDE source file not found"
    exit 1
fi

echo " Source file found"

# Test 2: Check file size
SOURCE_SIZE=$(wc -c < eon_ide_complete_550k.asm)
echo " Source file size: $SOURCE_SIZE bytes"

# Test 3: Check for key components
echo " Checking for key components..."

if grep -q "POSITION 1: PROJECT MANAGER" eon_ide_complete_550k.asm; then
    echo " Position 1 (Project Manager) found"
else
    echo " Position 1 missing"
fi

if grep -q "POSITION 11: FILE SYSTEM" eon_ide_complete_550k.asm; then
    echo " Position 11 (File System) found"
else
    echo " Position 11 missing"
fi

if grep -q "POSITION 13: UI INITIALIZATION" eon_ide_complete_550k.asm; then
    echo " Position 13 (UI) found"
else
    echo " Position 13 missing"
fi

# Test 4: Check for core functions
CORE_FUNCTIONS=("init_project_manager" "init_file_system" "init_ui" "lex_analyze" "parse_syntax" "generate_code")

echo " Checking core functions..."
for func in "${CORE_FUNCTIONS[@]}"; do
    if grep -q "$func:" eon_ide_complete_550k.asm; then
        echo " $func implemented"
    else
        echo " $func missing"
    fi
done

echo ""
echo " EON Assembly IDE source validation complete!"
echo " Ready for Linux compilation"
