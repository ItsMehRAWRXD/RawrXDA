@echo off
echo Project Organization Tool
echo ========================

echo.
echo Step 1: Analyzing projects (dry run)...
python organize_projects.py

echo.
echo Step 2: Review the plan in organization_plan.json
echo Step 3: If satisfied, run: python organize_projects.py --execute

pause