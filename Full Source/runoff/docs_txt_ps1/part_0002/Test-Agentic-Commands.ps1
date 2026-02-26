# 🤖 RawrXD Agentic Commands Test Guide
# This script shows you how to use agentic features in RawrXD

Write-Host @"
═══════════════════════════════════════════════════════════════
  🤖 RawrXD AGENTIC COMMANDS - HOW TO USE
═══════════════════════════════════════════════════════════════

When RawrXD opens, Agent Mode is ON by default.
Type these commands in the CHAT INPUT box (bottom of chat panel):

╔═══════════════════════════════════════════════════════════════╗
║  TERMINAL / SHELL COMMANDS                                    ║
╠═══════════════════════════════════════════════════════════════╣
║  /term ls                    - List files (via terminal)      ║
║  /term dir                   - List files (Windows style)     ║
║  /exec Get-ChildItem         - PowerShell command             ║
║  /terminal whoami            - Run any terminal command       ║
║  run command ls              - Natural language               ║
║  execute dir                 - Natural language               ║
╚═══════════════════════════════════════════════════════════════╝

╔═══════════════════════════════════════════════════════════════╗
║  FILE BROWSER COMMANDS (NEW!)                                 ║
╠═══════════════════════════════════════════════════════════════╣
║  /ls                         - List current directory         ║
║  /ls C:\Users                - List specific path             ║
║  /cd C:\temp                 - Change directory               ║
║  /pwd                        - Show current directory         ║
║  /mkdir testfolder           - Create new folder ✨           ║
║  /touch newfile.txt          - Create new file ✨             ║
║  /rm oldfile.txt             - Delete file (with confirm) ✨  ║
║  /open file.txt              - Open file in editor            ║
║  /write file.txt content     - Write content to file          ║
╚═══════════════════════════════════════════════════════════════╝

╔═══════════════════════════════════════════════════════════════╗
║  GIT COMMANDS                                                 ║
╠═══════════════════════════════════════════════════════════════╣
║  /git status                 - Git status                     ║
║  /git commit                 - Stage & commit all             ║
║  /git branch                 - List branches                  ║
║  /git log                    - Show git log                   ║
╚═══════════════════════════════════════════════════════════════╝

╔═══════════════════════════════════════════════════════════════╗
║  AI CHAT COMMANDS                                             ║
╠═══════════════════════════════════════════════════════════════╣
║  explain this code           - Ask AI about code              ║
║  analyze file.ps1            - Analyze a file                 ║
║  help me write a function    - Get AI help                    ║
║  /help                       - Show all commands              ║
╚═══════════════════════════════════════════════════════════════╝

═══════════════════════════════════════════════════════════════
  📝 QUICK TEST SEQUENCE - TRY THESE!
═══════════════════════════════════════════════════════════════

1. Launch RawrXD: .\RawrXD.ps1
2. Look for "Agent Mode: ON" button (should be green/on)
3. In chat input, type:  /pwd
   → Shows current working directory
4. Type:  /ls
   → Lists files in current directory  
5. Type:  /mkdir agent_test_folder
   → Creates a new folder!
6. Type:  /touch agent_test_file.txt
   → Creates a new empty file!
7. Type:  /ls
   → Now shows the new folder and file!
8. Type:  /term whoami
   → Runs terminal command, shows your username

═══════════════════════════════════════════════════════════════
"@ -ForegroundColor Cyan

$response = Read-Host "`nDo you want to launch RawrXD now to test? (y/n)"

if ($response -eq 'y' -or $response -eq 'Y') {
    Write-Host "`n🚀 Launching RawrXD..." -ForegroundColor Green
    Write-Host "Remember: Type commands in the CHAT INPUT box!" -ForegroundColor Yellow
    Write-Host "`nTry these commands:" -ForegroundColor Cyan
    Write-Host "  /pwd              - Show current directory" -ForegroundColor White
    Write-Host "  /ls               - List files" -ForegroundColor White
    Write-Host "  /mkdir test123    - Create folder" -ForegroundColor White  
    Write-Host "  /touch hello.txt  - Create file" -ForegroundColor White
    Write-Host "  /term whoami      - Run terminal command" -ForegroundColor White
    Write-Host ""
    
    & ".\RawrXD.ps1"
}
else {
    Write-Host "`n✅ Guide complete. Run .\RawrXD.ps1 when ready!" -ForegroundColor Green
}