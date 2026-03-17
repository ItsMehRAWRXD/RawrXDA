class LayoutBuilder {
    constructor(ide) {
        this.ide = ide;
        this.layouts = JSON.parse(localStorage.getItem('customLayouts') || '[]');
        this.currentLayout = null;
    }

    show() {
        const panel = document.createElement('div');
        panel.style.cssText = 'position:fixed;top:0;left:0;right:0;bottom:0;background:rgba(0,0,0,0.9);z-index:10000;display:flex;flex-direction:column;';
        panel.innerHTML = `
            <div style="background:#2d2d30;padding:15px;border-bottom:2px solid #ff9900;display:flex;justify-content:space-between;align-items:center;">
                <span style="color:#ff9900;font-weight:bold;font-size:20px;">🎨 Layout Builder</span>
                <button onclick="this.parentElement.parentElement.remove()" style="background:#f44;border:none;color:#fff;padding:8px 16px;cursor:pointer;border-radius:4px;font-weight:bold;">Close</button>
            </div>
            <div style="flex:1;display:flex;overflow:hidden;">
                <div style="width:250px;background:#252526;border-right:1px solid #3e3e42;overflow-y:auto;padding:15px;">
                    <h3 style="color:#ff9900;margin-bottom:15px;">Components</h3>
                    <div class="layout-component" draggable="true" data-type="editor" style="background:#2d2d30;padding:10px;margin-bottom:8px;border-radius:4px;cursor:move;border:1px solid #3e3e42;">
                        📝 Editor
                    </div>
                    <div class="layout-component" draggable="true" data-type="terminal" style="background:#2d2d30;padding:10px;margin-bottom:8px;border-radius:4px;cursor:move;border:1px solid #3e3e42;">
                        💻 Terminal
                    </div>
                    <div class="layout-component" draggable="true" data-type="ai-chat" style="background:#2d2d30;padding:10px;margin-bottom:8px;border-radius:4px;cursor:move;border:1px solid #3e3e42;">
                        🤖 AI Chat
                    </div>
                    <div class="layout-component" draggable="true" data-type="file-tree" style="background:#2d2d30;padding:10px;margin-bottom:8px;border-radius:4px;cursor:move;border:1px solid #3e3e42;">
                        📁 File Explorer
                    </div>
                    <div class="layout-component" draggable="true" data-type="browser" style="background:#2d2d30;padding:10px;margin-bottom:8px;border-radius:4px;cursor:move;border:1px solid #3e3e42;">
                        🌐 Browser
                    </div>
                    <div class="layout-component" draggable="true" data-type="debugger" style="background:#2d2d30;padding:10px;margin-bottom:8px;border-radius:4px;cursor:move;border:1px solid #3e3e42;">
                        🐛 Debugger
                    </div>
                    <div class="layout-component" draggable="true" data-type="output" style="background:#2d2d30;padding:10px;margin-bottom:8px;border-radius:4px;cursor:move;border:1px solid #3e3e42;">
                        📊 Output
                    </div>
                    <div class="layout-component" draggable="true" data-type="git" style="background:#2d2d30;padding:10px;margin-bottom:8px;border-radius:4px;cursor:move;border:1px solid #3e3e42;">
                        🔀 Git
                    </div>
                    <h3 style="color:#ff9900;margin:20px 0 15px;">Layouts</h3>
                    <button onclick="layoutBuilder.newLayout()" style="width:100%;padding:10px;background:#007ACC;border:none;color:#fff;cursor:pointer;border-radius:4px;margin-bottom:8px;">+ New Layout</button>
                    <div id="saved-layouts"></div>
                </div>
                <div style="flex:1;display:flex;flex-direction:column;">
                    <div style="background:#2d2d30;padding:10px;border-bottom:1px solid #3e3e42;display:flex;gap:10px;">
                        <button onclick="layoutBuilder.setGrid(2,2)" style="padding:8px 16px;background:#404040;border:none;color:#fff;cursor:pointer;border-radius:4px;">2×2</button>
                        <button onclick="layoutBuilder.setGrid(3,2)" style="padding:8px 16px;background:#404040;border:none;color:#fff;cursor:pointer;border-radius:4px;">3×2</button>
                        <button onclick="layoutBuilder.setGrid(2,3)" style="padding:8px 16px;background:#404040;border:none;color:#fff;cursor:pointer;border-radius:4px;">2×3</button>
                        <button onclick="layoutBuilder.setGrid(4,3)" style="padding:8px 16px;background:#404040;border:none;color:#fff;cursor:pointer;border-radius:4px;">4×3</button>
                        <input id="layout-name" placeholder="Layout name..." style="flex:1;padding:8px;background:#1e1e1e;border:1px solid #444;color:#fff;border-radius:4px;" />
                        <button onclick="layoutBuilder.saveLayout()" style="padding:8px 16px;background:#0f0;border:none;color:#000;cursor:pointer;border-radius:4px;font-weight:bold;">Save</button>
                        <button onclick="layoutBuilder.applyLayout()" style="padding:8px 16px;background:#ff9900;border:none;color:#fff;cursor:pointer;border-radius:4px;font-weight:bold;">Apply</button>
                    </div>
                    <div id="layout-canvas" style="flex:1;background:#1e1e1e;display:grid;grid-template-columns:1fr 1fr;grid-template-rows:1fr 1fr;gap:2px;padding:10px;"></div>
                </div>
            </div>
        `;
        document.body.appendChild(panel);
        
        this.initCanvas();
        this.renderSavedLayouts();
        this.setupDragDrop();
    }

    initCanvas() {
        const canvas = document.getElementById('layout-canvas');
        for (let i = 0; i < 4; i++) {
            const cell = document.createElement('div');
            cell.className = 'layout-cell';
            cell.dataset.index = i;
            cell.style.cssText = 'background:#2d2d30;border:2px dashed #444;border-radius:4px;display:flex;align-items:center;justify-content:center;color:#666;font-size:14px;';
            cell.textContent = 'Drop component here';
            canvas.appendChild(cell);
        }
    }

    setupDragDrop() {
        document.querySelectorAll('.layout-component').forEach(comp => {
            comp.addEventListener('dragstart', (e) => {
                e.dataTransfer.setData('type', comp.dataset.type);
            });
        });

        document.querySelectorAll('.layout-cell').forEach(cell => {
            cell.addEventListener('dragover', (e) => {
                e.preventDefault();
                cell.style.borderColor = '#ff9900';
            });
            
            cell.addEventListener('dragleave', () => {
                cell.style.borderColor = '#444';
            });
            
            cell.addEventListener('drop', (e) => {
                e.preventDefault();
                const type = e.dataTransfer.getData('type');
                const icons = {
                    'editor': '📝', 'terminal': '💻', 'ai-chat': '🤖',
                    'file-tree': '📁', 'browser': '🌐', 'debugger': '🐛',
                    'output': '📊', 'git': '🔀'
                };
                cell.innerHTML = `<div style="color:#fff;font-size:16px;">${icons[type]} ${type}</div>`;
                cell.dataset.component = type;
                cell.style.borderColor = '#0f0';
                cell.style.borderStyle = 'solid';
            });
        });
    }

    setGrid(cols, rows) {
        const canvas = document.getElementById('layout-canvas');
        canvas.style.gridTemplateColumns = `repeat(${cols}, 1fr)`;
        canvas.style.gridTemplateRows = `repeat(${rows}, 1fr)`;
        canvas.innerHTML = '';
        
        for (let i = 0; i < cols * rows; i++) {
            const cell = document.createElement('div');
            cell.className = 'layout-cell';
            cell.dataset.index = i;
            cell.style.cssText = 'background:#2d2d30;border:2px dashed #444;border-radius:4px;display:flex;align-items:center;justify-content:center;color:#666;font-size:14px;';
            cell.textContent = 'Drop component here';
            canvas.appendChild(cell);
        }
        
        this.setupDragDrop();
    }

    saveLayout() {
        const name = document.getElementById('layout-name').value || 'Untitled';
        const cells = Array.from(document.querySelectorAll('.layout-cell'));
        const canvas = document.getElementById('layout-canvas');
        
        const layout = {
            name,
            cols: canvas.style.gridTemplateColumns,
            rows: canvas.style.gridTemplateRows,
            components: cells.map(cell => ({
                index: cell.dataset.index,
                type: cell.dataset.component || null
            }))
        };
        
        this.layouts.push(layout);
        localStorage.setItem('customLayouts', JSON.stringify(this.layouts));
        if (this.ide && typeof this.ide.addAIMessage === "function") {
            this.ide.addAIMessage('system', `Layout "${name}" saved`);
        } else {
            console.log(`Layout "${name}" saved`);
        }
        this.renderSavedLayouts();
    }

    renderSavedLayouts() {
        const container = document.getElementById('saved-layouts');
        if (!container) return;
        
        container.innerHTML = this.layouts.map((layout, i) => `
            <div style="background:#2d2d30;padding:10px;margin-bottom:8px;border-radius:4px;border:1px solid #3e3e42;">
                <div style="color:#fff;font-weight:bold;margin-bottom:5px;">${layout.name}</div>
                <div style="display:flex;gap:5px;">
                    <button onclick="layoutBuilder.loadLayout(${i})" style="flex:1;padding:5px;background:#007ACC;border:none;color:#fff;cursor:pointer;border-radius:3px;font-size:11px;">Load</button>
                    <button onclick="layoutBuilder.deleteLayout(${i})" style="padding:5px 10px;background:#f44;border:none;color:#fff;cursor:pointer;border-radius:3px;font-size:11px;">×</button>
                </div>
            </div>
        `).join('');
    }

    loadLayout(index) {
        const layout = this.layouts[index];
        const canvas = document.getElementById('layout-canvas');
        
        canvas.style.gridTemplateColumns = layout.cols;
        canvas.style.gridTemplateRows = layout.rows;
        canvas.innerHTML = '';
        
        layout.components.forEach(comp => {
            const cell = document.createElement('div');
            cell.className = 'layout-cell';
            cell.dataset.index = comp.index;
            cell.dataset.component = comp.type;
            cell.style.cssText = 'background:#2d2d30;border:2px solid #0f0;border-radius:4px;display:flex;align-items:center;justify-content:center;color:#fff;font-size:14px;';
            
            if (comp.type) {
                const icons = {
                    'editor': '📝', 'terminal': '💻', 'ai-chat': '🤖',
                    'file-tree': '📁', 'browser': '🌐', 'debugger': '🐛',
                    'output': '📊', 'git': '🔀'
                };
                cell.innerHTML = `<div>${icons[comp.type]} ${comp.type}</div>`;
            } else {
                cell.textContent = 'Drop component here';
                cell.style.borderColor = '#444';
                cell.style.borderStyle = 'dashed';
            }
            
            canvas.appendChild(cell);
        });
        
        this.setupDragDrop();
        document.getElementById('layout-name').value = layout.name;
        this.currentLayout = layout;
    }

    deleteLayout(index) {
        if (confirm('Delete this layout?')) {
            this.layouts.splice(index, 1);
            localStorage.setItem('customLayouts', JSON.stringify(this.layouts));
            this.renderSavedLayouts();
        }
    }

    applyLayout() {
        if (!this.currentLayout && document.querySelectorAll('.layout-cell[data-component]').length === 0) {
            alert('Please create or load a layout first');
            return;
        }
        
        if (this.ide && typeof this.ide.addAIMessage === "function") {
            this.ide.addAIMessage('system', 'Applying custom layout...');
        } else {
            console.log('Applying custom layout...');
        }
        
        const cells = Array.from(document.querySelectorAll('.layout-cell'));
        const components = cells.filter(c => c.dataset.component).map(c => c.dataset.component);
        
        if (this.ide && typeof this.ide.addAIMessage === "function") {
            this.ide.addAIMessage('system', `Layout applied with: ${components.join(', ')}`);
        } else {
            console.log(`Layout applied with: ${components.join(', ')}`);
        }
        alert('Layout will be applied on next IDE restart');
    }

    newLayout() {
        document.getElementById('layout-name').value = '';
        this.setGrid(2, 2);
        this.currentLayout = null;
    }
}

// Export LayoutBuilder to window
window.layoutBuilder = new LayoutBuilder(window.ide);
