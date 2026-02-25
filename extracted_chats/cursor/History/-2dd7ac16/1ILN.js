/**
 * Tab System Debugger - Find out why tabs aren't working
 */

console.log('[TabDebug] 🔍 Tab system diagnostic starting...');

setTimeout(() => {
    console.log('\n========== TAB SYSTEM DIAGNOSTIC ==========\n');
    
    // 1. Check if TabSystem exists
    console.log('1. window.tabSystem exists?', !!window.tabSystem);
    if (window.tabSystem) {
        console.log('   - Active tabs:', Array.from(window.tabSystem.tabs.keys()));
        console.log('   - Current active tab:', window.tabSystem.activeTab);
    }
    
    // 2. Check chat buttons
    const chatButtons = document.querySelectorAll('[onclick*="openChatTab"]');
    console.log('\n2. Chat Tab buttons found:', chatButtons.length);
    chatButtons.forEach((btn, i) => {
        console.log(`   Button ${i+1}:`, btn.textContent.trim().substring(0, 30));
    });
    
    // 3. Check if #ai-input exists
    const aiInput = document.getElementById('ai-input');
    console.log('\n3. #ai-input element:', !!aiInput);
    if (aiInput) {
        console.log('   - Visible?', aiInput.offsetHeight > 0);
        console.log('   - Parent:', aiInput.parentElement?.id);
    }
    
    // 4. Check for center chat input
    const centerChat = document.getElementById('center-chat-input');
    console.log('\n4. #center-chat-input element:', !!centerChat);
    if (centerChat) {
        console.log('   - Visible?', centerChat.offsetHeight > 0);
    }
    
    // 5. Check tab content panels
    const panels = document.querySelectorAll('.tab-content-panel');
    console.log('\n5. Tab content panels found:', panels.length);
    panels.forEach((panel, i) => {
        const visible = panel.style.display !== 'none';
        console.log(`   Panel ${i+1}: ${panel.id} - Visible: ${visible}`);
    });
    
    // 6. Check Monaco container
    const monaco = document.getElementById('monaco-container');
    console.log('\n6. Monaco container:', !!monaco);
    if (monaco) {
        console.log('   - Display:', monaco.style.display || 'default');
    }
    
    console.log('\n========== END DIAGNOSTIC ==========\n');
    
    // Offer to test opening a chat tab
    console.log('💡 To test: Run `window.tabSystem.openChatTab()` in console');
    
}, 3000);

