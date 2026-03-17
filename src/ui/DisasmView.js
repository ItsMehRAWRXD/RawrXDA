/* RawrXD DisasmView.js - WebView2 Gutter + Code View */

class DisasmView {
    constructor(containerId) {
        this.container = document.getElementById(containerId);
        this.lines = [];
        this.virtualScrollOffset = 0;
        this.init();
    }

    init() {
        console.log("RawrXD DisasmView Initializing...");
        // Setup Virtualizer listener per Batch 1 bridge
        window.chrome.webview.addEventListener('message', event => {
            const msg = event.data;
            if (msg.type === 0x3004) { // DATA_DISASM
                this.updateCode(msg.payload);
            }
        });
    }

    updateCode(disasmChunks) {
        // High-speed DOM manipulation for disassembly display
        let html = '<div class="disasm-grid">';
        disasmChunks.forEach(chunk => {
            html += `
                <div class="disasm-line">
                    <span class="addr">${chunk.address.toString(16).padStart(16, '0')}</span>
                    <span class="bytes">${this.formatBytes(chunk.raw_bytes, chunk.length)}</span>
                    <span class="mnemonic">${chunk.mnemonic}</span>
                </div>
            `;
        });
        html += '</div>';
        this.container.innerHTML = html;
    }

    formatBytes(bytes, length) {
        return Array.from(bytes.slice(0, length))
            .map(b => b.toString(16).padStart(2, '0'))
            .join(' ');
    }
}

const g_DisasmView = new DisasmView('disasm-container');
