import { readFileSync, writeFileSync } from 'fs';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);

// Configuration
const HTML_FILE = 'C:\\Users\\HiH8e\\OneDrive\\Desktop\\IDEre2.html';

console.log('🔍 Scanning for template literal, DOM, and wiring issues...\n');

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
    
    // Backend/Middleware/Frontend wiring checks
    const requiredEndpoints = {
        backend: ['/api/health', '/api/drives', '/api/files/list', '/api/files/read', '/api/files/write', '/api/files/search', '/api/terminal'],
        orchestra: ['/health', '/v1/chat/completions', '/v1/models', '/api/tags']
    };
    
    const requiredConnectionChecks = ['checkBackendConnection', 'checkOllamaConnection', 'checkOrchestraConnection'];
    const requiredInitializations = ['initApp', 'initializeApp', 'DOMContentLoaded', 'window.addEventListener'];
    
    const foundEndpoints = {
        backend: new Set(),
        orchestra: new Set()
    };
    const foundConnectionChecks = new Set();
    const foundInitializations = new Set();
    const missingEventListeners = [];
    const apiCalls = [];
    const connectionStatusVars = new Set();
    
    // Placeholder/Mock/Simulation detection
    const placeholderPatterns = [
        /TODO/i, /FIXME/i, /XXX/i, /HACK/i, /PLACEHOLDER/i, /TEMP/i,
        /placeholder/i, /mock/i, /simulate/i, /fake/i, /dummy/i, /stub/i,
        /test data/i, /hardcoded/i, /example/i, /sample/i, /demo/i,
        /not implemented/i, /coming soon/i, /future/i, /later/i
    ];
    
    const mockPatterns = [
        /return\s+['"](?:mock|fake|dummy|test|placeholder|simulated)/i,
        /mock\w+/i, /fake\w+/i, /dummy\w+/i, /stub\w+/i,
        /simulate\w+/i, /test\w+data/i, /hardcoded/i
    ];
    
    const placeholderIssues = [];
    const mockIssues = [];
    const simulatedCodeIssues = [];
    
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
        
        // Check for backend endpoint usage
        requiredEndpoints.backend.forEach(endpoint => {
            if (line.includes(endpoint)) {
                foundEndpoints.backend.add(endpoint);
                apiCalls.push({
                    line: lineNum,
                    type: 'backend',
                    endpoint: endpoint,
                    code: line.trim().substring(0, 100)
                });
            }
        });
        
        // Check for Orchestra endpoint usage
        requiredEndpoints.orchestra.forEach(endpoint => {
            if (line.includes(endpoint)) {
                foundEndpoints.orchestra.add(endpoint);
                apiCalls.push({
                    line: lineNum,
                    type: 'orchestra',
                    endpoint: endpoint,
                    code: line.trim().substring(0, 100)
                });
            }
        });
        
        // Check for connection check functions
        requiredConnectionChecks.forEach(checkFunc => {
            if (line.includes(checkFunc)) {
                foundConnectionChecks.add(checkFunc);
            }
        });
        
        // Check for initialization calls
        if (line.includes('initApp') || line.includes('initializeApp')) {
            foundInitializations.add('initApp');
        }
        if (line.includes('DOMContentLoaded')) {
            foundInitializations.add('DOMContentLoaded');
        }
        if (line.includes('window.addEventListener') && (line.includes('load') || line.includes('DOMContentLoaded'))) {
            foundInitializations.add('window.addEventListener');
        }
        
        // Check for connection status variables
        if (line.match(/is(Backend|Ollama|Orchestra)Connected\s*=/)) {
            const match = line.match(/(is\w+Connected)\s*=/);
            if (match) {
                connectionStatusVars.add(match[1]);
            }
        }
        
        // Check for missing connection checks before API calls
        if (line.includes('fetch(') && (line.includes('localhost:9000') || line.includes('localhost:11442'))) {
            // Check if previous lines have connection checks
            let hasConnectionCheck = false;
            for (let j = Math.max(0, i - 10); j < i; j++) {
                if (lines[j].includes('isBackendConnected') || lines[j].includes('isOrchestraConnected') || 
                    lines[j].includes('checkBackendConnection') || lines[j].includes('checkOrchestraConnection')) {
                    hasConnectionCheck = true;
                    break;
                }
            }
            if (!hasConnectionCheck) {
                issues.push({
                    type: 'MISSING_CONNECTION_CHECK',
                    line: lineNum,
                    severity: 'WARNING',
                    message: 'API call found without connection check - may fail silently if server is offline',
                    code: line.trim().substring(0, 100),
                    fix: 'Add connection check before API call: if (!isBackendConnected) { showToast("Backend offline"); return; }'
                });
            }
        }
        
        // Check for event listeners that should be attached
        if (line.includes('addEventListener') && line.includes('click')) {
            // Check if it's properly attached
            if (!line.includes('document.') && !line.includes('window.') && !line.includes('element.')) {
                issues.push({
                    type: 'INCOMPLETE_EVENT_LISTENER',
                    line: lineNum,
                    severity: 'WARNING',
                    message: 'Event listener may not be properly attached to DOM element',
                    code: line.trim().substring(0, 100)
                });
            }
        }
        
        // Check for missing error handling in fetch calls
        if (line.includes('fetch(') && !line.includes('catch')) {
            // Check if there's a try-catch in the surrounding context
            let hasTryCatch = false;
            let tryDepth = 0;
            for (let j = Math.max(0, i - 20); j <= i; j++) {
                if (lines[j].includes('try {')) {
                    tryDepth++;
                }
                if (lines[j].includes('catch')) {
                    if (tryDepth > 0) {
                        hasTryCatch = true;
                        break;
                    }
                }
                if (lines[j].includes('}') && tryDepth > 0) {
                    tryDepth--;
                }
            }
            if (!hasTryCatch) {
                issues.push({
                    type: 'MISSING_ERROR_HANDLING',
                    line: lineNum,
                    severity: 'WARNING',
                    message: 'Fetch call without error handling - network errors will be unhandled',
                    code: line.trim().substring(0, 100),
                    fix: 'Wrap fetch in try-catch or add .catch() handler'
                });
            }
        }
        
        // Check for placeholders
        placeholderPatterns.forEach(pattern => {
            if (pattern.test(line) && !line.trim().startsWith('//')) {
                // Skip if it's in a comment that's explaining something
                if (!line.includes('//') || line.indexOf('//') > line.indexOf(pattern.source.replace(/[\/\\^$*+?.()|[\]{}]/g, '\\$&'))) {
                    placeholderIssues.push({
                        type: 'PLACEHOLDER_DETECTED',
                        line: lineNum,
                        severity: 'WARNING',
                        message: `Placeholder text found: ${pattern.source}`,
                        code: line.trim().substring(0, 100),
                        fix: 'Replace placeholder with real implementation'
                    });
                }
            }
        });
        
        // Check for mock/simulated code
        mockPatterns.forEach(pattern => {
            if (pattern.test(line)) {
                // Check if it's a return statement with mock data
                if (line.match(/return\s+['"](?:mock|fake|dummy|test|placeholder)/i)) {
                    mockIssues.push({
                        type: 'MOCK_RETURN_VALUE',
                        line: lineNum,
                        severity: 'ERROR',
                        message: 'Function returns mock/fake/placeholder data instead of real implementation',
                        code: line.trim().substring(0, 100),
                        fix: 'Replace mock return value with real API call or actual data'
                    });
                } else if (line.match(/(?:mock|fake|dummy|stub|simulate)\w+/i)) {
                    mockIssues.push({
                        type: 'MOCK_FUNCTION_CALL',
                        line: lineNum,
                        severity: 'WARNING',
                        message: 'Mock/simulated function call detected - may not be using real implementation',
                        code: line.trim().substring(0, 100),
                        fix: 'Replace with real function call or API endpoint'
                    });
                }
            }
        });
        
        // Check for hardcoded test data in place of real API responses
        if (line.match(/const\s+\w+\s*=\s*['"{\[].*['"}\]]\s*;/) && 
            (line.includes('test') || line.includes('example') || line.includes('sample') || 
             line.includes('mock') || line.includes('fake') || line.includes('dummy'))) {
            simulatedCodeIssues.push({
                type: 'HARDCODED_TEST_DATA',
                line: lineNum,
                severity: 'WARNING',
                message: 'Hardcoded test/example data detected - should use real API response',
                code: line.trim().substring(0, 100),
                fix: 'Replace hardcoded data with fetch() call to real endpoint'
            });
        }
        
        // Check for commented out real code (suspicious - might be placeholder)
        if (line.trim().startsWith('//') && (
            line.includes('fetch(') || line.includes('await') || 
            line.includes('API') || line.includes('endpoint')
        )) {
            // Check if there's active code below that's a placeholder
            let hasPlaceholderBelow = false;
            for (let j = i + 1; j < Math.min(i + 5, lines.length); j++) {
                if (lines[j].trim() && !lines[j].trim().startsWith('//')) {
                    if (lines[j].match(/return\s+['"](?:mock|fake|placeholder)/i) ||
                        lines[j].match(/(?:TODO|FIXME|PLACEHOLDER)/i)) {
                        hasPlaceholderBelow = true;
                        break;
                    }
                }
            }
            if (hasPlaceholderBelow) {
                simulatedCodeIssues.push({
                    type: 'COMMENTED_REAL_CODE',
                    line: lineNum,
                    severity: 'WARNING',
                    message: 'Real implementation is commented out while placeholder code is active',
                    code: line.trim().substring(0, 100),
                    fix: 'Uncomment real code and remove placeholder implementation'
                });
            }
        }
        
        // Check for functions that should make API calls but return hardcoded values
        if (line.match(/async\s+function\s+\w+\(\)/) || line.match(/const\s+\w+\s*=\s*async\s*\(\)\s*=>/)) {
            // Check next 20 lines for return statement with hardcoded data
            let hasRealApiCall = false;
            let hasHardcodedReturn = false;
            for (let j = i + 1; j < Math.min(i + 20, lines.length); j++) {
                if (lines[j].includes('fetch(') || lines[j].includes('await fetch')) {
                    hasRealApiCall = true;
                }
                if (lines[j].match(/return\s+['"{\[].*['"}\]]\s*;/) && 
                    !lines[j].includes('await') && !lines[j].includes('fetch')) {
                    hasHardcodedReturn = true;
                }
                if (lines[j].includes('}') && lines[j].match(/^\s*}\s*$/)) {
                    break; // End of function
                }
            }
            if (hasHardcodedReturn && !hasRealApiCall) {
                const funcMatch = line.match(/(?:async\s+function|const\s+(\w+)\s*=\s*async)/);
                const funcName = funcMatch ? (funcMatch[1] || 'unknown') : 'unknown';
                mockIssues.push({
                    type: 'FUNCTION_WITHOUT_API_CALL',
                    line: lineNum,
                    severity: 'ERROR',
                    message: `Async function '${funcName}' returns hardcoded data instead of making API call`,
                    code: line.trim().substring(0, 100),
                    fix: 'Replace hardcoded return with fetch() call to real endpoint'
                });
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
    
    // Check for missing connection checks
    requiredConnectionChecks.forEach(checkFunc => {
        if (!foundConnectionChecks.has(checkFunc)) {
            issues.push({
                type: 'MISSING_CONNECTION_CHECK_FUNCTION',
                line: 0,
                severity: 'WARNING',
                message: `Connection check function '${checkFunc}' not found - backend/Orchestra status may not be monitored`,
                fix: `Implement ${checkFunc}() function to check server status`
            });
        }
    });
    
    // Check for missing initializations
    if (foundInitializations.size === 0) {
        issues.push({
            type: 'MISSING_INITIALIZATION',
            line: 0,
            severity: 'ERROR',
            message: 'No initialization code found - app may not start properly',
            fix: 'Add DOMContentLoaded event listener or initApp() call'
        });
    }
    
    // Check for unused endpoints (endpoints defined but not called)
    const allEndpoints = [...requiredEndpoints.backend, ...requiredEndpoints.orchestra];
    allEndpoints.forEach(endpoint => {
        const isBackend = requiredEndpoints.backend.includes(endpoint);
        const endpointSet = isBackend ? foundEndpoints.backend : foundEndpoints.orchestra;
        if (!endpointSet.has(endpoint)) {
            issues.push({
                type: 'UNUSED_ENDPOINT',
                line: 0,
                severity: 'INFO',
                message: `Endpoint '${endpoint}' is available but not used in code`,
                fix: `Consider using ${endpoint} if needed, or remove if not required`
            });
        }
    });
    
    // Check for connection status variables
    if (connectionStatusVars.size === 0) {
        issues.push({
            type: 'MISSING_CONNECTION_STATUS',
            line: 0,
            severity: 'WARNING',
            message: 'No connection status variables found - cannot track backend/Orchestra state',
            fix: 'Add isBackendConnected and isOrchestraConnected variables'
        });
    }
    
    // Generate report
    console.log('📊 SCAN RESULTS:\n');
    console.log(`Total lines scanned: ${lines.length}`);
    console.log(`Issues found: ${issues.length}\n`);
    
    // Backend/Middleware/Frontend wiring summary
    console.log('🔌 WIRING STATUS:\n');
    console.log(`Backend endpoints used: ${foundEndpoints.backend.size}/${requiredEndpoints.backend.length}`);
    console.log(`  Found: ${Array.from(foundEndpoints.backend).join(', ')}`);
    console.log(`  Missing: ${requiredEndpoints.backend.filter(e => !foundEndpoints.backend.has(e)).join(', ')}\n`);
    
    console.log(`Orchestra endpoints used: ${foundEndpoints.orchestra.size}/${requiredEndpoints.orchestra.length}`);
    console.log(`  Found: ${Array.from(foundEndpoints.orchestra).join(', ')}`);
    console.log(`  Missing: ${requiredEndpoints.orchestra.filter(e => !foundEndpoints.orchestra.has(e)).join(', ')}\n`);
    
    console.log(`Connection checks: ${foundConnectionChecks.size}/${requiredConnectionChecks.length}`);
    console.log(`  Found: ${Array.from(foundConnectionChecks).join(', ')}`);
    console.log(`  Missing: ${requiredConnectionChecks.filter(c => !foundConnectionChecks.has(c)).join(', ')}\n`);
    
    console.log(`Initializations: ${foundInitializations.size} found`);
    console.log(`  Found: ${Array.from(foundInitializations).join(', ')}\n`);
    
    console.log(`Connection status variables: ${connectionStatusVars.size} found`);
    console.log(`  Found: ${Array.from(connectionStatusVars).join(', ')}\n`);
    
    console.log(`API calls detected: ${apiCalls.length}\n`);
    
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
        fixes: fixes,
        wiring: {
            backendEndpoints: {
                found: Array.from(foundEndpoints.backend),
                missing: requiredEndpoints.backend.filter(e => !foundEndpoints.backend.has(e))
            },
            orchestraEndpoints: {
                found: Array.from(foundEndpoints.orchestra),
                missing: requiredEndpoints.orchestra.filter(e => !foundEndpoints.orchestra.has(e))
            },
            connectionChecks: {
                found: Array.from(foundConnectionChecks),
                missing: requiredConnectionChecks.filter(c => !foundConnectionChecks.has(c))
            },
            initializations: Array.from(foundInitializations),
            connectionStatusVars: Array.from(connectionStatusVars),
            apiCalls: apiCalls
        }
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

