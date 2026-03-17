// ========================================================
// IDE FEATURE INVENTORY & LOGGER
// ========================================================
// Comprehensive script to log ALL features in the IDE

(function inventoryIDEFeatures() {
    console.log('%c🔍 IDE FEATURE INVENTORY STARTING...', 'color: cyan; font-size: 16px; font-weight: bold;');
    
    const inventory = {
        timestamp: new Date().toISOString(),
        globalFunctions: {},
        globalVariables: {},
        domElements: {},
        eventListeners: {},
        windowProperties: {},
        errors: []
    };

    // ========================================================
    // 1. SCAN GLOBAL FUNCTIONS
    // ========================================================
    console.log('%c📋 Scanning Global Functions...', 'color: yellow; font-weight: bold;');
    
    const functionPatterns = [
        'init', 'setup', 'create', 'add', 'remove', 'delete', 'update',
        'send', 'receive', 'fetch', 'load', 'save', 'open', 'close',
        'show', 'hide', 'toggle', 'switch', 'change', 'set', 'get',
        'start', 'stop', 'pause', 'resume', 'restart',
        'chat', 'message', 'multi', 'tab', 'panel', 'window',
        'editor', 'terminal', 'console', 'debug', 'test',
        'file', 'folder', 'tree', 'explorer',
        'ai', 'agent', 'model', 'bigdaddy', 'beaconism'
    ];

    for (let key in window) {
        try {
            if (typeof window[key] === 'function') {
                const funcName = key.toLowerCase();
                const isRelevant = functionPatterns.some(pattern => funcName.includes(pattern));
                
                if (isRelevant) {
                    inventory.globalFunctions[key] = {
                        type: 'function',
                        exists: true,
                        callable: typeof window[key] === 'function',
                        signature: window[key].toString().substring(0, 200)
                    };
                }
            }
        } catch (e) {
            inventory.errors.push({ key, error: e.message, type: 'function_scan' });
        }
    }

    console.log(`✅ Found ${Object.keys(inventory.globalFunctions).length} relevant functions`);

    // ========================================================
    // 2. SCAN GLOBAL VARIABLES & OBJECTS
    // ========================================================
    console.log('%c📋 Scanning Global Variables...', 'color: yellow; font-weight: bold;');
    
    const variablePatterns = [
        'chat', 'multi', 'tab', 'panel', 'editor', 'terminal',
        'file', 'open', 'active', 'current', 'selected',
        'ai', 'model', 'agent', 'config', 'settings', 'state'
    ];

    for (let key in window) {
        try {
            if (typeof window[key] !== 'function') {
                const varName = key.toLowerCase();
                const isRelevant = variablePatterns.some(pattern => varName.includes(pattern));
                
                if (isRelevant) {
                    const value = window[key];
                    inventory.globalVariables[key] = {
                        type: typeof value,
                        isNull: value === null,
                        isArray: Array.isArray(value),
                        isMap: value instanceof Map,
                        isSet: value instanceof Set,
                        size: Array.isArray(value) ? value.length : 
                              (value instanceof Map || value instanceof Set) ? value.size :
                              typeof value === 'object' ? Object.keys(value || {}).length : null
                    };
                }
            }
        } catch (e) {
            inventory.errors.push({ key, error: e.message, type: 'variable_scan' });
        }
    }

    console.log(`✅ Found ${Object.keys(inventory.globalVariables).length} relevant variables`);

    // ========================================================
    // 3. SCAN DOM ELEMENTS
    // ========================================================
    console.log('%c📋 Scanning DOM Elements...', 'color: yellow; font-weight: bold;');
    
    const elementSelectors = [
        // Multi-chat elements
        '#multi-chat-panel',
        '#multi-chat-messages',
        '#multi-chat-input',
        '#multi-chat-send',
        '#multi-chat-tabs',
        '#chat-tab-list',
        '.chat-tab',
        
        // Editor elements
        '#code-editor',
        '#editor',
        '#editor-tab-bar',
        '.editor-tab',
        
        // Terminal elements
        '#terminal-panel',
        '#terminal-pane',
        '.terminal-output',
        
        // Problems panel
        '#problems-panel',
        '#console-output',
        '#debug-console',
        
        // File explorer
        '#file-tree',
        '#file-explorer',
        
        // Buttons and controls
        '[onclick*="multi"]',
        '[onclick*="chat"]',
        '[onclick*="tab"]',
        '[onclick*="send"]',
        
        // AI elements
        '[id*="ai"]',
        '[id*="agent"]',
        '[id*="model"]'
    ];

    elementSelectors.forEach(selector => {
        try {
            const elements = document.querySelectorAll(selector);
            if (elements.length > 0) {
                inventory.domElements[selector] = {
                    count: elements.length,
                    visible: Array.from(elements).filter(el => {
                        const style = window.getComputedStyle(el);
                        return style.display !== 'none' && style.visibility !== 'hidden';
                    }).length,
                    ids: Array.from(elements).map(el => el.id).filter(Boolean),
                    classes: Array.from(elements).map(el => el.className).filter(Boolean)
                };
            }
        } catch (e) {
            inventory.errors.push({ selector, error: e.message, type: 'dom_scan' });
        }
    });

    console.log(`✅ Found ${Object.keys(inventory.domElements).length} element types`);

    // ========================================================
    // 4. CHECK SPECIFIC MULTI-CHAT FUNCTIONS
    // ========================================================
    console.log('%c📋 Checking Multi-Chat Functions...', 'color: yellow; font-weight: bold;');
    
    const multiChatFunctions = [
        'sendMultiChatMessage',
        'addMultiChatTab',
        'removeMultiChatTab',
        'switchMultiChatTab',
        'createNewChatTab',
        'toggleMultiChatResearch',
        'toggleMultiChatSettings',
        'saveMultiChatCheckpoint',
        'restoreLastCheckpoint',
        'retryLastCheckpoint',
        'changeMultiChatModel',
        'clearMultiChat',
        'closeMultiChat'
    ];

    inventory.multiChatStatus = {};
    multiChatFunctions.forEach(funcName => {
        inventory.multiChatStatus[funcName] = {
            exists: typeof window[funcName] === 'function',
            type: typeof window[funcName],
            location: window[funcName] ? 'window' : 'not found'
        };
    });

    const missingFunctions = multiChatFunctions.filter(f => typeof window[f] !== 'function');
    if (missingFunctions.length > 0) {
        console.warn(`⚠️ Missing ${missingFunctions.length} multi-chat functions:`, missingFunctions);
    } else {
        console.log('✅ All multi-chat functions found');
    }

    // ========================================================
    // 5. CHECK EVENT HANDLERS
    // ========================================================
    console.log('%c📋 Checking Event Handlers...', 'color: yellow; font-weight: bold;');
    
    const buttonsWithOnclick = document.querySelectorAll('[onclick]');
    inventory.eventListeners.onclickButtons = {
        total: buttonsWithOnclick.length,
        handlers: Array.from(buttonsWithOnclick).map(btn => ({
            id: btn.id,
            class: btn.className,
            onclick: btn.getAttribute('onclick')
        }))
    };

    console.log(`✅ Found ${buttonsWithOnclick.length} elements with onclick handlers`);

    // ========================================================
    // 6. SAVE INVENTORY TO FILE
    // ========================================================
    const inventoryJSON = JSON.stringify(inventory, null, 2);
    const inventoryText = generateInventoryReport(inventory);

    // Save to localStorage
    localStorage.setItem('ide_feature_inventory', inventoryJSON);
    
    // Log to console
    console.log('%c📊 INVENTORY COMPLETE!', 'color: green; font-size: 16px; font-weight: bold;');
    console.log('Inventory saved to localStorage as "ide_feature_inventory"');
    console.log(`Total Functions: ${Object.keys(inventory.globalFunctions).length}`);
    console.log(`Total Variables: ${Object.keys(inventory.globalVariables).length}`);
    console.log(`Total DOM Elements: ${Object.keys(inventory.domElements).length}`);
    console.log(`Total Errors: ${inventory.errors.length}`);
    
    // Create downloadable report
    downloadInventoryReport(inventoryText, inventoryJSON);
    
    return inventory;

    // ========================================================
    // HELPER FUNCTIONS
    // ========================================================
    
    function generateInventoryReport(inv) {
        let report = `
═══════════════════════════════════════════════════════════
                IDE FEATURE INVENTORY REPORT
═══════════════════════════════════════════════════════════
Generated: ${inv.timestamp}

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
1. GLOBAL FUNCTIONS (${Object.keys(inv.globalFunctions).length} found)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

`;
        
        for (let [name, data] of Object.entries(inv.globalFunctions)) {
            report += `✅ ${name}\n`;
            report += `   Callable: ${data.callable}\n`;
            report += `   Signature: ${data.signature.substring(0, 100)}...\n\n`;
        }

        report += `
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
2. GLOBAL VARIABLES (${Object.keys(inv.globalVariables).length} found)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

`;
        
        for (let [name, data] of Object.entries(inv.globalVariables)) {
            report += `📦 ${name}\n`;
            report += `   Type: ${data.type}\n`;
            if (data.size !== null) report += `   Size: ${data.size}\n`;
            report += `\n`;
        }

        report += `
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
3. DOM ELEMENTS (${Object.keys(inv.domElements).length} types)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

`;
        
        for (let [selector, data] of Object.entries(inv.domElements)) {
            report += `🔍 ${selector}\n`;
            report += `   Count: ${data.count} (${data.visible} visible)\n`;
            if (data.ids.length > 0) report += `   IDs: ${data.ids.join(', ')}\n`;
            report += `\n`;
        }

        report += `
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
4. MULTI-CHAT STATUS
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

`;
        
        for (let [name, data] of Object.entries(inv.multiChatStatus)) {
            const status = data.exists ? '✅' : '❌';
            report += `${status} ${name}: ${data.exists ? 'EXISTS' : 'MISSING'}\n`;
        }

        if (inv.errors.length > 0) {
            report += `
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
5. ERRORS (${inv.errors.length} found)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

`;
            inv.errors.forEach(err => {
                report += `❌ ${err.type}: ${err.key}\n`;
                report += `   ${err.error}\n\n`;
            });
        }

        report += `
═══════════════════════════════════════════════════════════
                        END OF REPORT
═══════════════════════════════════════════════════════════
`;
        
        return report;
    }

    function downloadInventoryReport(textReport, jsonReport) {
        // Create text file
        const textBlob = new Blob([textReport], { type: 'text/plain' });
        const textUrl = URL.createObjectURL(textBlob);
        const textLink = document.createElement('a');
        textLink.href = textUrl;
        textLink.download = `ide-inventory-${Date.now()}.txt`;
        textLink.click();

        // Create JSON file
        setTimeout(() => {
            const jsonBlob = new Blob([jsonReport], { type: 'application/json' });
            const jsonUrl = URL.createObjectURL(jsonBlob);
            const jsonLink = document.createElement('a');
            jsonLink.href = jsonUrl;
            jsonLink.download = `ide-inventory-${Date.now()}.json`;
            jsonLink.click();
        }, 1000);

        console.log('📥 Inventory reports downloaded (TXT and JSON)');
    }

})();

// Make it globally available
window.inventoryIDEFeatures = inventoryIDEFeatures;

console.log('%c✅ IDE FEATURE INVENTORY SCRIPT LOADED!', 'color: green; font-size: 16px; font-weight: bold;');
console.log('%cRun: inventoryIDEFeatures() or window.inventoryIDEFeatures()', 'color: cyan;');