// SovereignDashboard.js - Final v14.7 Glue
// Manages the UI components and IPC communication

const RawrXD = {
    // IPC Bridge to C++/MASM
    postMessage: (msg) => {
        if (window.chrome && window.chrome.webview) {
            window.chrome.webview.postMessage(msg);
        }
    },

    // Component Registry
    components: {
        modules: null,
        disasm: null,
        imports: null
    },

    init: () => {
        console.log("RawrXD Sovereign Dashboard Initializing...");
        
        // Listen for data from Native
        window.chrome.webview.addEventListener('message', event => {
            RawrXD.handleNativeMessage(event.data);
        });

        // Request initial state
        RawrXD.postMessage({ type: "REQ_MODULES" });
    },

    handleNativeMessage: (data) => {
        switch(data.type) {
            case "DATA_DISASM":
                if(RawrXD.components.disasm) RawrXD.components.disasm.update(data.payload);
                break;
            case "DATA_MODULES":
                if(RawrXD.components.modules) RawrXD.components.modules.update(data.payload);
                break;
            case "DATA_SYMBOL":
                console.log("Symbol Resolved:", data.payload);
                break;
        }
    }
};

document.addEventListener('DOMContentLoaded', RawrXD.init);
