/**
 * Monaco Bootstrap Test - Verify CSS and AMD loader
 */

console.log('[Monaco Test] Starting bootstrap verification...');

// Test 1: Check if CSS file exists
function testCSSFile() {
    const cssPath = './node_modules/monaco-editor/min/vs/editor/editor.main.css';
    
    fetch(cssPath)
        .then(response => {
            if (response.ok) {
                console.log('[Monaco Test] ✅ CSS file accessible:', response.status);
            } else {
                console.error('[Monaco Test] ❌ CSS file failed:', response.status);
            }
        })
        .catch(error => {
            console.error('[Monaco Test] ❌ CSS fetch error:', error);
        });
}

// Test 2: Check AMD loader
function testAMDLoader() {
    if (typeof require !== 'undefined' && require.config) {
        console.log('[Monaco Test] ✅ AMD loader available');
        
        // Configure Monaco paths
        require.config({
            paths: {
                'vs': './node_modules/monaco-editor/min/vs'
            }
        });
        
        // Try to load Monaco
        require(['vs/editor/editor.main'], function() {
            console.log('[Monaco Test] ✅ Monaco loaded successfully');
            
            if (typeof monaco !== 'undefined') {
                console.log('[Monaco Test] ✅ Monaco global available');
                
                // Try to create editor
                const container = document.createElement('div');
                container.style.width = '100px';
                container.style.height = '100px';
                document.body.appendChild(container);
                
                try {
                    const editor = monaco.editor.create(container, {
                        value: 'console.log("test");',
                        language: 'javascript'
                    });
                    console.log('[Monaco Test] ✅ Editor instance created');
                    editor.dispose();
                    container.remove();
                } catch (error) {
                    console.error('[Monaco Test] ❌ Editor creation failed:', error);
                }
            } else {
                console.error('[Monaco Test] ❌ Monaco global not available');
            }
        }, function(error) {
            console.error('[Monaco Test] ❌ Monaco load failed:', error);
        });
    } else {
        console.error('[Monaco Test] ❌ AMD loader not available');
    }
}

// Run tests
testCSSFile();
// Wait for Monaco AMD initialization to complete (increased from 1000ms to 2000ms)
setTimeout(testAMDLoader, 2000);

// Export for manual testing
window.monacoBootstrapTest = {
    testCSS: testCSSFile,
    testAMD: testAMDLoader
};