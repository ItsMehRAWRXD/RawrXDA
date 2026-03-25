# RawrXD Startup Error Summary

## Date: 2025-11-24

### Critical Issue: Script Cannot Start Due to Parse Errors

The script `RawrXD.ps1` is failing to start due to **duplicate key errors** in hash literals within the `Get-LanguageProjectTemplate` function.

### Error Details

**Error Type:** Parse Error - Duplicate keys in hash literals

**Affected Lines:**
- Line 11762: Duplicate key `"README.md"` 
- Line 11785: Duplicate key `"package.json"`
- Line 11814: Duplicate key `"README.md"`
- Line 11847: Duplicate key `"README.md"`
- Line 11879: Duplicate key `"README.md"`
- Line 11904: Duplicate key `"README.md"`
- Line 11942: Duplicate key `"README.md"`
- Line 11984: Duplicate key `"README.md"`

### Root Cause

PowerShell is detecting duplicate keys in hash table definitions. Each language template in `Get-LanguageProjectTemplate` has its own `if` block that should return independently, but PowerShell's parser is somehow seeing them as part of a single hash literal.

### Impact

- **Script cannot start** - Parse errors prevent execution
- **No error logs created** - Script fails before logging system initializes
- **GUI never loads** - Application cannot launch

### Previous Runtime Errors (from DevLog)

From `RawrXD_DevLog_20251124_180949.log`:
1. **Settings Error:** `Set-EditorSettings` cmdlet not recognized (Line 8)
2. **Theme Error:** Variable `$leftPanel` not set (Line 19)
3. **UI Optimization Warning:** `SetStyle` method not found on Form (Line 25)

### Log File Locations

**Expected Locations:**
- `%APPDATA%\RawrXD\Logs\ERRORS.log` - General errors
- `%APPDATA%\RawrXD\Logs\CRITICAL_ERRORS.log` - Critical errors
- `%APPDATA%\RawrXD\Logs\startup_YYYY-MM-DD.log` - Daily startup logs

**Current Status:** Log directories do not exist (script fails before creating them)

**Fallback Location:**
- `%TEMP%\RawrXD_Logs\` - Not found

**DevLog Files Found:**
- `RawrXD_DevLog_20251124_180949.log` - Last successful run (6:09 PM)
- `RawrXD_DevLog_20251124_175405.log` - Previous run (5:54 PM)

### Recommended Fix

1. **Immediate:** Fix duplicate key errors in `Get-LanguageProjectTemplate` function
2. **Verify:** Ensure each template's `if` block properly returns its own hash table
3. **Test:** Run script directly to verify parse errors are resolved
4. **Monitor:** Check log files after successful startup

### Next Steps

1. Fix the parse errors preventing script execution
2. Test script startup
3. Verify error logging system creates log files
4. Review any new errors in log files

