#!/usr/bin/env python3
"""
Duplicate Feature Analyzer
Analyzes the branch report to identify duplicate features across branches
"""

import json
from collections import defaultdict, Counter
from typing import Dict, List, Set

def analyze_duplicates():
    """Analyze duplicates in the branch report"""
    
    # Load the analysis report
    with open('branch-analysis-report.json', 'r') as f:
        report = json.load(f)
    
    # Track all features by type
    all_features = defaultdict(set)
    feature_branches = defaultdict(list)
    
    print("[DUPLICATE ANALYSIS] Analyzing feature overlaps across branches")
    print("=" * 70)
    
    # Collect all features
    for branch, features in report['missing_features'].items():
        for feature_type, feature_list in features.items():
            for feature in feature_list:
                all_features[feature_type].add(feature)
                feature_branches[feature].append(branch)
    
    # Analyze duplicates
    duplicates = {}
    unique_features = {}
    
    for feature_type, features in all_features.items():
        duplicates[feature_type] = {}
        unique_features[feature_type] = set()
        
        for feature in features:
            branches = feature_branches[feature]
            if len(branches) > 1:
                duplicates[feature_type][feature] = branches
            else:
                unique_features[feature_type].add(feature)
    
    # Display results
    total_duplicates = 0
    total_unique = 0
    
    for feature_type in sorted(duplicates.keys()):
        dup_count = len(duplicates[feature_type])
        unique_count = len(unique_features[feature_type])
        total_duplicates += dup_count
        total_unique += unique_count
        
        print(f"\n[{feature_type.upper()}]")
        print(f"  Duplicates: {dup_count}")
        print(f"  Unique: {unique_count}")
        print(f"  Total: {dup_count + unique_count}")
        
        if dup_count > 0:
            print(f"  Most duplicated features:")
            # Sort by number of branches
            sorted_dups = sorted(duplicates[feature_type].items(), 
                               key=lambda x: len(x[1]), reverse=True)
            
            for feature, branches in sorted_dups[:5]:  # Top 5
                print(f"    '{feature}' appears in {len(branches)} branches:")
                for branch in branches[:3]:  # Show first 3 branches
                    print(f"      - {branch}")
                if len(branches) > 3:
                    print(f"      ... and {len(branches) - 3} more")
    
    print(f"\n[SUMMARY]")
    print("=" * 70)
    print(f"Total duplicate features: {total_duplicates}")
    print(f"Total unique features: {total_unique}")
    print(f"Total features analyzed: {total_duplicates + total_unique}")
    print(f"Duplicate percentage: {(total_duplicates / (total_duplicates + total_unique) * 100):.1f}%")
    
    # Find the most common duplicates
    print(f"\n[TOP DUPLICATES ACROSS ALL TYPES]")
    print("-" * 50)
    
    all_duplicates = []
    for feature_type, dups in duplicates.items():
        for feature, branches in dups.items():
            all_duplicates.append((feature, len(branches), feature_type))
    
    # Sort by number of occurrences
    all_duplicates.sort(key=lambda x: x[1], reverse=True)
    
    for i, (feature, count, feature_type) in enumerate(all_duplicates[:10], 1):
        print(f"{i:2d}. {feature} ({feature_type}) - appears in {count} branches")
    
    # Branch overlap analysis
    print(f"\n[BRANCH OVERLAP ANALYSIS]")
    print("-" * 50)
    
    branch_overlap = defaultdict(int)
    for feature, branches in feature_branches.items():
        if len(branches) > 1:
            # Count pairwise overlaps
            for i in range(len(branches)):
                for j in range(i + 1, len(branches)):
                    pair = tuple(sorted([branches[i], branches[j]]))
                    branch_overlap[pair] += 1
    
    # Show most overlapping branch pairs
    sorted_overlaps = sorted(branch_overlap.items(), key=lambda x: x[1], reverse=True)
    print("Most overlapping branch pairs:")
    for i, ((branch1, branch2), count) in enumerate(sorted_overlaps[:10], 1):
        print(f"{i:2d}. {branch1} <-> {branch2}: {count} shared features")
    
    return {
        'duplicates': duplicates,
        'unique': unique_features,
        'total_duplicates': total_duplicates,
        'total_unique': total_unique,
        'branch_overlaps': dict(sorted_overlaps)
    }

if __name__ == "__main__":
    analyze_duplicates()
