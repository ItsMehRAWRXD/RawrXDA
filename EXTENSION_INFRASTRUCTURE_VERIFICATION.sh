#!/bin/bash
# ============================================================================
# EXTENSION_INFRASTRUCTURE_VERIFICATION.sh
# ============================================================================
# Verifies all 9 extension infrastructure systems are present and complete
# Usage: bash EXTENSION_INFRASTRUCTURE_VERIFICATION.sh
# ============================================================================

BASEDIR="d:/RawrXD"
INCLUDE_DIR="$BASEDIR/include"
SRC_DIR="$BASEDIR/src"
MANIFEST_FILE="$BASEDIR/EXTENSION_INFRASTRUCTURE_MANIFEST.md"

echo "=========================================="
echo "Extension Infrastructure Verification"
echo "=========================================="
echo ""

# System definitions
declare -a SYSTEMS=(
    "quickjs_extension_host"
    "extension_activation_events"
    "extension_manifest_parser"
    "extension_permissions"
    "marketplace_discovery_backend"
    "extension_dependency_resolver"
    "extension_auto_updater"
    "extension_configuration_ui"
    "workspace_trust_integration"
)

TOTAL_FILES=0
FOUND_FILES=0
TOTAL_LINES=0

for SYSTEM in "${SYSTEMS[@]}"; do
    echo "Checking: $SYSTEM"
    
    # Check header
    if [ -f "$INCLUDE_DIR/${SYSTEM}.h" ]; then
        LINES=$(wc -l < "$INCLUDE_DIR/${SYSTEM}.h")
        echo "  ✅ Header found: ${SYSTEM}.h ($LINES lines)"
        ((FOUND_FILES++))
        ((TOTAL_LINES += LINES))
    else
        echo "  ❌ Header missing: ${SYSTEM}.h"
    fi
    ((TOTAL_FILES++))
    
    # Check implementation
    if [ -f "$SRC_DIR/${SYSTEM}.cpp" ]; then
        LINES=$(wc -l < "$SRC_DIR/${SYSTEM}.cpp")
        echo "  ✅ Implementation found: ${SYSTEM}.cpp ($LINES lines)"
        ((FOUND_FILES++))
        ((TOTAL_LINES += LINES))
    else
        echo "  ❌ Implementation missing: ${SYSTEM}.cpp"
    fi
    ((TOTAL_FILES++))
    
    echo ""
done

# Check manifest
if [ -f "$MANIFEST_FILE" ]; then
    echo "✅ Manifest document found: EXTENSION_INFRASTRUCTURE_MANIFEST.md"
    ((FOUND_FILES++))
else
    echo "❌ Manifest document missing"
fi
((TOTAL_FILES++))

echo ""
echo "=========================================="
echo "Summary"
echo "=========================================="
echo "Files Found: $FOUND_FILES / $TOTAL_FILES"
echo "Total Lines: $TOTAL_LINES"
echo ""

if [ $FOUND_FILES -eq $TOTAL_FILES ]; then
    echo "✅ ALL SYSTEMS VERIFIED"
    exit 0
else
    echo "❌ SOME SYSTEMS MISSING"
    exit 1
fi
