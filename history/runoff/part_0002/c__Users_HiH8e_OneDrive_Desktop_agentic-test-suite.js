// ========================================================
// BIGDADDYG AGENTIC CAPABILITY TEST SUITE
// ========================================================
// Drop this into your browser console to test all capabilities

(async function runAgenticTest() {
    console.log("=== 🚀 AGENTIC CAPABILITY TEST STARTED ===");
    console.log("Testing all IDE capabilities...\n");

    const results = {};

    // ========================================================
    // 1. Test File System Access API (Browser)
    // ========================================================
    console.log("📁 Testing File System Access...");
    try {
        if ('showDirectoryPicker' in window) {
            results.fileSystemAPI = "✅ OK - Available";
        } else {
            results.fileSystemAPI = "⚠️ NOT AVAILABLE - Use Chrome/Edge";
        }
    } catch(e) { 
        results.fileSystemAPI = "❌ FAIL: " + e.message; 
    }

    // ========================================================
    // 2. Test OPFS (Origin Private File System)
    // ========================================================
    console.log("💾 Testing OPFS...");
    try {
        if ('storage' in navigator && 'getDirectory' in navigator.storage) {
            const root = await navigator.storage.getDirectory();
            const testFile = await root.getFileHandle('test.txt', { create: true });
            const writable = await testFile.createWritable();
            await writable.write('OPFS_TEST');
            await writable.close();
            
            const file = await testFile.getFile();
            const content = await file.text();
            
            results.opfs = content === 'OPFS_TEST' ? "✅ OK - Read/Write working" : "❌ FAIL - Content mismatch";
        } else {
            results.opfs = "❌ NOT SUPPORTED - Use Chrome 102+";
        }
    } catch(e) { 
        results.opfs = "❌ FAIL: " + e.message; 
    }

    // ========================================================
    // 3. Test LocalStorage
    // ========================================================
    console.log("🗄️ Testing LocalStorage...");
    try {
        localStorage.setItem('agentic_test', 'OK');
        const val = localStorage.getItem('agentic_test');
        localStorage.removeItem('agentic_test');
        results.localStorage = val === 'OK' ? "✅ OK" : "❌ FAIL";
    } catch(e) { 
        results.localStorage = "❌ FAIL: " + e.message; 
    }

    // ========================================================
    // 4. Test IndexedDB
    // ========================================================
    console.log("📊 Testing IndexedDB...");
    try {
        const dbTest = indexedDB.open('agentic_test_db', 1);
        await new Promise((resolve, reject) => {
            dbTest.onsuccess = () => {
                dbTest.result.close();
                indexedDB.deleteDatabase('agentic_test_db');
                resolve();
            };
            dbTest.onerror = reject;
        });
        results.indexedDB = "✅ OK";
    } catch(e) { 
        results.indexedDB = "❌ FAIL: " + e.message; 
    }

    // ========================================================
    // 5. Test Backend File API (if available)
    // ========================================================
    console.log("📂 Testing Backend File API...");
    try {
        const r = await fetch("/fs/read?path=index.html", { 
            method: 'GET',
            signal: AbortSignal.timeout(2000)
        });
        if (r.ok) {
            const data = await r.json();
            results.backendFileAPI = data.success ? "✅ OK" : "⚠️ AVAILABLE but failed";
        } else {
            results.backendFileAPI = `⚠️ HTTP ${r.status}`;
        }
    } catch(e) { 
        results.backendFileAPI = "❌ NOT RUNNING - Optional"; 
    }

    // ========================================================
    // 6. Test Backend Terminal API
    // ========================================================
    console.log("💻 Testing Backend Terminal API...");
    try {
        const c = await fetch("/terminal/run", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ cmd: "echo agentic_terminal_test" }),
            signal: AbortSignal.timeout(2000)
        });
        if (c.ok) {
            const data = await c.json();
            results.backendTerminal = data.output?.includes("agentic_terminal_test") ? "✅ OK" : "⚠️ AVAILABLE but failed";
        } else {
            results.backendTerminal = `⚠️ HTTP ${c.status}`;
        }
    } catch(e) { 
        results.backendTerminal = "❌ NOT RUNNING - Optional"; 
    }

    // ========================================================
    // 7. Test Clang Compiler Access
    // ========================================================
    console.log("🔧 Testing Clang Compiler...");
    try {
        const clang = await fetch("/terminal/run", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ cmd: "clang --version" }),
            signal: AbortSignal.timeout(2000)
        });
        if (clang.ok) {
            const data = await clang.json();
            results.clang = data.output ? "✅ OK - Clang available" : "⚠️ Terminal OK, Clang missing";
        } else {
            results.clang = "❌ Backend terminal not running";
        }
    } catch(e) { 
        results.clang = "❌ NOT AVAILABLE"; 
    }

    // ========================================================
    // 8. Test C Compilation & Execution
    // ========================================================
    console.log("⚙️ Testing C Compilation...");
    try {
        const code = `#include <stdio.h>\nint main(){ printf("C_WORKS"); return 0; }`;
        
        // Write test file
        await fetch("/fs/write", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ path: "test_agentic.c", content: code }),
            signal: AbortSignal.timeout(2000)
        });

        // Compile
        const build = await fetch("/terminal/run", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ cmd: "clang test_agentic.c -o test_agentic" }),
            signal: AbortSignal.timeout(5000)
        });

        // Execute
        const exec = await fetch("/terminal/run", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ cmd: "./test_agentic || .\\test_agentic.exe" }),
            signal: AbortSignal.timeout(2000)
        });

        if (exec.ok) {
            const data = await exec.json();
            results.cCompilation = data.output?.includes("C_WORKS") ? "✅ OK - Full pipeline working" : "⚠️ Compile OK, exec failed";
        } else {
            results.cCompilation = "❌ FAIL";
        }
    } catch(e) { 
        results.cCompilation = "❌ NOT AVAILABLE"; 
    }

    // ========================================================
    // 9. Test Browser Navigator APIs
    // ========================================================
    console.log("🌐 Testing Browser APIs...");
    try {
        const apis = {
            navigator: !!window.navigator,
            clipboard: !!navigator.clipboard,
            mediaDevices: !!navigator.mediaDevices,
            geolocation: !!navigator.geolocation,
            serviceWorker: !!navigator.serviceWorker
        };
        const availableCount = Object.values(apis).filter(Boolean).length;
        results.browserAPIs = `✅ ${availableCount}/5 APIs available`;
    } catch(e) { 
        results.browserAPIs = "❌ FAIL"; 
    }

    // ========================================================
    // 10. Test Local AI (Ollama)
    // ========================================================
    console.log("🤖 Testing Ollama AI...");
    try {
        const o = await fetch("http://localhost:11434/api/tags", {
            signal: AbortSignal.timeout(2000)
        });
        if (o.ok) {
            const data = await o.json();
            results.ollama = Array.isArray(data.models) && data.models.length > 0 
                ? `✅ OK - ${data.models.length} models available` 
                : "⚠️ Running but no models";
        } else {
            results.ollama = "❌ HTTP " + o.status;
        }
    } catch(e) { 
        results.ollama = "❌ NOT RUNNING - Install Ollama"; 
    }

    // ========================================================
    // 11. Test WebSocket Support
    // ========================================================
    console.log("🔌 Testing WebSocket...");
    try {
        results.webSocket = typeof WebSocket !== 'undefined' ? "✅ OK" : "❌ NOT SUPPORTED";
    } catch(e) { 
        results.webSocket = "❌ FAIL"; 
    }

    // ========================================================
    // 12. Test Web Workers
    // ========================================================
    console.log("👷 Testing Web Workers...");
    try {
        results.webWorkers = typeof Worker !== 'undefined' ? "✅ OK" : "❌ NOT SUPPORTED";
    } catch(e) { 
        results.webWorkers = "❌ FAIL"; 
    }

    // ========================================================
    // 13. Test Monaco Editor (if present)
    // ========================================================
    console.log("📝 Testing Monaco Editor...");
    try {
        results.monacoEditor = typeof monaco !== 'undefined' ? "✅ OK - Monaco loaded" : "⚠️ Not loaded yet";
    } catch(e) { 
        results.monacoEditor = "⚠️ Not loaded"; 
    }

    // ========================================================
    // 14. Test Extension Manager
    // ========================================================
    console.log("📦 Testing Extension Manager...");
    try {
        results.extensionManager = typeof window.extensionManager !== 'undefined' 
            ? `✅ OK - ${window.extensionManager.extensions.size} extensions` 
            : "❌ NOT INITIALIZED";
    } catch(e) { 
        results.extensionManager = "❌ FAIL: " + e.message; 
    }

    // ========================================================
    // 15. Test Self-Debug Capture
    // ========================================================
    console.log("🐛 Testing Self-Debug System...");
    try {
        const originalError = console.error;
        let captured = false;
        console.error = function() {
            captured = true;
            originalError.apply(console, arguments);
        };
        console.error("SELF_DEBUG_TEST");
        console.error = originalError;
        results.selfDebug = captured ? "✅ OK - Console intercepted" : "❌ FAIL";
    } catch(e) { 
        results.selfDebug = "❌ FAIL"; 
    }

    // ========================================================
    // DISPLAY RESULTS
    // ========================================================
    console.log("\n=== 📊 AGENTIC TEST RESULTS ===\n");
    console.table(results);

    // Calculate score
    const total = Object.keys(results).length;
    const passed = Object.values(results).filter(v => v.includes('✅')).length;
    const percentage = Math.round((passed / total) * 100);

    console.log(`\n🎯 SCORE: ${passed}/${total} (${percentage}%)`);

    if (percentage === 100) {
        console.log("🎉 PERFECT! Your IDE has FULL Cursor++ capabilities!");
    } else if (percentage >= 70) {
        console.log("✅ GOOD! Most features working. Check warnings for improvements.");
    } else if (percentage >= 50) {
        console.log("⚠️ PARTIAL. Core features work but missing advanced capabilities.");
    } else {
        console.log("❌ NEEDS WORK. Many features missing. See detailed report below.");
    }

    // ========================================================
    // GENERATE RECOMMENDATIONS
    // ========================================================
    console.log("\n=== 💡 RECOMMENDATIONS ===\n");
    
    const missing = [];
    
    if (!results.fileSystemAPI.includes('✅')) {
        missing.push("• Use Chrome/Edge for File System Access API support");
    }
    if (!results.opfs.includes('✅')) {
        missing.push("• OPFS not available - upgrade browser or enable flags");
    }
    if (!results.backendFileAPI.includes('✅')) {
        missing.push("• Backend file API not running - start server for advanced file ops");
    }
    if (!results.backendTerminal.includes('✅')) {
        missing.push("• Backend terminal not running - start server for terminal access");
    }
    if (!results.clang.includes('✅')) {
        missing.push("• Install Clang/LLVM for C/C++ compilation support");
    }
    if (!results.ollama.includes('✅')) {
        missing.push("• Install Ollama (https://ollama.ai) for local AI models");
    }
    if (!results.extensionManager.includes('✅')) {
        missing.push("• Initialize Extension Manager for plugin support");
    }

    if (missing.length > 0) {
        console.log("To achieve 100% capability:\n");
        missing.forEach(m => console.log(m));
    } else {
        console.log("🎉 No recommendations - everything is perfect!");
    }

    console.log("\n=== ✅ TEST COMPLETE ===\n");
    
    return { results, score: { passed, total, percentage } };
})();
