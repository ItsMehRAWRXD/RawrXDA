#!/bin/bash
# compare_code_length.sh — Show line counts for both approaches

echo "=== Code Length Comparison ==="
echo ""

instant=$(wc -l < instant_fix_2000.py 2>/dev/null || echo 0)
slow=$(wc -l < slow_fix_2000.py 2>/dev/null || echo 0)

echo "instant_fix_2000.py : $instant lines"
echo "slow_fix_2000.py    : $slow lines"
echo ""

if [ "$instant" -lt 100 ]; then
    echo "✓ Instant approach: Under 100 lines ($instant)"
else
    echo "✗ Instant approach: Over 100 lines ($instant)"
fi

if [ "$slow" -ge 500 ] && [ "$slow" -le 1000 ]; then
    echo "✓ Comprehensive approach: 500-1000 lines ($slow)"
else
    echo "~ Comprehensive approach: $slow lines"
fi
