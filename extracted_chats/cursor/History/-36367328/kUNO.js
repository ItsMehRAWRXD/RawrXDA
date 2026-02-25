// Comprehensive validation and fix
const fs = require('fs');

console.log('🔍 Validating and fixing FIXED-CURSOR-CLONE-WITH-BIGDADDYG.html...\n');

let html = fs.readFileSync('FIXED-CURSOR-CLONE-WITH-BIGDADDYG.html', 'utf8');
let issues = [];
let fixes = 0;

// 1. Check for unclosed template literals
const templateLiteralIssues = html.match(/`[^`]*$/gm);
if (templateLiteralIssues) {
    issues.push(`⚠️ Found ${templateLiteralIssues.length} unclosed template literals`);
}

// 2. Check for broken function definitions
const functionMatches = html.match(/function\s+\w+\s*\([^)]*\)\s*\{/g);
console.log(`✅ Found ${functionMatches?.length || 0} function definitions`);

// 3. Find sendToAgent function
const sendToAgentMatch = html.match(/async\s+function\s+sendToAgent\s*\(\)\s*\{/);
if (sendToAgentMatch) {
    console.log('✅ sendToAgent function exists');
} else {
    issues.push('❌ sendToAgent function NOT FOUND!');
    console.log('❌ CRITICAL: sendToAgent function is missing!');
}

// 4. Find manualStartOrchestra function
const manualStartMatch = html.match(/async\s+function\s+startOrchestraFromIDE\s*\(\)/);
if (manualStartMatch) {
    console.log('✅ startOrchestraFromIDE function exists');
} else {
    issues.push('⚠️ startOrchestraFromIDE function might be missing');
}

// 5. Check for window.manualStartOrchestra binding
if (html.includes('window.manualStartOrchestra = startOrchestraFromIDE')) {
    console.log('✅ manualStartOrchestra is bound to window');
} else {
    issues.push('❌ manualStartOrchestra NOT bound to window');
    console.log('❌ CRITICAL: manualStartOrchestra not accessible!');
    
    // FIX: Add the binding
    const bindingCode = '\n        window.manualStartOrchestra = startOrchestraFromIDE;\n';
    const insertAt = html.indexOf("console.log('✅ Autonomous IDE initialized');");
    
    if (insertAt !== -1) {
        html = html.substring(0, insertAt) + bindingCode + html.substring(insertAt);
        fixes++;
        console.log('✅ FIXED: Added window.manualStartOrchestra binding');
    }
}

// 6. Check for duplicate function definitions
const functionNames = html.match(/function\s+(\w+)\s*\(/g);
if (functionNames) {
    const names = functionNames.map(f => f.match(/function\s+(\w+)/)[1]);
    const duplicates = names.filter((name, index) => names.indexOf(name) !== index);
    if (duplicates.length > 0) {
        issues.push('⚠️ Duplicate functions: ' + [...new Set(duplicates)].join(', '));
    } else {
        console.log('✅ No duplicate function definitions');
    }
}

// 7. Validate the embedded BigDaddyG engine class
if (html.includes('class BigDaddyGEngine {')) {
    console.log('✅ BigDaddyGEngine class exists');
    
    // Check if it has all required methods
    const requiredMethods = ['generateCodeResponse', 'generateDebugResponse', 'generateCryptoResponse', 'generateLatestResponse'];
    requiredMethods.forEach(method => {
        if (html.includes(method + '(')) {
            console.log(\`  ✅ \${method} exists\`);
        } else {
            issues.push('❌ Missing method: ' + method);
        }
    });
} else {
    issues.push('❌ BigDaddyGEngine class NOT FOUND!');
}

// 8. Check for isOrchestraConnected variable
if (html.includes('let isOrchestraConnected')) {
    console.log('✅ isOrchestraConnected variable exists');
} else {
    issues.push('❌ isOrchestraConnected variable missing');
}

// 9. Simple fix: Ensure all onclick handlers have their functions
const onclickHandlers = html.match(/onclick="([^"]+)"/g) || [];
console.log(\`✅ Found \${onclickHandlers.length} onclick handlers\`);

// Check a few critical ones
const criticalHandlers = ['sendToAgent()', 'manualStartOrchestra()'];
criticalHandlers.forEach(handler => {
    if (html.includes(\`onclick="\${handler}"\`)) {
        console.log(\`  ✅ Handler exists: \${handler}\`);
    }
});

// 10. Write the fixed file
fs.writeFileSync('FIXED-CURSOR-CLONE-WITH-BIGDADDYG.html', html);

console.log(\`\n📊 Summary:\`);
console.log(\`  Issues found: \${issues.length}\`);
console.log(\`  Fixes applied: \${fixes}\`);

if (issues.length > 0) {
    console.log(\`\n⚠️ Issues:\`);
    issues.forEach(issue => console.log(\`  - \${issue}\`));
}

if (fixes > 0) {
    console.log(\`\n✅ File updated with \${fixes} fix(es)\`);
} else {
    console.log(\`\n✅ File structure appears valid\`);
}

// 11. Create a minimal test page to isolate the issue
const testPage = \`<!DOCTYPE html>
<html>
<head>
    <title>Button Test</title>
    <style>
        body { background: #1e1e1e; color: #0098ff; font-family: monospace; padding: 20px; }
        button { background: #4ec9b0; color: white; border: none; padding: 10px 20px; 
                 cursor: pointer; font-size: 14px; border-radius: 5px; margin: 10px; 
                 position: relative; z-index: 9999; }
        button:hover { background: #00ff00; }
        #output { background: #000; padding: 20px; margin-top: 20px; border: 2px solid #0098ff; }
    </style>
</head>
<body>
    <h1>🧪 Button Click Test</h1>
    <p>If these buttons work, the issue is in the full IDE's CSS/JS</p>
    
    <button onclick="test1()">Test 1: Alert</button>
    <button onclick="test2()">Test 2: Console Log</button>
    <button onclick="test3()">Test 3: Update Text</button>
    <button id="test4">Test 4: Event Listener</button>
    
    <div id="output">
        <div id="result">Click buttons above to test...</div>
    </div>
    
    <script>
        function test1() {
            alert('✅ onclick works!');
        }
        
        function test2() {
            console.log('✅ Console works!');
            document.getElementById('result').textContent = '✅ Test 2 passed!';
        }
        
        function test3() {
            document.getElementById('result').textContent = '✅ Test 3 passed! Text updated.';
        }
        
        document.getElementById('test4').addEventListener('click', () => {
            document.getElementById('result').textContent = '✅ Test 4 passed! Event listener works.';
        });
        
        console.log('✅ Test page loaded');
    </script>
</body>
</html>\`;

fs.writeFileSync('button-test.html', testPage);
console.log(\`\n💡 Created button-test.html to isolate the issue\`);
console.log(\`   Open it to verify buttons work in a clean environment\`);

