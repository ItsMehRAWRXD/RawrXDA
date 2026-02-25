import { readFileSync, writeFileSync } from 'fs';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);

// Configuration
const HTML_FILE = 'C:\\Users\\HiH8e\\OneDrive\\Desktop\\IDEre2.html';

console.log('🔍 Scanning for template literal and DOM issues...\n');

try {
    const content = readFileSync(HTML_FILE, 'utf-8');
    const lines = content.split('\n');
    
    const issues = [];
    let inTemplateLiteral = false;
    let templateStartLine = 0;
    let templateDepth = 0;
    const backtickStack = [];
    
    // Functions that should be globally exposed
    const requiredFunctions = [
        'setAIMode', 'setQuality', 'toggleTuningPanel', 'changeModel',
        'handleQueueInput', 'sendToAgent', 'showBottomTab', 'togglePanel',
        'showSearchPanel', 'browseDrives', 'saveCurrentFile', 'createNewFile',
        'toggleFloatAIPanel', 'clearAIChat', 'toggleAIPanelVisibility',
        'togglePaneCollapse', 'askAgent', 'runCode', 'contextMenuAction',
        'toggleMessageQueue', 'submitQueuedMessage', 'removeFromQueue',
        'executeTerminalCommand', 'clearTerminalOutput', 'handleTerminalKeydown',
        'handleTerminalKeypress', 'executeAgenticTerminalCommand', 'readFileFromSystem'
    ];
    
    const exposedFunctions = new Set();
    const missingFunctions = new Set(requiredFunctions);
    
    // Scan for issues
    for (let i = 0; i < lines.length; i++) {
        const line = lines[i];
        const lineNum = i + 1;
        
        // Check for template literal boundaries
        const backtickMatches = line.match(/`/g);
        if (backtickMatches) {
            const backtickCount = backtickMatches.length;
            
            // Check for triple backticks inside template literals
            if (inTemplateLiteral && line.includes('```')) {
                issues.push({
                    type: 'TEMPLATE_LITERAL_ERROR',
                    line: lineNum,
                    severity: 'ERROR',
                    message: 'Triple backticks (```) found inside template literal - will cause syntax error',
                    code: line.trim().substring(0, 100),
                    fix: 'Replace triple backticks with escaped version or use string concatenation'
                });
            }
            
            // Track template literal state
            for (let j = 0; j < line.length; j++) {
                const char = line[j];
                const prevChar = j > 0 ? line[j - 1] : '';
                const nextChar = j < line.length - 1 ? line[j + 1] : '';
                
                // Check if backtick is escaped
                if (char === '`' && prevChar !== '\\') {
                    if (inTemplateLiteral) {
                        // Check if this is closing a template literal
                        if (backtickStack.length > 0) {
                            backtickStack.pop();
                            if (backtickStack.length === 0) {
                                inTemplateLiteral = false;
                            }
                        } else {
                            inTemplateLiteral = false;
                        }
                    } else {
                        // Starting a new template literal
                        inTemplateLiteral = true;
                        templateStartLine = lineNum;
                        backtickStack.push(lineNum);
                    }
                }
            }
        }
        
        // Check for function definitions
        const functionMatch = line.match(/(?:function\s+|const\s+|let\s+|var\s+|window\.)?(\w+)\s*[=:]\s*(?:function|\(|async\s*\(|=>)/);
        if (functionMatch) {
            const funcName = functionMatch[1];
            if (requiredFunctions.includes(funcName)) {
                exposedFunctions.add(funcName);
                missingFunctions.delete(funcName);
            }
        }
        
        // Check for window assignments
        const windowMatch = line.match(/window\[?['"]?(\w+)['"]?\]?\s*=\s*/);
        if (windowMatch) {
            const funcName = windowMatch[1];
            if (requiredFunctions.includes(funcName)) {
                exposedFunctions.add(funcName);
                missingFunctions.delete(funcName);
            }
        }
        
        // Check for onclick/onkeypress handlers that reference undefined functions
        const handlerMatch = line.match(/on(?:click|keypress|change|input)="([^"]+)"/);
        if (handlerMatch) {
            const handler = handlerMatch[1];
            const funcCalls = handler.match(/(\w+)\s*\(/g);
            if (funcCalls) {
                funcCalls.forEach(call => {
                    const funcName = call.replace(/\s*\(/, '');
                    if (requiredFunctions.includes(funcName) && !exposedFunctions.has(funcName)) {
                        issues.push({
                            type: 'MISSING_FUNCTION',
                            line: lineNum,
                            severity: 'ERROR',
                            message: `Function '${funcName}' is referenced in inline handler but may not be globally exposed`,
                            code: line.trim().substring(0, 100)
                        });
                    }
                });
            }
        }
        
        // Check for common syntax errors in template literals
        if (inTemplateLiteral) {
            // Check for unescaped backticks in code examples
            if (line.includes('```') && !line.includes('\\`\\`\\`')) {
                issues.push({
                    type: 'UNESCAPED_BACKTICKS',
                    line: lineNum,
                    severity: 'ERROR',
                    message: 'Unescaped triple backticks in template literal - will break syntax',
                    code: line.trim().substring(0, 100),
                    fix: 'Replace ``` with \\`\\`\\` or use string concatenation'
                });
            }
            
            // Check for ${} inside what looks like markdown code blocks
            if (line.includes('```') && line.includes('${')) {
                issues.push({
                    type: 'TEMPLATE_IN_TEMPLATE',
                    line: lineNum,
                    severity: 'WARNING',
                    message: 'Template expression ${} found near code block markers - may cause issues',
                    code: line.trim().substring(0, 100)
                });
            }
        }
    }
    
    // Report missing functions
    if (missingFunctions.size > 0) {
        issues.push({
            type: 'MISSING_FUNCTIONS',
            line: 0,
            severity: 'WARNING',
            message: `Functions not found in code: ${Array.from(missingFunctions).join(', ')}`,
            fix: 'Ensure these functions are defined and exposed to window object'
        });
    }
    
    // Generate report
    console.log('📊 SCAN RESULTS:\n');
    console.log(`Total lines scanned: ${lines.length}`);
    console.log(`Issues found: ${issues.length}\n`);
    
    // Group by severity
    const errors = issues.filter(i => i.severity === 'ERROR');
    const warnings = issues.filter(i => i.severity === 'WARNING');
    
    if (errors.length > 0) {
        console.log('❌ ERRORS (must fix):\n');
        errors.forEach(issue => {
            console.log(`  Line ${issue.line}: ${issue.message}`);
            if (issue.code) {
                console.log(`    Code: ${issue.code}`);
            }
            if (issue.fix) {
                console.log(`    Fix: ${issue.fix}`);
            }
            console.log('');
        });
    }
    
    if (warnings.length > 0) {
        console.log('⚠️  WARNINGS:\n');
        warnings.forEach(issue => {
            console.log(`  Line ${issue.line}: ${issue.message}`);
            if (issue.code) {
                console.log(`    Code: ${issue.code}`);
            }
            console.log('');
        });
    }
    
    // Generate fix suggestions
    console.log('\n🔧 AUTO-FIX SUGGESTIONS:\n');
    
    const fixes = [];
    issues.forEach(issue => {
        if (issue.type === 'TEMPLATE_LITERAL_ERROR' || issue.type === 'UNESCAPED_BACKTICKS') {
            const line = lines[issue.line - 1];
            if (line.includes('```powershell')) {
                fixes.push({
                    line: issue.line,
                    old: line,
                    new: line.replace(/```powershell[^`]*```/g, 'Use code blocks with powershell/cmd format')
                });
            } else if (line.includes('```')) {
                fixes.push({
                    line: issue.line,
                    old: line,
                    new: line.replace(/```([^`]*)```/g, 'code block: $1')
                });
            }
        }
    });
    
    if (fixes.length > 0) {
        console.log('Suggested fixes for template literal issues:\n');
        fixes.forEach(fix => {
            console.log(`Line ${fix.line}:`);
            console.log(`  OLD: ${fix.old.trim()}`);
            console.log(`  NEW: ${fix.new.trim()}`);
            console.log('');
        });
    }
    
    // Save report to file
    const report = {
        timestamp: new Date().toISOString(),
        file: HTML_FILE,
        totalLines: lines.length,
        issues: issues,
        exposedFunctions: Array.from(exposedFunctions),
        missingFunctions: Array.from(missingFunctions),
        fixes: fixes
    };
    
    const reportFile = join(__dirname, 'template-literal-scan-report.json');
    writeFileSync(reportFile, JSON.stringify(report, null, 2));
    console.log(`\n✅ Report saved to: ${reportFile}`);
    
    // Exit with error code if critical issues found
    if (errors.length > 0) {
        process.exit(1);
    }
    
} catch (error) {
    console.error('❌ Error scanning file:', error.message);
    process.exit(1);
}

