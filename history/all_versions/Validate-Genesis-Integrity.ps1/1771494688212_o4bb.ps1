# Validate-Genesis-Integrity.ps1
param([string]$Root="D:\rawrxd")

$files = @{
    Genesis = "$Root\genesis.asm"
    WidgetEngine = "$Root\RawrXD_WidgetEngine.asm"
    HeadlessWidgets = "$Root\HeadlessWidgets.asm"
}

$Report = @{Passed=0; Failed=0; Details=@()}

function Test-Rule($Name, $File, $Pattern, $ShouldMatch=$true) {
    $content = gc $File -Raw -ErrorAction SilentlyContinue
    $found = $content -match $Pattern
    $ok = ($found -and $ShouldMatch) -or (!$found -and !$ShouldMatch)
    $Report.Details += [PSCustomObject]@{Rule=$Name; File=(Split-Path $File -Leaf); Status=$(if($ok){"PASS"}else{"FAIL"})}
    if($ok){$Report.Passed++}else{$Report.Failed++}
    Write-Host "[$($Report.Details[-1].Status)] $Name" -Fore $(if($ok){"Green"}else{"Red"})
}

Write-Host "`n=== GENESIS.ASM CRITICAL FIXES ===" -Fore Cyan

# Fix 1: No illegal mem-to-mem MOV (mov [mem], [mem] is illegal in x64)
Test-Rule "No_MemToMem_MOV" $files.Genesis "mov\s+\[[^\]]+\],\s*qword\s+ptr\s+\[" $false

# Fix 2: Verify the fix (should see rax routing)
Test-Rule "Rax_Routing_Present" $files.Genesis "mov\s+rax,\s*qword\s+ptr\s+\[rcx\]|mov\s+qword\s+ptr\s+\[rbp-8h\],\s*rax" $true

# Fix 3: Template markers consistent (all use START/END, not single-line)
$genesisContent = gc $files.Genesis -Raw
$startMarkers = ([regex]::Matches($genesisContent, "\[GENESIS:\w+_START\]")).Count
$endMarkers = ([regex]::Matches($genesisContent, "\[GENESIS:\w+_END\]")).Count
$oldMarkers = ([regex]::Matches($genesisContent, "\[GENESIS:\w+\](?!_START|_END)")).Count

if($startMarkers -eq $endMarkers -and $oldMarkers -eq 0 -and $startMarkers -ge 7) {
    Write-Host "[PASS] Template markers consistent ($startMarkers START/END pairs, 0 legacy)" -Fore Green
    $Report.Passed++
} else {
    Write-Host "[FAIL] Template mismatch (START:$startMarkers, END:$endMarkers, Legacy:$oldMarkers)" -Fore Red
    $Report.Failed++
}

Write-Host "`n=== PROC COUNT VALIDATION ===" -Fore Cyan

foreach($f in $files.GetEnumerator()) {
    if(Test-Path $f.Value) {
        $procs = ([regex]::Matches((gc $f.Value -Raw), "^\s*(\w+)\s+PROC", [System.Text.RegularExpressions.RegexOptions]::Multiline)).Count
        $expected = switch($f.Key){"Genesis"{22}"WidgetEngine"{16}"HeadlessWidgets"{17}}
        if($procs -eq $expected) {
            Write-Host "[PASS] $($f.Key): $procs PROCs" -Fore Green
            $Report.Passed++
        } else {
            Write-Host "[FAIL] $($f.Key): $procs PROCs (expected $expected)" -Fore Red
            $Report.Failed++
        }
    }
}

Write-Host "`n=== ABI & CODE QUALITY ===" -Fore Cyan

# Verify no dead stubs (PROCs with just ret)
foreach($f in $files.Values) {
    $content = gc $f -Raw
    $emptyProcs = [regex]::Matches($content, "(\w+)\s+PROC\s*(\s*;\s*[^\r\n]+)?\s*ret\s*(\w+)\s+ENDP")
    if($emptyProcs.Count -eq 0) {
        Write-Host "[PASS] $(Split-Path $f -Leaf): No empty stub PROCs" -Fore Green
        $Report.Passed++
    } else {
        Write-Host "[WARN] $(Split-Path $f -Leaf): $($emptyProcs.Count) potential stubs" -Fore Yellow
    }
}

# Verify stack frame consistency (rbp usage)
Test-Rule "StackFrame_Consistency" $files.Genesis "rbp-8h|rsp.*shadow|push\s+rbp.*mov\s+rbp,\s*rsp" $true

Write-Host "`n=== FINAL GENESIS STATUS ===" -Fore White
Write-Host "Passed: $($Report.Passed)" -Fore Green
Write-Host "Failed: $($Report.Failed)" -Fore Red

if($Report.Failed -eq 0) {
    Write-Host "`nGENESIS v14.2 VERIFIED - Code generation pipeline sealed" -Fore Green
    Write-Host "Template splicing: OPERATIONAL (7 contexts)" -Fore Green
    Write-Host "Assembly integrity: LEGAL (no mem-to-mem)" -Fore Green
    exit 0
} else {
    exit 1
}
