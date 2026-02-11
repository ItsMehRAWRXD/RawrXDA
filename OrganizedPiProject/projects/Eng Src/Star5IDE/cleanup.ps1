# Step 5: Create a safety branch
git checkout -b security/cleanup

# Step 6: Sweep for suspicious files (already done, results in suspicious_files.txt)

# Step 7: Remove build artifacts and obvious offenders
git rm -rf --cached node_modules,x64/Debug,.vs
git rm --cached -f *.aps,*.user

# Remove files found in suspicious_files.txt
if (Test-Path "suspicious_files.txt") {
    $lines = Get-Content suspicious_files.txt
    foreach ($line in $lines) {
        $path = $line -split ':' | Select-Object -First 1
        if (Test-Path $path) {
            git rm --cached -f $path
        }
    }
}

# Step 8: Fix accidental/typo files
if (Test-Path "IDEProject/PropogationPreview.h") {
    git mv IDEProject/PropogationPreview.h IDEProject/PropagationPreview.h
}
if (Test-Path "IDEProject/PropagationPreview..h") {
    git rm -f IDEProject/PropagationPreview..h
}

# Step 9: Commit and push your changes
git add .
git commit -m "security: cleanup suspicious files and artifacts"
git push origin security/cleanup
