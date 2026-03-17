param(
    [string]$Root = "D:\lazy init ide",
    [string]$Dataset = ".\rawrxd_dataset"
)

# Load dataset
$topology = Get-Content "$Dataset\topology.json" | ConvertFrom-Json
$commands = Get-Content "$Dataset\commands.json" | ConvertFrom-Json
$subsystems = Get-Content "$Dataset\subsystems.json" | ConvertFrom-Json
$entrypoints = Get-Content "$Dataset\entrypoints.json" | ConvertFrom-Json
$agents = Get-Content "$Dataset\agents.json" | ConvertFrom-Json
$asm_bridges = Get-Content "$Dataset\asm_bridges.json" | ConvertFrom-Json
$random = Get-Content "$Dataset\random.json" | ConvertFrom-Json

# Validation
if ($random.Count -gt 0) {
    Write-Error "Unclassified structures detected. Cannot rebuild."
    $random | Format-Table
    exit 1
}

# Generate missing WM_PAINT if needed
if (-not $subsystems.rendering.WM_PAINT) {
    Write-Host "Injecting WM_PAINT handler..."
    $paintCode = @"
case WM_PAINT:
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT r;
    GetClientRect(hwnd, &r);
    DrawTextA(hdc, "RawrXD IDE", -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    EndPaint(hwnd, &ps);
    return 0;
}
"@

    # Find WndProc and inject
    $wndProcFile = Get-ChildItem $Root -Recurse -Include *.cpp |
        Where-Object { Select-String -Path $_.FullName -Pattern "WndProc" } |
        Select-Object -First 1

    if ($wndProcFile) {
        Add-Content $wndProcFile.FullName $paintCode
    }
}

# Generate COMMAND_TABLE if missing
if (-not $subsystems.messaging.DispatchTable) {
    Write-Host "Generating COMMAND_TABLE..."
    $tableCode = @"
struct CommandEntry {
    const char* name;
    void(*handler)();
};

void handleBootTest() { MessageBoxA(NULL, "Boot test", "RawrXD", MB_OK); }

CommandEntry COMMAND_TABLE[] = {
    {"boot_test", handleBootTest}
};
"@

    $tableCode | Out-File "$Root\generated_command_table.cpp"
}

# Build instructions
Write-Host "Ready to build. Run:"
Write-Host "ml64.exe /c *.asm"
Write-Host "cl.exe *.cpp /link /SUBSYSTEM:WINDOWS"