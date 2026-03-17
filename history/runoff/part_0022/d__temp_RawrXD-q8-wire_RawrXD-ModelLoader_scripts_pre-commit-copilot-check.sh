#!/bin/bash
# pre-commit-copilot-check.sh - Local pre-commit validation
# 
# Install with:
#   cp scripts/pre-commit-copilot-check.sh .git/hooks/pre-commit
#   chmod +x .git/hooks/pre-commit

echo "🔍 Validating copilot instructions..."

# Check if instructions file exists
if [ ! -f ".github/copilot-instructions.md" ]; then
    echo "❌ Error: .github/copilot-instructions.md not found"
    exit 1
fi

# Run Python validation if available
if command -v python3 &> /dev/null; then
    if [ -f "scripts/validate-copilot-instructions.py" ]; then
        python3 scripts/validate-copilot-instructions.py
        if [ $? -ne 0 ]; then
            echo ""
            echo "❌ Copilot instructions validation failed"
            echo "Fix the issues above or use: git commit --no-verify"
            exit 1
        fi
    fi
fi

# Check for common issues in staged files
STAGED_FILES=$(git diff --cached --name-only)

# If copilot instructions changed, verify they match CMakeLists
if echo "$STAGED_FILES" | grep -q "\.github/copilot-instructions.md"; then
    echo "📝 Copilot instructions modified - running detailed checks..."
    
    # Check referenced files exist
    REFERENCED=$(grep -o '`src/[^`]*\.[hpp|cpp]*`' .github/copilot-instructions.md | tr -d '`')
    for ref in $REFERENCED; do
        if [ ! -f "$ref" ]; then
            echo "❌ Error: Referenced file not found: $ref"
            exit 1
        fi
    done
    
    # Check C++ standard matches
    if grep -q "C++20" .github/copilot-instructions.md; then
        if ! grep -q "CMAKE_CXX_STANDARD 20" CMakeLists.txt; then
            echo "⚠️  Warning: Instructions specify C++20 but CMakeLists may differ"
        fi
    fi
fi

# Check for .disabled files that should be documented
DISABLED=$(find src -name "*.disabled" 2>/dev/null | wc -l)
if [ $DISABLED -gt 0 ]; then
    echo "⚠️  Found $DISABLED .disabled files - ensure they're documented in instructions if relevant"
fi

echo "✅ Pre-commit validation passed"
exit 0
