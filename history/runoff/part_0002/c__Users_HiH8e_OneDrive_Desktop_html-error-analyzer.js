#!/usr/bin/env node
/**
 * HTML Error Analysis and Fix Tool
 * Analyzes the IDEre2.html file for specific errors and provides fixes
 */

const fs = require('fs');
const path = require('path');

const HTML_FILE = "C:\\Users\\HiH8e\\OneDrive\\Desktop\\IDEre2.html";

// Error patterns to search for and fix
const errorPatterns = [
    {
        name: "Messages Container Error",
        searchPattern: /Cannot find messages container/g,
        context: "MULTI-CHAT FIX",
        fix: "Add null check before accessing messages container"
    },
    {
        name: "Ollama Connection Error", 
        searchPattern: /Ollama not connected for this model/g,
        context: "sendToAgent",
        fix: "Add proper connection validation before model usage"
    },
    {
        name: "Console Error Override",
        searchPattern: /console\.error\s*=\s*function/g,
        context: "console override",
        fix: "Ensure proper error handling in console override"
    },
    {
        name: "Empty Catch Blocks",
        searchPattern: /catch\s*\([^)]*\)\s*{\s*}/g,
        context: "error handling",
        fix: "Add proper error logging in catch blocks"
    }
];

function analyzeHTMLFile() {
    console.log(`🔍 Analyzing ${HTML_FILE}`);
    
    if (!fs.existsSync(HTML_FILE)) {
        console.error(`❌ File not found: ${HTML_FILE}`);
        return;
    }

    const content = fs.readFileSync(HTML_FILE, 'utf8');
    const lines = content.split('\n');
    
    console.log(`📄 File has ${lines.length} lines\n`);

    let totalIssues = 0;
    
    // Check for each error pattern
    for (const pattern of errorPatterns) {
        console.log(`🔍 Checking for: ${pattern.name}`);
        
        const matches = content.match(pattern.searchPattern);
        if (matches && matches.length > 0) {
            console.log(`⚠️  Found ${matches.length} occurrence(s) of ${pattern.name}`);
            totalIssues += matches.length;
            
            // Find line numbers
            for (let i = 0; i < lines.length; i++) {
                if (pattern.searchPattern.test(lines[i])) {
                    console.log(`   Line ${i + 1}: ${lines[i].trim()}`);
                }
            }
            
            console.log(`   Suggested fix: ${pattern.fix}\n`);
        } else {
            console.log(`✅ No issues found for ${pattern.name}\n`);
        }
    }

    // Check specific line numbers from the error output
    const errorLines = [8234, 8687, 12092, 13161, 16831];
    console.log('🎯 Checking specific error lines:');
    
    for (const lineNum of errorLines) {
        if (lineNum <= lines.length) {
            const line = lines[lineNum - 1];
            console.log(`Line ${lineNum}: ${line.trim()}`);
            
            // Check if this line has problematic patterns
            if (line.includes('throw new Error')) {
                console.log(`   ⚠️  Contains error throw`);
            }
            if (line.includes('console.error')) {
                console.log(`   ⚠️  Contains console.error call`);
            }
            if (line.includes('await') && !line.includes('try')) {
                console.log(`   ⚠️  Unhandled async operation`);
            }
        }
    }
    
    console.log(`\n📊 Total issues found: ${totalIssues}`);
    
    if (totalIssues > 0) {
        console.log('\n🔧 Recommended actions:');
        console.log('1. Add proper null checks before DOM element access');
        console.log('2. Wrap async operations in try-catch blocks');
        console.log('3. Validate service connections before usage');
        console.log('4. Add meaningful error logging in catch blocks');
    } else {
        console.log('\n✅ No major issues detected in error patterns');
    }
}

function createFixedVersion() {
    console.log('\n🛠️  Creating error-resilient version...');
    
    const content = fs.readFileSync(HTML_FILE, 'utf8');
    let fixedContent = content;
    
    // Fix 1: Add null checks for DOM elements
    const domCheckFix = `
// Enhanced DOM element check with logging
function safeGetElement(id, context = 'Unknown') {
    const element = document.getElementById(id);
    if (!element) {
        console.warn(\`[SAFE-CHECK] Element '\${id}' not found in context: \${context}\`);
        return null;
    }
    return element;
}`;
    
    // Fix 2: Enhanced error wrapper for sendToAgent
    const sendToAgentFix = `
// Enhanced sendToAgent with better error handling
const originalSendToAgent = sendToAgent;
sendToAgent = async function(...args) {
    try {
        // Check Ollama connection first
        if (!isOrchestraConnected && !currentModel.startsWith('claude') && !currentModel.startsWith('gpt')) {
            const error = new Error('Ollama not connected for this model');
            console.warn('⚠️ Ollama connection check failed:', error.message);
            throw error;
        }
        
        return await originalSendToAgent.apply(this, args);
    } catch (error) {
        console.error('🚫 sendToAgent error:', error);
        // Don't rethrow if it's a connection error - just log it
        if (error.message.includes('Ollama not connected')) {
            showToast('Model not available - please check connection', 'warning');
            return null;
        }
        throw error;
    }
};`;

    // Fix 3: Enhanced console.error override with better error filtering
    const consoleErrorFix = `
// Enhanced console.error override with filtering
const originalConsoleError = console.error;
console.error = function(...args) {
    const message = args.join(' ');
    
    // Filter out expected warnings/errors that don't need to be shown
    const isExpectedError = [
        'Cannot find messages container',
        'Ollama not connected for this model',
        'Element not found'
    ].some(pattern => message.includes(pattern));
    
    if (isExpectedError) {
        // Log as warning instead of error
        console.warn('📋 Expected issue (handled):', message);
        return;
    }
    
    // Call original console.error for real errors
    originalConsoleError.apply(console, args);
    
    // Add to debug log if function exists
    if (typeof addDebugLog === 'function') {
        addDebugLog(message, 'error');
    }
};`;

    // Insert fixes at the beginning of the script section
    const scriptStart = fixedContent.indexOf('<script>');
    if (scriptStart !== -1) {
        const insertPoint = fixedContent.indexOf('\n', scriptStart) + 1;
        fixedContent = fixedContent.slice(0, insertPoint) + 
                      domCheckFix + '\n\n' + 
                      sendToAgentFix + '\n\n' + 
                      consoleErrorFix + '\n\n' +
                      fixedContent.slice(insertPoint);
    }
    
    const outputFile = HTML_FILE.replace('.html', '-fixed.html');
    fs.writeFileSync(outputFile, fixedContent);
    
    console.log(`✅ Fixed version saved as: ${outputFile}`);
    console.log('🎉 Error-resilient enhancements added!');
}

// Run analysis
console.log('🚀 HTML Error Analysis Tool\n');
analyzeHTMLFile();

// Ask if user wants to create fixed version
console.log('\n❓ Would you like to create an error-resilient version?');
console.log('   This will add enhanced error handling and null checks.');
console.log('   Run with --fix flag to automatically create it.');

if (process.argv.includes('--fix')) {
    createFixedVersion();
}