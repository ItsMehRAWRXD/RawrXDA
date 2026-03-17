#!/usr/bin/env python3
r"""
Instant Fix for RawrXD (D:\RawrXD) - Under 100 Lines
Full-apply mode: archives orphans, flags missing, writes comparison JSON.
"""
import os, re, glob, json, shutil, time

ROOT = r"D:\RawrXD"
CMAKE = os.path.join(ROOT, "CMakeLists.txt")
SRC   = os.path.join(ROOT, "src")
ARCH  = os.path.join(ROOT, ".archived_orphans")
KEEP  = ("main","core","engine","bridge","loader","foundation",
         "integration","init","startup","bootstrap","entry")

def cmake_sources():
    refs = set()
    with open(CMAKE, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            for m in re.finditer(r'[\w/\\.-]+\.(?:cpp|c|asm|h|hpp)\b', line, re.I):
                refs.add(os.path.basename(m.group().replace("\\", "/").strip()))
    return refs

def disk_sources():
    d = {}
    for ext in ("*.cpp","*.c","*.asm","*.h","*.hpp"):
        for p in glob.glob(os.path.join(SRC,"**",ext), recursive=True):
            d[os.path.basename(p)] = os.path.relpath(p, ROOT).replace("\\","/")
    return d

def classify():
    cb = cmake_sources(); db = disk_sources()
    active = [v for k,v in db.items() if k in cb]
    orphan = [v for k,v in db.items() if k not in cb]
    missing = [r for r in cb if r not in db]
    return active, orphan, missing

def apply_fixes(orphan, missing):
    needed = [f for f in orphan if any(k in f.lower() for k in KEEP)]
    safe   = [f for f in orphan if f not in needed]
    os.makedirs(ARCH, exist_ok=True)
    moved = 0
    for f in safe:
        src = os.path.join(ROOT, f); dst = os.path.join(ARCH, os.path.basename(f))
        if os.path.isfile(src) and not os.path.exists(dst):
            shutil.move(src, dst); moved += 1
    return needed, safe, moved

def main():
    t0 = time.time()
    active, orphan, missing = classify()
    needed, safe, moved = apply_fixes(orphan, missing)
    elapsed = round(time.time() - t0, 2)
    print(f"=== RawrXD Instant Fix (Full Apply) ===")
    print(f"Active: {len(active)} | Orphan: {len(orphan)} | Missing: {len(missing)}")
    print(f"Kept (needed): {len(needed)} | Archived: {moved}/{len(safe)}")
    for f in sorted(needed)[:15]: print(f"  KEEP:    {f}")
    for f in sorted(missing)[:10]: print(f"  MISSING: {f}")
    report = {"approach":"instant","lines_of_code":59,"elapsed_sec":elapsed,
              "active":len(active),"orphan":len(orphan),"missing":list(missing),
              "needed_orphans":needed,"archived":moved,"safe_count":len(safe)}
    with open(os.path.join(ROOT,"instant_audit.json"),"w") as f:
        json.dump(report, f, indent=2)
    print(f"Done in {elapsed}s -> instant_audit.json")

if __name__ == "__main__":
    main()
