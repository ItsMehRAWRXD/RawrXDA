# 🔄 PHASE 3 GIT WORKFLOW GUIDE

**Project**: Mirai Security Toolkit - Phase 3  
**For**: All developers on BotBuilder & Beast Swarm tasks  
**Date**: November 21, 2025

---

## 🎯 QUICK START

### First Time Setup (Do this FIRST)
```bash
# Navigate to project directory
cd c:\Users\[YourName]\OneDrive\Desktop\Mirai-Source-Code-master

# Verify you're on main/master branch
git status

# Create your feature branch
git checkout -b phase3-botbuilder-gui        # If Task 2 (C#)
# OR
git checkout -b phase3-beast-optimization    # If Task 3 (Python)

# Verify you're on new branch
git branch
# Should show: * phase3-[your-task-name]

# You're ready to start coding!
```

---

## 📝 DAILY WORKFLOW

### Beginning of Day
```bash
# 1. Make sure you're on your feature branch
git status
# Should show: On branch phase3-[your-task-name]

# 2. Pull any updates
git pull origin phase3-[your-task-name]

# 3. Start coding!
```

### End of Day (IMPORTANT: Do this daily)
```bash
# 1. Check what you changed
git status

# 2. Stage all changes
git add .

# 3. Commit with descriptive message
git commit -m "Phase 3 Task [2|3]: [What you accomplished today]

- Item 1: What works
- Item 2: What's done
- Item 3: Next step"

# 4. Push to origin
git push origin phase3-[your-task-name]

# 5. Verify push succeeded
git log --oneline -5
# Should show your latest commit
```

---

## 📌 COMMIT MESSAGE EXAMPLES

### TASK 2 (BotBuilder) Examples

**Day 1 - Configuration Tab:**
```bash
git commit -m "Phase 3 Task 2: Configuration Tab - Core UI Complete

- Main window XAML structure created
- Bot name textbox functional
- C2 server/port inputs working
- Architecture combobox (x86/x64) implemented
- All controls binding to model
- Next: Advanced Tab"
```

**Day 2 - Advanced Tab:**
```bash
git commit -m "Phase 3 Task 2: Advanced Tab - Security Options

- Anti-VM detection checkboxes added
- Anti-debugging options functional
- Persistence methods (registry, COM, WMI, task) implemented
- Network protocol selection working
- All event handlers connected
- Next: Build Tab"
```

**Day 3 - Build Tab:**
```bash
git commit -m "Phase 3 Task 2: Build Tab - Compilation & Progress

- Compression options (None/Zlib/LZMA/UPX) implemented
- Encryption selection (AES-256/AES-128/XOR) working
- Build button triggers compilation process
- Progress bar updates during build
- Status messages display correctly
- Next: Preview Tab & QA"
```

**Day 4 - Preview Tab & Final:**
```bash
git commit -m "Phase 3 Task 2: BotBuilder GUI COMPLETE ✅

- Preview Tab shows estimated payload size
- SHA256 hash calculated and displayed
- Evasion score computed
- Export button functional
- All 4 tabs tested and working
- Code compiles with 0 errors, 3 warnings
- Application ready for integration testing"
```

### TASK 3 (Beast Swarm) Examples

**Day 1 - Baseline Profiling:**
```bash
git commit -m "Phase 3 Task 3: Performance Baseline Established

- Current memory usage: [X MB] baseline
- Current CPU usage: [X%] baseline
- Current throughput: [X requests/sec]
- Profiling environment set up
- cProfile configured for monitoring
- Next: Phase 1 Memory/CPU optimization"
```

**Day 2 - Phase 1 Optimization:**
```bash
git commit -m "Phase 3 Task 3: Phase 1 - Memory/CPU Optimization (50%)

- Memory pooling implemented
- Batch processing optimization added
- Connection reuse configured
- Cache optimization in progress
- Baseline: [X MB] → Current: [Y MB] (-15% ✓)
- CPU improvement: [X%] → [Y%] (-20% progress)
- Next: Complete cache, move to Phase 2"
```

**Day 3 - Phase 2 & 3:**
```bash
git commit -m "Phase 3 Task 3: Phase 2 Error Handling + Phase 3 Deployment (70%)

- BeastSwarmError exception hierarchy complete
- All critical sections wrapped with error handling
- Logging configured for all modules
- Deployment scripts created (deploy.sh, verify, rollback)
- Health-check scripts functional
- Next: Phase 4 Unit Tests"
```

**Day 5 - Final Phases:**
```bash
git commit -m "Phase 3 Task 3: Beast Swarm PRODUCTION-READY ✅

- Phase 1: Memory -15.2% ✓, CPU -21.5% ✓
- Phase 2: All error handling complete
- Phase 3: Deployment scripts tested
- Phase 4: Unit tests 100% passing (80%+ coverage)
- Phase 5: Integration tests 100% passing
- Phase 6: Performance targets verified
- Ready for production deployment"
```

---

## 🚀 COMMIT FREQUENCY

**Minimum Commitment**: At least **1 commit per day**

**Recommended**:
- **Morning**: Review previous day's code, fix issues
- **Midday**: Commit completed section (after 2 hours of work)
- **End of Day**: Commit daily progress (required)

This ensures:
- ✅ Progress is tracked
- ✅ Work is backed up
- ✅ Team can see what's done
- ✅ Easy rollback if needed

---

## ⚠️ COMMON MISTAKES TO AVOID

### ❌ DON'T: Commit without message
```bash
# BAD
git commit -m "updates"

# GOOD
git commit -m "Phase 3 Task 2: Configuration Tab - Form inputs working"
```

### ❌ DON'T: Commit on wrong branch
```bash
# Check first!
git branch
# Should show: * phase3-[your-task-name]
```

### ❌ DON'T: Forget to push
```bash
# Always push after commit
git add .
git commit -m "..."
git push origin phase3-[your-task-name]  # DON'T FORGET THIS!
```

### ❌ DON'T: Commit to main/master
```bash
# If you accidentally did:
git reset --soft HEAD~1
git checkout -b phase3-[task-name]
git commit -m "..."
git push origin phase3-[task-name]
```

### ✅ DO: Use descriptive messages
```bash
# Clearly explain what changed
git commit -m "Phase 3 Task 2: Configuration Tab - All inputs working

- Bot name field takes input
- C2 server/port fields functional
- Architecture selection added
- Output format options working
- Data binding complete
- Next: Advanced Tab"
```

### ✅ DO: Commit regularly
```bash
# Multiple small commits better than one giant commit
# Commit every 2 hours or when completing a feature section
git add .
git commit -m "Phase 3 Task 2: Configuration Tab - [Specific section done]"
git push origin phase3-[task-name]
```

---

## 🔍 CHECKING YOUR PROGRESS

### See all your commits
```bash
# View your commit history
git log --oneline

# Should show your daily commits:
abc1234 Phase 3 Task 2: BotBuilder GUI COMPLETE ✅
def5678 Phase 3 Task 2: Preview Tab - Complete
ghi9012 Phase 3 Task 2: Build Tab - Compilation logic
jkl3456 Phase 3 Task 2: Advanced Tab - Security options
mno7890 Phase 3 Task 2: Configuration Tab - Core UI
```

### Check current status
```bash
git status
# Should show either:
# "nothing to commit, working tree clean"  ← All committed
# OR
# "Untracked files:" or "Changes not staged:" ← You have work to commit
```

### See what you changed
```bash
git diff                    # See all changes not staged
git diff --staged           # See changes staged for commit
git diff HEAD~1             # See changes in last commit
```

---

## 📊 PROJECT LEAD VIEW

As project lead, use these commands to track progress:

```bash
# See all commits on task branches
git log --oneline phase3-botbuilder-gui
git log --oneline phase3-beast-optimization

# See commits since yesterday
git log --since="24 hours ago" --oneline

# See who committed what
git log --author="[Developer Name]" --oneline

# See commits by task
git log --grep="Phase 3 Task" --oneline

# Compare progress between branches
git log --oneline phase3-botbuilder-gui
git log --oneline phase3-beast-optimization
```

---

## 🎯 SUCCESS CHECKLIST

Before completing your task, ensure:

```
Git Workflow:
□ Working on correct feature branch (phase3-[task-name])
□ Committing at least once per day
□ All commits have descriptive messages
□ All changes pushed to origin
□ No uncommitted changes on completion day

Commit Quality:
□ Messages are descriptive (20+ characters)
□ Each commit represents logical unit of work
□ No "WIP" or "test" commits in final version
□ Final commit clearly indicates completion

Code Quality:
□ Code compiles cleanly
□ No unhandled exceptions
□ Tests passing
□ Changes are meaningful (not just whitespace)
```

---

## 🚨 IF SOMETHING GOES WRONG

### "I committed to wrong branch"
```bash
# Don't panic! Fix it:
git log --oneline -5          # Note your commit hash
git reset --soft HEAD~1       # Undo last commit
git checkout -b phase3-[correct-branch]
git commit -m "..."           # Recommit on correct branch
```

### "I need to undo last commit"
```bash
# If not pushed yet:
git reset --soft HEAD~1       # Undo but keep changes
git reset --hard HEAD~1       # Undo and lose changes (be careful!)

# If already pushed (ask project lead first!):
git revert HEAD                # Create new commit that undoes it
```

### "I have uncommitted changes and need to switch branches"
```bash
git stash                     # Save work temporarily
git checkout -b phase3-[new-branch]
git stash pop                 # Restore work
```

### "I forgot to commit something"
```bash
# Add forgotten files
git add [forgotten-files]
git commit --amend            # Add to last commit (if not pushed)
# OR
git add [forgotten-files]
git commit -m "Fix: Add forgotten files"  # New commit (if already pushed)
```

---

## 📞 NEED HELP?

**Git Issue** | **Command to Check** | **Contact**
---|---|---
"What branch am I on?" | `git branch` | Check yourself
"Did my commit push?" | `git log --oneline` | Check yourself
"What did I change?" | `git diff` | Check yourself
"I broke something" | `git log` then ask | Project lead
"Merge conflict" | `git status` | Project lead
"Lost changes" | `git reflog` | Project lead

---

## ✅ FINAL REMINDERS

1. **ALWAYS** create feature branch first (don't commit to main)
2. **ALWAYS** write descriptive commit messages
3. **ALWAYS** push at end of day (don't lose work)
4. **ALWAYS** commit at least once daily
5. **ALWAYS** check `git status` before committing

---

## 🎉 YOU'RE SET!

This git workflow will keep your work organized and safe.

**Summary**:
- ✅ Create feature branch (phase3-[task-name])
- ✅ Commit daily with clear messages
- ✅ Push to origin at end of day
- ✅ Let project lead track progress via git log

**Questions?** Ask project lead or check your task assignment.

**Ready to commit?** 🚀

---

*Phase 3 Git Workflow Guide*  
*All Developers - Read This!*  
*Updated: November 21, 2025*
