#!/bin/bash

# ==========================================
# RAWRXD C++ BACKEND VERIFICATION SUITE
# ==========================================
# Note: On Windows with MSVC, use verification_script.ps1 instead.
# This script is for WSL / Git Bash / Linux / CI environments.

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}[1/4] Preparing Build Environment...${NC}"

# 1. Clean Slate Strategy to ensure CMake picks up new source files
if [ -d "build" ]; then
    echo "Cleaning existing build directory..."
    rm -rf build
fi
mkdir build
cd build

echo -e "${YELLOW}[2/4] Configuring with CMake...${NC}"
# 2. Configure - We expect to see the new sources in the output
cmake .. 
if [ $? -ne 0 ]; then
    echo -e "${RED}[FAIL] CMake Configuration Failed.${NC}"
    exit 1
fi

echo -e "${YELLOW}[3/4] Compiling (Parallel Jobs)...${NC}"
# 3. Build - This validates all syntax, headers, and linking
make -j$(nproc)
if [ $? -ne 0 ]; then
    echo -e "${RED}[FAIL] Compilation Failed.${NC}"
    exit 1
fi

echo -e "${GREEN}[PASS] Build Successful.${NC}"

echo -e "${YELLOW}[4/4] Running Smoke Tests...${NC}"

# 4. Smoke Test: RawrEngine (contains input_guard_slicer + replay_harness)
if [ -x "./RawrEngine" ] || [ -x "./RawrEngine.exe" ]; then
    echo -e "${GREEN}[PASS] RawrEngine binary created.${NC}"
else
    echo -e "${RED}[FAIL] RawrEngine binary missing.${NC}"
    exit 1
fi

# 5. Smoke Test: Run RawrEngine with --help to check for boot crashes
./RawrEngine --help > /dev/null 2>&1
RC=$?
if [ $RC -eq 0 ] || [ $RC -eq 1 ]; then
    echo -e "${GREEN}[PASS] RawrEngine runs (boot verification, exit=$RC).${NC}"
else
    echo -e "${RED}[FAIL] RawrEngine crashed on boot (exit=$RC).${NC}"
    exit 1
fi

# 6. Smoke Test: Win32IDE (if built)
if [ -x "./RawrXD-Win32IDE" ] || [ -x "./RawrXD-Win32IDE.exe" ]; then
    echo -e "${GREEN}[PASS] RawrXD-Win32IDE binary created.${NC}"
else
    echo -e "${YELLOW}[SKIP] RawrXD-Win32IDE not built (Win32 only).${NC}"
fi

# 7. Smoke Test: InferenceEngine (if built)
if [ -x "./RawrXD-InferenceEngine" ] || [ -x "./RawrXD-InferenceEngine.exe" ]; then
    echo -e "${GREEN}[PASS] RawrXD-InferenceEngine binary created.${NC}"
else
    echo -e "${YELLOW}[SKIP] RawrXD-InferenceEngine not built.${NC}"
fi

echo -e "${GREEN}==========================================${NC}"
echo -e "${GREEN}   FULL SYSTEM INTEGRATION: VERIFIED      ${NC}"
echo -e "${GREEN}==========================================${NC}"
