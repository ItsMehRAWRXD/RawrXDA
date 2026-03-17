/**
 * BigDaddyG IDE - Hot-Swappable Model Plugin System
 * Switch between AI models instantly with hotkeys
 */

// ============================================================================
// MODEL REGISTRY (PLUGINABLE)
// ============================================================================

const ModelRegistry = {
    // Built-in BigDaddyG variants
    'BigDaddyG:Latest': {
        name: 'BigDaddyG Latest',
        icon: '🌌',
        type: 'built-in',
        specialization: 'General purpose',
        hotkey: 'Ctrl+Shift+1',
        color: '#00d4ff',
        description: 'Trained on 200K lines ASM/Security/Encryption',
        context_size: 1000000,
        temperature: 0.7
    },
    'BigDaddyG:Code': {
        name: 'BigDaddyG Code',
        icon: '💻',
        type: 'built-in',
        specialization: 'Code generation',
        hotkey: 'Ctrl+Shift+2',
        color: '#00ff88',
        description: 'Optimized for code writing and debugging',
        context_size: 1000000,
        temperature: 0.5
    },
    'BigDaddyG:Debug': {
        name: 'BigDaddyG Debug',
        icon: '🐛',
        type: 'built-in',
        specialization: 'Bug fixing',
        hotkey: 'Ctrl+Shift+3',
        color: '#ff6b35',
        description: 'Expert at finding and fixing bugs',
        context_size: 1000000,
        temperature: 0.3
    },
    'BigDaddyG:Crypto': {
        name: 'BigDaddyG Crypto',
        icon: '🔐',
        type: 'built-in',
        specialization: 'Cryptography & Security',
        hotkey: 'Ctrl+Shift+4',
        color: '#a855f7',
        description: 'Specialized in encryption and security',
        context_size: 1000000,
        temperature: 0.6
    },

    // Plugin slots for external models (Ollama, etc.)
    'plugin:1': null,
    'plugin:2': null,
    'plugin:3': null,
    'plugin:4': null,
    'plugin:5': null,
    'plugin:6': null
};

let activeModel = 'BigDaddyG:Latest';
let modelCache = new Map();
let hotkeysRegistered = false;

// ============================================================================
// MODEL HOT-SWAP
// ============================================================================

function swapModel(modelId) {
    const model = ModelRegistry[modelId];

    if (!model) {
        console.error(`[HotSwap] ❌ Model not found: ${modelId}`);
        return false;
    }

    console.log(`[HotSwap] 🔄 Switching to ${model.name}...`);

    const previousModel = activeModel;
    activeModel = modelId;

    // Show swap notification
    showSwapNotification(model, previousModel);

    // Update UI
    updateModelIndicator(model);

    // Log telemetry
    console.log(`[HotSwap] ✅ Swapped: ${previousModel} → ${modelId}`);

    // Trigger event for other components
    document.dispatchEvent(new CustomEvent('model-swapped', {
        detail: { from: previousModel, to: modelId, model: model }
    }));

    return true;
}

function showSwapNotification(model, previousModel) {
    const notification = document.createElement('div');
    notification.style.cssText = `
        position: fixed;
        top: 50%;
        left: 50%;
        transform: translate(-50%, -50%) scale(0);
        background: rgba(10, 10, 30, 0.98);
        backdrop-filter: blur(30px);
        border: 3px solid ${model.color};
        border-radius: 20px;
        padding: 40px;
        z-index: 1000000;
        box-shadow: 0 0 60px ${model.color}80;
        animation: modelSwapNotification 1.5s ease-out forwards;
        text-align: center;
        min-width: 400px;
    `;

    notification.innerHTML = `
        <div style="font-size: 64px; margin-bottom: 15px;">${model.icon}</div>
        <div style="font-size: 28px; font-weight: bold; color: ${model.color}; margin-bottom: 10px;">
            ${model.name}
        </div>
        <div style="font-size: 14px; color: #888; margin-bottom: 15px;">
            ${model.description}
        </div>
        <div style="display: flex; justify-content: center; gap: 20px; font-size: 12px; color: #666;">
            <div>
                <div style="color: var(--cyan);">Specialization</div>
                <div style="color: white; font-weight: bold;">${model.specialization}</div>
            </div>
            <div>
                <div style="color: var(--purple);">Context</div>
                <div style="color: white; font-weight: bold;">${(model.context_size / 1000).toFixed(0)}K</div>
            </div>
            <div>
                <div style="color: var(--orange);">Temp</div>
                <div style="color: white; font-weight: bold;">${model.temperature}</div>
            </div>
        </div>
        <div style="margin-top: 20px; padding: 10px; background: rgba(0,212,255,0.1); border-radius: 8px; font-size: 11px; color: #888;">
            Hotkey: ${model.hotkey}
        </div>
    `;

    // Add animation
    const style = document.createElement('style');
    style.textContent = `
        @keyframes modelSwapNotification {
            0% { transform: translate(-50%, -50%) scale(0) rotate(-180deg); opacity: 0; }
            50% { transform: translate(-50%, -50%) scale(1.1) rotate(10deg); opacity: 1; }
            70% { transform: translate(-50%, -50%) scale(0.95) rotate(-5deg); }
            100% { transform: translate(-50%, -50%) scale(1) rotate(0deg); opacity: 1; }
        }
    `;
    document.head.appendChild(style);

    document.body.appendChild(notification);

    setTimeout(() => {
        notification.style.animation = 'modelSwapOut 0.5s ease-in forwards';
        setTimeout(() => notification.remove(), 500);
    }, 1500);

    // Add disappear animation
    const styleOut = document.createElement('style');
    styleOut.textContent = `
        @keyframes modelSwapOut {
            from { opacity: 1; transform: translate(-50%, -50%) scale(1); }
            to { opacity: 0; transform: translate(-50%, -50%) scale(0.8); }
        }
    `;
    document.head.appendChild(styleOut);
}

// ============================================================================
// MODEL INDICATOR (ALWAYS VISIBLE)
// ============================================================================

function createModelIndicator() {
    const indicator = document.createElement('div');
    indicator.id = 'model-indicator';
    indicator.style.cssText = `
        position: fixed;
        top: 80px;
        right: 20px;
        background: rgba(10, 10, 30, 0.95);
        backdrop-filter: blur(20px);
        border: 2px solid var(--cyan);
        border-radius: 12px;
        padding: 15px;
        z-index: 999998;
        cursor: pointer;
        transition: all 0.3s;
        box-shadow: 0 5px 20px rgba(0,212,255,0.4);
        display: flex;
        align-items: center;
        gap: 10px;
    `;

    updateModelIndicator(ModelRegistry[activeModel]);

    // Add browser button
    const browserBtn = document.createElement('button');
    browserBtn.title = 'Open Model Browser (Ctrl+M)';
    browserBtn.style.cssText = `
        background: rgba(0,212,255,0.2);
        border: 1px solid var(--cyan);
        color: var(--cyan);
        padding: 8px 12px;
        border-radius: 8px;
        cursor: pointer;
        font-size: 12px;
        font-weight: bold;
        transition: all 0.3s;
        white-space: nowrap;
    `;
    browserBtn.textContent = '🎯 Browser';
    browserBtn.onclick = (e) => {
        e.stopPropagation();
        if (typeof modelBrowser !== 'undefined' && modelBrowser) {
            modelBrowser.openBrowser();
        }
    };
    browserBtn.onmouseover = () => {
        browserBtn.style.background = 'rgba(0,212,255,0.4)';
        browserBtn.style.boxShadow = '0 0 15px rgba(0,212,255,0.6)';
    };
    browserBtn.onmouseout = () => {
        browserBtn.style.background = 'rgba(0,212,255,0.2)';
        browserBtn.style.boxShadow = 'none';
    };

    indicator.onclick = () => showModelSelector();

    indicator.onmouseenter = () => {
        indicator.style.transform = 'scale(1.05)';
        indicator.style.boxShadow = '0 8px 30px rgba(0,212,255,0.6)';
    };

    indicator.onmouseleave = () => {
        indicator.style.transform = 'scale(1)';
        indicator.style.boxShadow = '0 5px 20px rgba(0,212,255,0.4)';
    };

    indicator.appendChild(browserBtn);
    document.body.appendChild(indicator);
}

function updateModelIndicator(model) {
    const indicator = document.getElementById('model-indicator');
    if (!indicator) return;

    indicator.style.borderColor = model.color;

    // Get existing browser button if it exists
    const browserBtn = indicator.querySelector('button');

    // Clear and rebuild
    indicator.innerHTML = `
        <div style="display: flex; align-items: center; gap: 12px;">
            <div style="font-size: 32px;">${model.icon}</div>
            <div>
                <div style="font-size: 14px; font-weight: bold; color: ${model.color};">
                    ${model.name}
                </div>
                <div style="font-size: 10px; color: #888;">
                    ${model.specialization}
                </div>
                <div style="font-size: 9px; color: #666; margin-top: 3px;">
                    ${model.hotkey}
                </div>
            </div>
        </div>
    `;

    // Recreate browser button
    const newBrowserBtn = document.createElement('button');
    newBrowserBtn.title = 'Open Model Browser (Ctrl+M)';
    newBrowserBtn.style.cssText = `
        background: rgba(0,212,255,0.2);
        border: 1px solid var(--cyan);
        color: var(--cyan);
        padding: 8px 12px;
        border-radius: 8px;
        cursor: pointer;
        font-size: 12px;
        font-weight: bold;
        transition: all 0.3s;
        white-space: nowrap;
    `;
    newBrowserBtn.textContent = '🎯 Browser';
    newBrowserBtn.onclick = (e) => {
        e.stopPropagation();
        if (typeof modelBrowser !== 'undefined' && modelBrowser) {
            modelBrowser.openBrowser();
        }
    };
    newBrowserBtn.onmouseover = () => {
        newBrowserBtn.style.background = 'rgba(0,212,255,0.4)';
        newBrowserBtn.style.boxShadow = '0 0 15px rgba(0,212,255,0.6)';
    };
    newBrowserBtn.onmouseout = () => {
        newBrowserBtn.style.background = 'rgba(0,212,255,0.2)';
        newBrowserBtn.style.boxShadow = 'none';
    };

    indicator.appendChild(newBrowserBtn);
}// ============================================================================
// MODEL SELECTOR (VISUAL SWITCHER)
// ============================================================================

function showModelSelector() {
    // Remove existing selector
    const existing = document.getElementById('model-selector');
    if (existing) {
        existing.remove();
        return;
    }

    const selector = document.createElement('div');
    selector.id = 'model-selector';
    selector.style.cssText = `
        position: fixed;
        top: 50%;
        left: 50%;
        transform: translate(-50%, -50%);
        background: rgba(10, 10, 30, 0.98);
        backdrop-filter: blur(30px);
        border: 3px solid var(--cyan);
        border-radius: 20px;
        padding: 30px;
        z-index: 1000001;
        max-width: 900px;
        max-height: 80vh;
        overflow-y: auto;
        box-shadow: 0 10px 50px rgba(0,212,255,0.6);
    `;

    let html = `
        <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 25px; padding-bottom: 15px; border-bottom: 2px solid var(--cyan);">
            <div>
                <h2 style="color: var(--cyan); margin: 0; font-size: 24px;">🤖 Model Hot-Swap</h2>
                <div style="font-size: 12px; color: #888; margin-top: 5px;">Press hotkey or click to switch models instantly</div>
            </div>
            <button onclick="closeModelSelector()" style="background: var(--red); color: white; border: none; padding: 10px 20px; border-radius: 8px; cursor: pointer; font-weight: bold; font-size: 14px;">✕ Close</button>
        </div>
        
        <div style="display: grid; grid-template-columns: repeat(2, 1fr); gap: 15px; margin-bottom: 25px;">
    `;

    // Built-in models
    Object.keys(ModelRegistry).forEach(key => {
        const model = ModelRegistry[key];
        if (!model || key.startsWith('plugin:')) return;

        const isActive = key === activeModel;

        html += `
            <div onclick="swapModel('${key}')" style="
                padding: 20px;
                background: ${isActive ? `linear-gradient(135deg, ${model.color}33, ${model.color}11)` : 'rgba(0,0,0,0.5)'};
                border: 2px solid ${isActive ? model.color : 'rgba(255,255,255,0.1)'};
                border-radius: 12px;
                cursor: pointer;
                transition: all 0.3s;
                ${isActive ? `box-shadow: 0 0 30px ${model.color}80;` : ''}
            " onmouseover="this.style.transform='scale(1.05)'; this.style.borderColor='${model.color}'" onmouseout="this.style.transform='scale(1)'; this.style.borderColor='${isActive ? model.color : 'rgba(255,255,255,0.1)'}'">
                <div style="display: flex; align-items: center; gap: 15px; margin-bottom: 12px;">
                    <div style="font-size: 40px;">${model.icon}</div>
                    <div>
                        <div style="font-size: 18px; font-weight: bold; color: ${model.color};">
                            ${model.name}
                        </div>
                        <div style="font-size: 11px; color: #888;">
                            ${model.specialization}
                        </div>
                    </div>
                </div>
                <div style="font-size: 11px; color: #666; margin-bottom: 10px;">
                    ${model.description}
                </div>
                <div style="display: flex; justify-content: space-between; align-items: center; padding-top: 10px; border-top: 1px solid rgba(255,255,255,0.1);">
                    <div style="font-size: 10px; color: #888;">
                        <strong style="color: ${model.color};">${model.hotkey}</strong>
                    </div>
                    <div style="font-size: 10px; color: #666;">
                        Temp: ${model.temperature} | Context: ${(model.context_size / 1000).toFixed(0)}K
                    </div>
                </div>
                ${isActive ? `<div style="margin-top: 10px; padding: 8px; background: ${model.color}33; border-radius: 6px; text-align: center; font-size: 11px; color: ${model.color}; font-weight: bold;">✅ ACTIVE</div>` : ''}
            </div>
        `;
    });

    html += `
        </div>
        
        <div style="padding: 20px; background: rgba(0,212,255,0.1); border-radius: 12px; border-left: 4px solid var(--cyan); margin-bottom: 20px;">
            <div style="font-size: 14px; font-weight: bold; color: var(--cyan); margin-bottom: 10px;">
                🔌 Plugin Slots (External Models)
            </div>
            <div style="font-size: 12px; color: #888; margin-bottom: 15px;">
                Add Ollama or custom models to these slots
            </div>
            <div style="display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px;">
    `;

    for (let i = 1; i <= 6; i++) {
        const pluginKey = `plugin:${i}`;
        const plugin = ModelRegistry[pluginKey];
        const hotkey = `Ctrl+Alt+${i}`;

        html += `
            <div onclick="assignPluginSlot('${pluginKey}')" style="
                padding: 15px;
                background: rgba(0,0,0,0.5);
                border: 1px dashed rgba(255,255,255,0.3);
                border-radius: 8px;
                cursor: pointer;
                text-align: center;
                transition: all 0.3s;
            " onmouseover="this.style.borderColor='var(--purple)'; this.style.background='rgba(168,85,247,0.1)'" onmouseout="this.style.borderColor='rgba(255,255,255,0.3)'; this.style.background='rgba(0,0,0,0.5)'">
                ${plugin ? `
                    <div style="font-size: 24px; margin-bottom: 5px;">${plugin.icon}</div>
                    <div style="font-size: 11px; font-weight: bold; color: var(--purple);">${plugin.name}</div>
                    <div style="font-size: 9px; color: #666; margin-top: 5px;">${hotkey}</div>
                ` : `
                    <div style="font-size: 32px; opacity: 0.3; margin-bottom: 5px;">+</div>
                    <div style="font-size: 10px; color: #666;">Empty Slot</div>
                    <div style="font-size: 9px; color: #666; margin-top: 5px;">${hotkey}</div>
                `}
            </div>
        `;
    }

    html += `
            </div>
        </div>
        
        <div style="padding: 15px; background: rgba(255,107,53,0.1); border-radius: 10px; border-left: 4px solid var(--orange); font-size: 12px; color: #ccc;">
            <strong style="color: var(--orange);">💡 Tips:</strong><br>
            • Press hotkey to instant-swap models<br>
            • Hold Ctrl+Shift and press 1-4 for built-in models<br>
            • Hold Ctrl+Alt and press 1-6 for plugin slots<br>
            • Click model indicator (top-right) to open this menu
        </div>
    `;

    selector.innerHTML = html;
    document.body.appendChild(selector);
}

function closeModelSelector() {
    const selector = document.getElementById('model-selector');
    if (selector) selector.remove();
}

// ============================================================================
// PLUGIN MANAGEMENT
// ============================================================================

async function assignPluginSlot(slotKey) {
    console.log(`[HotSwap] 🔌 Assigning plugin to ${slotKey}...`);

    // Show Ollama model picker
    try {
        const response = await fetch('http://localhost:11441/ollama/api/tags');
        const data = await response.json();

        if (data.models && data.models.length > 0) {
            showOllamaModelPicker(slotKey, data.models);
        } else {
            alert('No Ollama models found. Start Ollama with: ollama serve');
        }
    } catch (error) {
        alert('Could not connect to Ollama. Make sure it\'s running.');
    }
}

function showOllamaModelPicker(slotKey, models) {
    const picker = document.createElement('div');
    picker.id = 'ollama-picker';
    picker.style.cssText = `
        position: fixed;
        top: 50%;
        left: 50%;
        transform: translate(-50%, -50%);
        background: rgba(10, 10, 30, 0.98);
        backdrop-filter: blur(30px);
        border: 3px solid var(--purple);
        border-radius: 20px;
        padding: 30px;
        z-index: 1000002;
        max-width: 600px;
        max-height: 70vh;
        overflow-y: auto;
        box-shadow: 0 10px 50px rgba(168,85,247,0.6);
    `;

    let html = `
        <div style="margin-bottom: 20px; padding-bottom: 15px; border-bottom: 2px solid var(--purple);">
            <h3 style="color: var(--purple); margin: 0;">🦙 Select Ollama Model</h3>
            <div style="font-size: 12px; color: #888; margin-top: 5px;">Assign to ${slotKey}</div>
        </div>
        <div style="display: flex; flex-direction: column; gap: 10px;">
    `;

    models.forEach(model => {
        const sizeGB = (model.size / (1024 * 1024 * 1024)).toFixed(2);

        html += `
            <div onclick="assignModelToSlot('${slotKey}', '${model.name}')" style="
                padding: 15px;
                background: rgba(0,0,0,0.5);
                border: 1px solid rgba(168,85,247,0.3);
                border-radius: 10px;
                cursor: pointer;
                transition: all 0.3s;
            " onmouseover="this.style.background='rgba(168,85,247,0.2)'; this.style.borderColor='var(--purple)'" onmouseout="this.style.background='rgba(0,0,0,0.5)'; this.style.borderColor='rgba(168,85,247,0.3)'">
                <div style="display: flex; justify-content: space-between; align-items: center;">
                    <div>
                        <div style="font-size: 14px; font-weight: bold; color: var(--purple);">${model.name}</div>
                        <div style="font-size: 11px; color: #888; margin-top: 3px;">Size: ${sizeGB} GB</div>
                    </div>
                    <div style="font-size: 24px;">🦙</div>
                </div>
            </div>
        `;
    });

    html += `
        </div>
        <button onclick="closeOllamaPicker()" style="width: 100%; margin-top: 15px; padding: 10px; background: var(--red); color: white; border: none; border-radius: 8px; cursor: pointer; font-weight: bold;">Cancel</button>
    `;

    picker.innerHTML = html;
    document.body.appendChild(picker);
}

function assignModelToSlot(slotKey, modelName) {
    const slotNum = slotKey.split(':')[1];

    ModelRegistry[slotKey] = {
        name: modelName,
        icon: '🦙',
        type: 'ollama',
        specialization: 'External Model',
        hotkey: `Ctrl+Alt+${slotNum}`,
        color: '#a855f7',
        description: `Ollama model: ${modelName}`,
        context_size: 4096,
        temperature: 0.7
    };

    console.log(`[HotSwap] ✅ Assigned ${modelName} to ${slotKey}`);

    closeOllamaPicker();
    closeModelSelector();
    showModelSelector(); // Refresh

    // Show success notification
    const notification = document.createElement('div');
    notification.style.cssText = `
        position: fixed;
        top: 100px;
        right: 20px;
        background: rgba(10, 10, 30, 0.98);
        backdrop-filter: blur(20px);
        border: 2px solid var(--purple);
        border-radius: 12px;
        padding: 20px;
        z-index: 1000003;
        box-shadow: 0 5px 25px rgba(168,85,247,0.6);
    `;

    notification.innerHTML = `
        <div style="font-size: 14px; font-weight: bold; color: var(--purple); margin-bottom: 5px;">✅ Plugin Assigned</div>
        <div style="font-size: 12px; color: #ccc;">${modelName} → ${slotKey}</div>
        <div style="font-size: 11px; color: #666; margin-top: 5px;">Hotkey: Ctrl+Alt+${slotNum}</div>
    `;

    document.body.appendChild(notification);
    setTimeout(() => notification.remove(), 3000);
}

function closeOllamaPicker() {
    const picker = document.getElementById('ollama-picker');
    if (picker) picker.remove();
}

// ============================================================================
// HOTKEY REGISTRATION
// ============================================================================

function registerHotkeys() {
    if (hotkeysRegistered) return;

    document.addEventListener('keydown', (e) => {
        // Built-in models: Ctrl+Shift+1-4
        if (e.ctrlKey && e.shiftKey && !e.altKey) {
            const models = Object.keys(ModelRegistry).filter(k => !k.startsWith('plugin:'));
            const index = parseInt(e.key) - 1;

            if (index >= 0 && index < models.length) {
                e.preventDefault();
                swapModel(models[index]);
            }
        }

        // Plugin slots: Ctrl+Alt+1-6
        if (e.ctrlKey && e.altKey && !e.shiftKey) {
            const slotNum = parseInt(e.key);

            if (slotNum >= 1 && slotNum <= 6) {
                const slotKey = `plugin:${slotNum}`;
                const model = ModelRegistry[slotKey];

                if (model) {
                    e.preventDefault();
                    swapModel(slotKey);
                }
            }
        }

        // Model selector: Ctrl+M
        if (e.ctrlKey && !e.shiftKey && !e.altKey && e.key === 'm') {
            e.preventDefault();
            showModelSelector();
        }
    });

    hotkeysRegistered = true;
    console.log('[HotSwap] ⌨️ Hotkeys registered');
    console.log('[HotSwap] 💡 Ctrl+Shift+1-4: Built-in models');
    console.log('[HotSwap] 💡 Ctrl+Alt+1-6: Plugin slots');
    console.log('[HotSwap] 💡 Ctrl+M: Open model selector');
}

// ============================================================================
// API FOR OTHER COMPONENTS
// ============================================================================

function getCurrentModel() {
    return { id: activeModel, ...ModelRegistry[activeModel] };
}

function getAvailableModels() {
    return Object.keys(ModelRegistry)
        .filter(k => ModelRegistry[k] !== null)
        .map(k => ({ id: k, ...ModelRegistry[k] }));
}

// ============================================================================
// INITIALIZATION
// ============================================================================

// Wait for DOM
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        createModelIndicator();
        registerHotkeys();
        console.log('[HotSwap] ✅ Model hot-swap system initialized');
        console.log('[HotSwap] 💎 Active model:', activeModel);
    });
} else {
    createModelIndicator();
    registerHotkeys();
    console.log('[HotSwap] ✅ Model hot-swap system initialized');
    console.log('[HotSwap] 💎 Active model:', activeModel);
}

// Export functions
if (typeof module !== 'undefined' && module.exports) {
    module.exports = {
        swapModel,
        getCurrentModel,
        getAvailableModels,
        assignPluginSlot,
        ModelRegistry
    };
}

