// ========================================================
// IDE FEATURE VALIDATOR & TESTER
// ========================================================
// Tests all features found in inventory and reports what works/doesn't work

(function validateIDEFeatures() {
    console.log('%c🧪 IDE FEATURE VALIDATION STARTING...', 'color: cyan; font-size: 16px; font-weight: bold;');
    
    // Load inventory from localStorage
    const inventoryJSON = localStorage.getItem('ide_feature_inventory');
    if (!inventoryJSON) {
        console.error('❌ No inventory found! Run inventoryIDEFeatures() first');
        return null;
    }

    const inventory = JSON.parse(inventoryJSON);
    const validation = {
        timestamp: new Date().toISOString(),
        working: [],
        broken: [],
        warnings: [],
        testResults: {}
    };

    // ========================================================
    // 1. TEST GLOBAL FUNCTIONS
    // ========================================================
    console.log('%c🧪 Testing Global Functions...', 'color: yellow; font-weight: bold;');
    
    for (let [funcName, funcData] of Object.entries(inventory.globalFunctions)) {
        try {
            const func = window[funcName];
            
            if (typeof func === 'function') {
                // Test if function is callable (don't actually call it)
                const testResult = {
                    function: funcName,
                    exists: true,
                    callable: true,
                    parameters: func.length,
                    status: 'working'
                };
                
                validation.working.push(testResult);
                validation.testResults[funcName] = testResult;
            } else {
                const testResult = {
                    function: funcName,
                    exists: false,
                    callable: false,
                    status: 'broken',
                    error: 'Not a function'
                };
                
                validation.broken.push(testResult);
                validation.testResults[funcName] = testResult;
            }
        } catch (e) {
            const testResult = {
                function: funcName,
                exists: false,
                callable: false,
                status: 'broken',
                error: e.message
            };
            
            validation.broken.push(testResult);
            validation.testResults[funcName] = testResult;
        }
    }

    console.log(`✅ Working: ${validation.working.length}`);
    console.log(`❌ Broken: ${validation.broken.length}`);

    // ========================================================
    // 2. TEST MULTI-CHAT SPECIFIC FUNCTIONS
    // ========================================================
    console.log('%c🧪 Testing Multi-Chat Functions...', 'color: yellow; font-weight: bold;');
    
    const multiChatTests = {
        'sendMultiChatMessage': {
            test: () => typeof window.sendMultiChatMessage === 'function',
            required: true
        },
        'addMultiChatTab': {
            test: () => typeof window.addMultiChatTab === 'function' || 
                       typeof window.createNewChatTab === 'function',
            required: true
        },
        'Multi-chat panel exists': {
            test: () => document.getElementById('multi-chat-panel') !== null,
            required: true
        },
        'Multi-chat tabs container': {
            test: () => document.getElementById('chat-tab-list') !== null,
            required: true
        },
        'Multi-chat input': {
            test: () => document.getElementById('multi-chat-input') !== null,
            required: true
        },
        'New tab button (+ button)': {
            test: () => {
                const buttons = document.querySelectorAll('button, [onclick]');
                return Array.from(buttons).some(btn => 
                    btn.textContent.includes('+') || 
                    btn.textContent.includes('New') ||
                    (btn.getAttribute('onclick') || '').includes('chat') ||
                    (btn.getAttribute('onclick') || '').includes('tab')
                );
            },
            required: true
        }
    };

    validation.multiChatStatus = {};
    for (let [testName, testData] of Object.entries(multiChatTests)) {
        try {
            const passed = testData.test();
            validation.multiChatStatus[testName] = {
                passed: passed,
                required: testData.required,
                status: passed ? 'working' : 'broken'
            };
            
            if (!passed && testData.required) {
                validation.warnings.push({
                    test: testName,
                    message: 'Required multi-chat feature is missing or broken'
                });
            }
        } catch (e) {
            validation.multiChatStatus[testName] = {
                passed: false,
                required: testData.required,
                status: 'error',
                error: e.message
            };
        }
    }

    // ========================================================
    // 3. TEST DOM ELEMENTS VISIBILITY
    // ========================================================
    console.log('%c🧪 Testing DOM Element Visibility...', 'color: yellow; font-weight: bold;');
    
    validation.visibilityTests = {};
    for (let [selector, data] of Object.entries(inventory.domElements)) {
        const elements = document.querySelectorAll(selector);
        const visible = Array.from(elements).filter(el => {
            const style = window.getComputedStyle(el);
            return style.display !== 'none' && 
                   style.visibility !== 'hidden' &&
                   style.opacity !== '0';
        });

        validation.visibilityTests[selector] = {
            total: elements.length,
            visible: visible.length,
            hidden: elements.length - visible.length,
            status: visible.length > 0 ? 'visible' : 'hidden'
        };
    }

    // ========================================================
    // 4. GENERATE VALIDATION REPORT
    // ========================================================
    const report = generateValidationReport(validation);
    
    // Save to localStorage
    localStorage.setItem('ide_feature_validation', JSON.stringify(validation, null, 2));
    
    // Log summary
    console.log('%c📊 VALIDATION COMPLETE!', 'color: green; font-size: 16px; font-weight: bold;');
    console.log('Validation saved to localStorage as "ide_feature_validation"');
    console.log(`✅ Working: ${validation.working.length}`);
    console.log(`❌ Broken: ${validation.broken.length}`);
    console.log(`⚠️ Warnings: ${validation.warnings.length}`);
    
    // Download report
    downloadValidationReport(report, JSON.stringify(validation, null, 2));
    
    // Show critical issues
    showCriticalIssues(validation);
    
    return validation;

    // ========================================================
    // HELPER FUNCTIONS
    // ========================================================
    
    function generateValidationReport(val) {
        let report = `
═══════════════════════════════════════════════════════════
            IDE FEATURE VALIDATION REPORT
═══════════════════════════════════════════════════════════
Generated: ${val.timestamp}

SUMMARY:
  ✅ Working: ${val.working.length}
  ❌ Broken: ${val.broken.length}
  ⚠️ Warnings: ${val.warnings.length}

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
1. MULTI-CHAT STATUS
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

`;
        
        for (let [testName, result] of Object.entries(val.multiChatStatus)) {
            const icon = result.passed ? '✅' : '❌';
            const req = result.required ? '[REQUIRED]' : '[OPTIONAL]';
            report += `${icon} ${testName} ${req}\n`;
            report += `   Status: ${result.status}\n`;
            if (result.error) report += `   Error: ${result.error}\n`;
            report += `\n`;
        }

        report += `
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
2. BROKEN FUNCTIONS (${val.broken.length})
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

`;
        
        val.broken.forEach(item => {
            report += `❌ ${item.function}\n`;
            report += `   Error: ${item.error}\n\n`;
        });

        report += `
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
3. VISIBILITY TESTS
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

`;
        
        for (let [selector, result] of Object.entries(val.visibilityTests)) {
            const icon = result.visible > 0 ? '👁️' : '🙈';
            report += `${icon} ${selector}\n`;
            report += `   Total: ${result.total}, Visible: ${result.visible}, Hidden: ${result.hidden}\n\n`;
        }

        if (val.warnings.length > 0) {
            report += `
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
4. WARNINGS (${val.warnings.length})
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

`;
            val.warnings.forEach(warn => {
                report += `⚠️ ${warn.test}\n`;
                report += `   ${warn.message}\n\n`;
            });
        }

        report += `
═══════════════════════════════════════════════════════════
                    END OF REPORT
═══════════════════════════════════════════════════════════
`;
        
        return report;
    }

    function downloadValidationReport(textReport, jsonReport) {
        // Create text file
        const textBlob = new Blob([textReport], { type: 'text/plain' });
        const textUrl = URL.createObjectURL(textBlob);
        const textLink = document.createElement('a');
        textLink.href = textUrl;
        textLink.download = `ide-validation-${Date.now()}.txt`;
        textLink.click();

        // Create JSON file
        setTimeout(() => {
            const jsonBlob = new Blob([jsonReport], { type: 'application/json' });
            const jsonUrl = URL.createObjectURL(jsonBlob);
            const jsonLink = document.createElement('a');
            jsonLink.href = jsonUrl;
            jsonLink.download = `ide-validation-${Date.now()}.json`;
            jsonLink.click();
        }, 1000);

        console.log('📥 Validation reports downloaded (TXT and JSON)');
    }

    function showCriticalIssues(val) {
        console.group('%c🚨 CRITICAL ISSUES', 'color: red; font-size: 14px; font-weight: bold;');
        
        // Check multi-chat
        const multiChatBroken = Object.values(val.multiChatStatus)
            .filter(s => s.required && !s.passed);
        
        if (multiChatBroken.length > 0) {
            console.error('❌ Multi-Chat has critical issues:');
            multiChatBroken.forEach(issue => {
                console.error(`  - ${Object.keys(val.multiChatStatus).find(k => val.multiChatStatus[k] === issue)}`);
            });
        }

        // Check for missing functions
        if (val.broken.length > 0) {
            console.warn('⚠️ Missing/Broken functions:', val.broken.map(b => b.function));
        }

        console.groupEnd();
    }

})();

// Make it globally available
window.validateIDEFeatures = validateIDEFeatures;

console.log('%c✅ IDE FEATURE VALIDATOR SCRIPT LOADED!', 'color: green; font-size: 16px; font-weight: bold;');
console.log('%cRun: validateIDEFeatures() or window.validateIDEFeatures()', 'color: cyan;');
console.log('%cMake sure to run inventoryIDEFeatures() first!', 'color: yellow;');