# Quick Reference: Missing Functions Diagnostic Tool

## How to Use

### Run the Diagnostic Script
```powershell
powershell -ExecutionPolicy Bypass -File "C:\Users\HiH8e\OneDrive\Desktop\find-missing-functions.ps1"
```

### What It Does
1. Scans `IDEre2.html` for all function definitions
2. Identifies all functions called from inline event handlers (onclick, onchange, etc.)
3. Compares the two lists to find missing functions
4. Generates a color-coded console report
5. Saves detailed report to `missing-functions-report.txt`

### Output
- **Green:** Summary information and success messages
- **Red:** Missing functions section header
- **Yellow:** Count of missing functions
- **Cyan:** Table headers
- **White:** Function names and call counts

### Interpreting Results

#### ✅ No Missing Functions
```
Found 0 undefined functions called from inline handlers
No missing functions found! All event handler functions appear to be defined.
```
This means all your onclick/onchange handlers can find their functions. ✨

#### ❌ Missing Functions Found
```
Function Name                          | Call Count
-------------------------------------------------------
myFunction                              | 5
anotherFunction                         | 2
```
This means:
- `myFunction()` is called 5 times from event handlers but not defined anywhere
- `anotherFunction()` is called 2 times from event handlers but not defined anywhere

### What to Do When Functions Are Missing

1. **Check if the function should exist:**
   - Is it a typo in the HTML? (e.g., `onClick="myFuntion()"` instead of `myFunction()`)
   - Was it removed during refactoring?
   - Is it in a different file?

2. **If the function should exist, add it:**
   ```javascript
   function myFunction() {
       console.log('Function called!');
       // Your implementation here
   }
   
   // Expose it globally for onclick handlers
   window.myFunction = myFunction;
   ```

3. **If using defineGlobalFunction helper:**
   ```javascript
   function myFunction() {
       // Your implementation
   }
   
   // Use the helper to expose it (if available in your script block)
   if (typeof defineGlobalFunction === 'function') {
       defineGlobalFunction('myFunction', myFunction);
   } else {
       window.myFunction = myFunction;
   }
   ```

### Pattern Detection
The script detects these function definition patterns:
- `function myFunction() { }`
- `async function myFunction() { }`
- `const myFunction = function() { }`
- `const myFunction = () => { }`
- `const myFunction = async () => { }`
- `let myFunction = function() { }`
- `var myFunction = function() { }`
- `window.myFunction = function() { }`
- `window.myFunction = async function() { }`

### Common Pitfalls

❌ **Function defined AFTER it's assigned to window:**
```javascript
window.myFunction = myFunction;  // ← undefined here!

function myFunction() {
    // This is defined too late
}
```

✅ **Correct order:**
```javascript
function myFunction() {
    // Define first
}

window.myFunction = myFunction;  // ← Now it exists!
```

❌ **Function in IIFE (Immediately Invoked Function Expression) not exposed:**
```javascript
(function() {
    function myFunction() {
        // This is private, not accessible from onclick handlers!
    }
})();
```

✅ **Expose from IIFE:**
```javascript
(function() {
    function myFunction() {
        // Implementation
    }
    
    // Expose to global scope
    window.myFunction = myFunction;
})();
```

### Advanced Usage

#### Check a Different File
```powershell
powershell -ExecutionPolicy Bypass -File "find-missing-functions.ps1" -FilePath "C:\path\to\your\file.html"
```

#### View the Generated Report
```powershell
Get-Content "C:\Users\HiH8e\OneDrive\Desktop\missing-functions-report.txt"
```

### Integration with Development Workflow

**Before committing changes:**
```powershell
# Run the diagnostic
powershell -ExecutionPolicy Bypass -File "find-missing-functions.ps1"

# If errors found, fix them
# Then run again to verify
powershell -ExecutionPolicy Bypass -File "find-missing-functions.ps1"
```

**Automate with a batch file** (`check-functions.bat`):
```batch
@echo off
powershell -ExecutionPolicy Bypass -File "find-missing-functions.ps1"
pause
```

### Troubleshooting

**Issue:** Script doesn't run
**Solution:** Ensure PowerShell execution policy allows scripts:
```powershell
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```

**Issue:** False positives (functions shown as missing but they work)
**Solution:** The function might be:
- Defined dynamically at runtime
- Loaded from an external script
- Added to a different scope

**Issue:** Can't find the script
**Solution:** Use full path:
```powershell
powershell -ExecutionPolicy Bypass -File "C:\Users\HiH8e\OneDrive\Desktop\find-missing-functions.ps1"
```

## Summary
This tool helps you maintain code quality by ensuring all event handler functions are properly defined and accessible. Run it regularly during development to catch issues early! 🚀
