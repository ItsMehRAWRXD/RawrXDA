#!/usr/bin/env python3
"""
Quick Branch Scanner - One-click analysis of all branches
Run this anytime to see what's new or missing from your local setup
"""

import subprocess
import sys
from pathlib import Path

def quick_scan():
    """Quick scan of branches vs local setup"""
    print("🔍 RawrZ Quick Branch Scanner")
    print("=" * 40)
    
    # Check if temp-repo exists
    if not Path("../temp-repo").exists():
        print("❌ temp-repo not found. Run git clone first.")
        return
    
    # Run the full analysis
    print("📊 Running full branch analysis...")
    try:
        result = subprocess.run([sys.executable, "branch-feature-extractor.py"], 
                              capture_output=True, text=True)
        if result.returncode == 0:
            print("✅ Branch analysis complete!")
            
            # Run duplicate analysis
            print("\n🔄 Analyzing duplicates...")
            dup_result = subprocess.run([sys.executable, "duplicate-analyzer.py"], 
                                      capture_output=True, text=True)
            if dup_result.returncode == 0:
                print("✅ Duplicate analysis complete!")
                print("\n📋 Check branch-analysis-report.json for full details")
            else:
                print("❌ Duplicate analysis failed")
        else:
            print("❌ Branch analysis failed")
            print(result.stderr)
    except Exception as e:
        print(f"❌ Error: {e}")

def show_summary():
    """Show quick summary from last scan"""
    report_file = Path("branch-analysis-report.json")
    if not report_file.exists():
        print("❌ No analysis report found. Run quick_scan() first.")
        return
    
    import json
    with open(report_file, 'r') as f:
        report = json.load(f)
    
    summary = report['summary']
    print(f"\n📈 Last Scan Summary:")
    print(f"   Branches scanned: {summary['total_branches_scanned']}")
    print(f"   Branches with features: {summary['branches_with_missing_features']}")
    print(f"   Local engines: {summary['total_local_engines']}")
    print(f"   Extraction recommendations: {len(report['extraction_recommendations'])}")

if __name__ == "__main__":
    quick_scan()
    show_summary()
