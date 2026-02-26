$file = "D:\rawrxd\gui\ide_chatbot.html"
$c = [IO.File]::ReadAllText($file)
$lines = [IO.File]::ReadAllLines($file)
$out = @()

$out += "=== AUDIT OF ide_chatbot.html ==="
$out += "Total lines: $($lines.Count)"
$out += ""

# 1. Script tag balance
$openScript = [regex]::Matches($c, '<script[\s>]').Count
$closeScript = [regex]::Matches($c, '</script>').Count
$out += "=== 1. SCRIPT TAG BALANCE ==="
$out += "Opening <script: $openScript"
$out += "Closing </script>: $closeScript"
$out += "Match: $($openScript -eq $closeScript)"
# Show locations
for ($i=0; $i -lt $lines.Count; $i++) {
  if ($lines[$i] -match '<script[\s>]') { $out += "  OPEN  line $($i+1): $($lines[$i].Trim().Substring(0,[Math]::Min(90,$lines[$i].Trim().Length)))" }
  if ($lines[$i] -match '</script>') { $out += "  CLOSE line $($i+1): $($lines[$i].Trim().Substring(0,[Math]::Min(90,$lines[$i].Trim().Length)))" }
}
$out += ""

# 2. HTML structure
$out += "=== 2. HTML STRUCTURE ==="
foreach ($tag in @("html","head","body","style")) {
  $op = [regex]::Matches($c, "<$tag[\s>]").Count
  $cl = [regex]::Matches($c, "</$tag>").Count
  $out += "$tag : open=$op close=$cl match=$($op -eq $cl)"
}
$out += ""

# 3. Duplicate functions
$funcMatches = [regex]::Matches($c, 'function\s+(\w+)\s*\(')
$funcMap = @{}
for ($i=0; $i -lt $lines.Count; $i++) {
  if ($lines[$i] -match 'function\s+(\w+)\s*\(') {
    $name = $Matches[1]
    if (-not $funcMap.ContainsKey($name)) { $funcMap[$name] = @() }
    $funcMap[$name] += ($i+1)
  }
}
$out += "=== 3. DUPLICATE FUNCTIONS ==="
$out += "Total unique function names: $($funcMap.Count)"
$dupCount = 0
$funcMap.GetEnumerator() | Sort-Object Key | ForEach-Object {
  if ($_.Value.Count -gt 1) {
    $dupCount++
    $out += "  DUP: $($_.Value.Count)x $($_.Key) at lines $($_.Value -join ', ')"
  }
}
if ($dupCount -eq 0) { $out += "  No duplicates found" }
$out += ""

# 4. Dead onclick references
$out += "=== 4. DEAD ONCLICK REFERENCES ==="
$onclickNames = [regex]::Matches($c, 'onclick="(\w+)\(') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique
$funcNames = $funcMap.Keys
$deadCount = 0
foreach ($oc in $onclickNames) {
  if ($oc -notin $funcNames) {
    # Also check if it's a global/window method or property
    $out += "  DEAD: $oc (onclick references this but no 'function $oc(' found)"
    $deadCount++
  }
}
if ($deadCount -eq 0) { $out += "  All onclick handlers have matching function definitions" }
$out += "  Total onclick handlers: $($onclickNames.Count)"
$out += ""

# 5. Backend-Switcher
$out += "=== 5. BACKEND-SWITCHER PANEL ==="
$hasShowBackendSwitcher = $c -match 'function\s+showBackendSwitcher\s*\('
$out += "showBackendSwitcher function: $hasShowBackendSwitcher"
$hasBackendSwitcherDiv = $c -match 'id="[^"]*backend-switcher[^"]*"'
$out += "backend-switcher div: $hasBackendSwitcherDiv"
if ($hasBackendSwitcherDiv) { $out += "  ID match: $($Matches[0])" }
$out += ""

# 6. Router panel
$out += "=== 6. ROUTER PANEL ==="
$hasShowRouter = $c -match 'function\s+showRouterPanel\s*\('
$out += "showRouterPanel function: $hasShowRouter"
$hasRouterDiv = [regex]::Matches($c, 'id="[^"]*router[^"]*"') | ForEach-Object { $_.Value }
$out += "Router divs: $($hasRouterDiv -join '; ')"
$out += ""

# 7. Swarm panel
$out += "=== 7. SWARM PANEL ==="
$hasShowSwarm = $c -match 'function\s+showSwarmPanel\s*\('
$out += "showSwarmPanel function: $hasShowSwarm"
$hasSwarmDiv = [regex]::Matches($c, 'id="[^"]*swarm[^"]*"') | ForEach-Object { $_.Value }
$out += "Swarm divs: $($hasSwarmDiv -join '; ')"
$out += ""

# 8. Safety dashboard
$out += "=== 8. SAFETY DASHBOARD ==="
$hasSafetyFunc = [regex]::Matches($c, 'function\s+\w*[Ss]afety\w*\s*\(') | ForEach-Object { $_.Value }
$out += "Safety functions: $($hasSafetyFunc -join '; ')"
$hasSafetyDiv = [regex]::Matches($c, 'id="[^"]*safety[^"]*"') | ForEach-Object { $_.Value }
$out += "Safety divs: $($hasSafetyDiv -join '; ')"
$out += ""

# 9. ASM debugger
$out += "=== 9. ASM DEBUGGER ==="
$hasAsmDebugger = [regex]::Matches($c, 'function\s+\w*[Aa]sm[Dd]ebug\w*\s*\(') | ForEach-Object { $_.Value }
$out += "ASM debugger functions: $($hasAsmDebugger -join '; ')"
$hasShowAsm = $c -match 'showAsmDebugger'
$out += "showAsmDebugger reference exists: $hasShowAsm"
$out += ""

# 10. Standalone titlebar
$out += "=== 10. STANDALONE TITLEBAR ==="
$hasTitlebarDiv = $c -match 'id="standaloneTitlebar"'
$out += "div#standaloneTitlebar exists: $hasTitlebarDiv"
$hasTitlebarCSS = $c -match '\.standalone-titlebar'
$out += "CSS .standalone-titlebar exists: $hasTitlebarCSS"
$hasTitlebarJS = [regex]::Matches($c, 'standaloneTitlebar') | Measure-Object | Select-Object -ExpandProperty Count
$out += "JS references to standaloneTitlebar: $hasTitlebarJS"
$out += ""

# 11. connectBackend
$out += "=== 11. CONNECT BACKEND ==="
$hasConnectBackend = $c -match 'function\s+connectBackend\s*\('
$out += "connectBackend function: $hasConnectBackend"
$hasServePy = $c -match 'serve\.py'
$out += "serve.py reference: $hasServePy"
$hasOllama = $c -match 'ollama'
$out += "Ollama reference: $hasOllama"
$hasOffline = $c -match 'offline'
$out += "Offline reference: $hasOffline"
# Find the line range for connectBackend
for ($i=0; $i -lt $lines.Count; $i++) {
  if ($lines[$i] -match 'function\s+connectBackend\s*\(') {
    $out += "  Found at line $($i+1)"
  }
}
$out += ""

# 12. CSP meta tag
$out += "=== 12. CONTENT-SECURITY-POLICY ==="
$cspMatches = [regex]::Matches($c, '<meta[^>]*Content-Security-Policy[^>]*>')
$out += "CSP meta tags found: $($cspMatches.Count)"
foreach ($m in $cspMatches) { $out += "  $($m.Value.Substring(0,[Math]::Min(200,$m.Value.Length)))" }
$hasFileProto = $c -match "file://"
$out += "file:// in CSP or document: $hasFileProto"
$hasLocalhost = $c -match 'localhost'
$out += "localhost reference: $hasLocalhost"

$out | Out-File "D:\rawrxd\audit_result.txt" -Encoding UTF8
Write-Output "Audit complete. Results in D:\rawrxd\audit_result.txt"
