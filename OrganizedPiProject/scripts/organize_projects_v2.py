#!/usr/bin/env python3
import os
import shutil
from pathlib import Path
from collections import defaultdict
import json
import re

def analyze_projects():
    desktop_path = Path("C:/Users/Garre/Desktop")
    projects = defaultdict(list)
    
    # Scan both Desktop root and Desktop/Desktop
    search_paths = [desktop_path, desktop_path / "Desktop"]
    
    for search_path in search_paths:
        if not search_path.exists():
            continue
            
        for item in search_path.iterdir():
            if not item.is_dir() or item.name.startswith('.'):
                continue
                
            name = item.name.lower()
            
            # Skip system folders
            if name in ['organized-projects', 'archived-projects']:
                continue
            
            # Group similar projects
            base_name = name
            
            # Remove suffixes
            for suffix in ['-backup', '_backup', '-copy', '-old', '-v2', '-clean', '-final']:
                base_name = base_name.replace(suffix, '')
            
            # Remove dates/versions
            base_name = re.sub(r'-?\d{4}-\d{2}-\d{2}', '', base_name)
            base_name = re.sub(r'-?v?\d+\.\d+\.\d+', '', base_name)
            
            # Group by type
            if 'rawrz' in base_name:
                projects['rawrz-projects'].append(item)
            elif any(x in base_name for x in ['ide', 'editor']):
                projects['ide-projects'].append(item)
            elif 'secure' in base_name:
                projects['secure-projects'].append(item)
            else:
                projects[base_name].append(item)
    
    return projects

def create_plan():
    projects = analyze_projects()
    plan = {"duplicates": {}, "actions": []}
    
    for group, folders in projects.items():
        if len(folders) > 1:
            plan["duplicates"][group] = [str(f) for f in folders]
            latest = max(folders, key=lambda x: x.stat().st_mtime)
            plan["actions"].append({
                "group": group,
                "keep": str(latest),
                "archive": [str(f) for f in folders if f != latest]
            })
    
    return plan

def execute_plan(dry_run=True):
    plan = create_plan()
    desktop_path = Path("C:/Users/Garre/Desktop")
    archive_path = desktop_path / "archived-projects"
    
    print(f"{'DRY RUN' if dry_run else 'EXECUTING'} - Project Organization")
    print("=" * 60)
    
    if not plan["actions"]:
        print("No duplicate projects found to organize.")
        return
    
    for action in plan["actions"]:
        print(f"\nGroup: {action['group']}")
        print(f"KEEP: {Path(action['keep']).name}")
        print("ARCHIVE:")
        
        for item in action["archive"]:
            print(f"  -> {Path(item).name}")
            
            if not dry_run:
                archive_path.mkdir(exist_ok=True)
                src = Path(item)
                dst = archive_path / src.name
                if src.exists():
                    shutil.move(str(src), str(dst))
                    print(f"     Moved to archived-projects/")
    
    # Save plan
    with open(desktop_path / "organization_plan.json", "w") as f:
        json.dump(plan, f, indent=2)
    
    print(f"\nFound {len(plan['duplicates'])} groups with duplicates")
    print(f"Plan saved to organization_plan.json")

if __name__ == "__main__":
    import sys
    dry_run = "--execute" not in sys.argv
    execute_plan(dry_run)