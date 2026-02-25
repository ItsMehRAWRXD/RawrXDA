// Universal HTML Layout Fix Script
// Run this in browser console to fix any HTML file with layout issues

(function() {
    console.log('🔧 Applying universal layout fix...');
    
    // Force body constraints
    document.body.style.width = '100vw';
    document.body.style.height = '100vh';
    document.body.style.maxWidth = '100vw';
    document.body.style.maxHeight = '100vh';
    document.body.style.overflow = 'hidden';
    document.body.style.boxSizing = 'border-box';
    document.body.style.margin = '0';
    document.body.style.padding = '0';
    
    // Force html constraints
    document.documentElement.style.width = '100%';
    document.documentElement.style.height = '100%';
    document.documentElement.style.maxWidth = '100vw';
    document.documentElement.style.maxHeight = '100vh';
    document.documentElement.style.overflow = 'hidden';
    
    // Fix all elements
    const allElements = document.querySelectorAll('*');
    allElements.forEach(el => {
        el.style.maxWidth = '100vw';
        el.style.maxHeight = '100vh';
        el.style.boxSizing = 'border-box';
    });
    
    // Fix common containers
    const containers = document.querySelectorAll('.container, .panel, .section, .main, .content, #left, #right, #mid, #left-panel, #right-panel');
    containers.forEach(container => {
        container.style.maxWidth = '100%';
        container.style.overflow = 'hidden';
    });
    
    // Fix textareas and inputs
    const inputs = document.querySelectorAll('textarea, input');
    inputs.forEach(input => {
        input.style.maxWidth = '100%';
        input.style.boxSizing = 'border-box';
    });
    
    console.log('✅ Layout fix applied successfully!');
    console.log('Body dimensions:', document.body.offsetWidth + 'x' + document.body.offsetHeight);
})();
