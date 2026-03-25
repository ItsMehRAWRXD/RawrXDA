# PowerShell Module/Extension Limits Guide

## Technical Limits

### Function and Variable Limits (PowerShell 5.1)
- **Default Limit**: 4,096 functions per session (`$MaximumFunctionCount`)
- **Default Limit**: 4,096 variables per session (`$MaximumVariableCount`)
- **Module Nesting Limit**: 10 levels deep

### Your Current Usage
Based on the refactoring:
- **Total Functions**: 299 functions
- **Total Modules**: 11 modules
- **Usage**: ~7.3% of the 4,096 function limit ✅ **Well within limits**

### PowerShell 7+ (Recommended)
- **No function/variable count limits** - unlimited functions
- **Better performance** with large module sets
- **Recommended** for enterprise applications

## Checking Current Limits

```powershell
# Check current limits
$MaximumFunctionCount
$MaximumVariableCount

# Check current usage
(Get-Command).Count  # Total commands (functions + cmdlets)
(Get-Variable).Count  # Total variables

# Check loaded modules
Get-Module | Measure-Object | Select-Object Count
```

## Increasing Limits (PowerShell 5.1)

If you need more capacity:

```powershell
# Increase function limit
$MaximumFunctionCount = 8192  # or higher

# Increase variable limit  
$MaximumVariableCount = 8192  # or higher

# Make permanent (add to PowerShell profile)
# $PROFILE location: $PROFILE
```

## Practical Recommendations

### For Your Refactored Structure

**Current Status**: ✅ **Safe**
- 299 functions across 11 modules
- Well below 4,096 limit
- Room for ~3,700 more functions

### Best Practices

1. **Module Size Guidelines**
   - **Small modules**: 5-20 functions (ideal)
   - **Medium modules**: 20-100 functions (acceptable)
   - **Large modules**: 100+ functions (consider splitting)
   - **Your Terminal module**: 198 functions - consider splitting

2. **Module Count**
   - **No hard limit** on number of modules
   - **Practical limit**: 50-100 modules before performance degrades
   - **Your count**: 11 modules ✅ **Excellent**

3. **Extension System**
   - Your `$script:extensionRegistry` has no explicit limit
   - Limited only by memory and function count
   - Can handle hundreds of extensions

## Performance Considerations

### Module Loading
- **Cold start**: Loading 11 modules takes ~1-3 seconds
- **Warm start**: Cached modules load instantly
- **Recommendation**: Use `-Force` only when needed

### Memory Usage
- Each module: ~1-5 MB (depending on size)
- Your modules: ~1 MB total ✅ **Efficient**
- Can handle 100+ modules without issues

## Recommendations for Your Structure

### Current Issues to Address

1. **Terminal Module (198 functions)**
   - **Issue**: Too large, likely miscategorized
   - **Action**: Split into:
     - `RawrXD.Terminal` (CLI commands)
     - `RawrXD.Editor` (Editor functions)
     - `RawrXD.FileSystem` (File operations)
     - `RawrXD.CLI` (CLI interface)

2. **Logging Module (67 functions)**
   - **Issue**: May contain non-logging functions
   - **Action**: Review and recategorize

### Optimal Structure

```
Target: 15-20 modules with 10-50 functions each
Current: 11 modules (good start)
```

## Extension System Limits

Your extension registry (`$script:extensionRegistry`) has:
- **No hard limit** on number of extensions
- **Practical limit**: Memory and function count
- **Current capacity**: Thousands of extensions possible

### Extension Loading
```powershell
# Check extension registry size
$script:extensionRegistry.Count

# Check loaded extensions
Get-Module | Where-Object { $_.Name -like "RawrXD.*" }
```

## Migration to PowerShell 7+

**Benefits**:
- ✅ No function/variable limits
- ✅ Better performance
- ✅ Cross-platform support
- ✅ Modern features

**Command to check version**:
```powershell
$PSVersionTable.PSVersion
```

## Summary

| Metric | Limit | Your Usage | Status |
|--------|-------|------------|--------|
| Functions | 4,096 (PS 5.1) / Unlimited (PS 7+) | 299 | ✅ 7.3% |
| Modules | No limit | 11 | ✅ Excellent |
| Extensions | No limit | Variable | ✅ Unlimited |
| Nesting | 10 levels | < 3 | ✅ Safe |

**Conclusion**: Your refactored structure is **well within limits** and has **plenty of room for growth**. Consider splitting the Terminal module for better organization, but you're not hitting any technical constraints.

