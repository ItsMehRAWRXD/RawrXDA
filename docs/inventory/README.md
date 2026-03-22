# Code path inventory (machine export)

| File | Description |
|------|-------------|
| **`ide_inventory_paths.txt`** | One repo-relative path per line (`/` separators), normalized from a Windows absolute export. |

**Regenerate from your machine** (PowerShell, repo root = `D:\rawrxd`):

```powershell
Get-Content D:\ide_inventory_report\ide_inventory_in.txt -Encoding UTF8 |
  ForEach-Object {
    if ($_ -match '^\s*$') { return }
    ($_ -replace '^D:\\rawrxd\\','') -replace '\\','/'
  } |
  Set-Content .\docs\inventory\ide_inventory_paths.txt -Encoding UTF8
```

See **`docs/IDE_CODEBASE_INVENTORY.md`** for counts and how this relates to the Win32 IDE vs vendored trees.
