/**
 * BigDaddyG IDE - Chameleon Theme System
 * Dynamic text colors with hue/transparency sliders
 * Rainbow cycling and real-time color adjustment
 */

// ============================================================================
// CHAMELEON CONFIGURATION
// ============================================================================

const ChameleonConfig = {
    // Color settings
    baseHue: 180,              // Starting hue (0-360) - Cyan
    saturation: 80,            // Saturation (0-100)
    lightness: 60,             // Lightness (0-100)
    transparency: 1.0,         // Alpha (0-1)
    
    // Animation
    enabled: true,
    animateHue: false,         // Auto-cycle through rainbow
    animationSpeed: 0.5,       // Hue shift speed (degrees per frame)
    rainbowMode: false,        // Each element gets different color
    
    // Target elements
    targets: {
        text: true,
        headings: true,
        links: true,
        code: true,
        buttons: true,
        borders: true
    }
};

let currentHue = ChameleonConfig.baseHue;
let animationFrame = null;

// ============================================================================
// CHAMELEON THEME ENGINE
// ============================================================================

class ChameleonTheme {
    constructor() {
        this.hue = ChameleonConfig.baseHue;
        this.saturation = ChameleonConfig.saturation;
        this.lightness = ChameleonConfig.lightness;
        this.transparency = ChameleonConfig.transparency;
        this.animating = false;
        
        this.init();
    }
    
    init() {
        console.log('[Chameleon] 🦎 Initializing theme engine...');
        
        // Apply initial theme
        this.applyTheme();
        
        // Start animation if enabled
        if (ChameleonConfig.animateHue) {
            this.startAnimation();
        }
        
        console.log('[Chameleon] ✅ Theme engine initialized');
    }
    
    applyTheme() {
        const style = this.generateCSS();
        
        // Remove existing chameleon style
        const existingStyle = document.getElementById('chameleon-style');
        if (existingStyle) {
            existingStyle.remove();
        }
        
        // Add new style
        const styleEl = document.createElement('style');
        styleEl.id = 'chameleon-style';
        styleEl.textContent = style;
        document.head.appendChild(styleEl);
        
        // Update Monaco editor if present
        this.updateMonacoTheme();
    }
    
    getHSLA(hue, opacity = null) {
        const alpha = opacity !== null ? opacity : this.transparency;
        return `hsla(${hue}, ${this.saturation}%, ${this.lightness}%, ${alpha})`;
    }
    
    generateCSS() {
        const baseColor = this.getHSLA(this.hue);
        const color1 = this.getHSLA((this.hue + 30) % 360);
        const color2 = this.getHSLA((this.hue + 60) % 360);
        const color3 = this.getHSLA((this.hue + 90) % 360);
        const color4 = this.getHSLA((this.hue + 120) % 360);
        
        return `
            /* Chameleon Theme - Dynamic Colors */
            
            ${ChameleonConfig.targets.text ? `
            /* Body text */
            body {
                color: ${baseColor} !important;
            }
            
            /* All text elements */
            p, span, div, label, td, th {
                color: ${baseColor};
            }
            ` : ''}
            
            ${ChameleonConfig.targets.headings ? `
            /* Headings with gradient */
            h1, h2, h3, h4, h5, h6 {
                background: linear-gradient(135deg, ${color1}, ${color2});
                -webkit-background-clip: text;
                -webkit-text-fill-color: transparent;
                background-clip: text;
            }
            ` : ''}
            
            ${ChameleonConfig.targets.links ? `
            /* Links */
            a {
                color: ${color2} !important;
                transition: color 0.3s;
            }
            
            a:hover {
                color: ${color3} !important;
            }
            ` : ''}
            
            ${ChameleonConfig.targets.code ? `
            /* Code blocks */
            code, pre {
                color: ${color2};
                border-color: ${this.getHSLA(this.hue, 0.3)};
            }
            
            .monaco-editor .mtk1 {
                color: ${baseColor} !important;
            }
            ` : ''}
            
            ${ChameleonConfig.targets.buttons ? `
            /* Buttons */
            button {
                border-color: ${this.getHSLA(this.hue, 0.5)} !important;
                color: ${baseColor} !important;
            }
            
            button:hover {
                background: ${this.getHSLA(this.hue, 0.2)} !important;
                box-shadow: 0 5px 20px ${this.getHSLA(this.hue, 0.4)} !important;
            }
            ` : ''}
            
            ${ChameleonConfig.targets.borders ? `
            /* Borders */
            .tree-node, .editor-tab, .bottom-tab {
                border-color: ${this.getHSLA(this.hue, 0.3)} !important;
            }
            ` : ''}
            
            /* CSS Custom Properties */
            :root {
                --chameleon-hue: ${this.hue};
                --chameleon-primary: ${baseColor};
                --chameleon-secondary: ${color1};
                --chameleon-accent: ${color2};
                --chameleon-highlight: ${color3};
            }
        `;
    }
    
    updateMonacoTheme() {
        // If Monaco editor is loaded, update its colors
        if (typeof monaco !== 'undefined') {
            monaco.editor.defineTheme('chameleon-dark', {
                base: 'vs-dark',
                inherit: true,
                rules: [
                    { token: '', foreground: this.hue.toString(16).padStart(6, '0') }
                ],
                colors: {
                    'editor.foreground': this.getHSLA(this.hue),
                    'editor.selectionBackground': this.getHSLA(this.hue, 0.3)
                }
            });
            
            if (window.editor) {
                monaco.editor.setTheme('chameleon-dark');
            }
        }
    }
    
    setHue(hue) {
        this.hue = Math.max(0, Math.min(360, hue));
        currentHue = this.hue;
        this.applyTheme();
    }
    
    setTransparency(alpha) {
        this.transparency = Math.max(0, Math.min(1, alpha));
        this.applyTheme();
    }
    
    setSaturation(sat) {
        this.saturation = Math.max(0, Math.min(100, sat));
        this.applyTheme();
    }
    
    setLightness(light) {
        this.lightness = Math.max(0, Math.min(100, light));
        this.applyTheme();
    }
    
    startAnimation() {
        if (this.animating) return;
        
        this.animating = true;
        
        const animate = () => {
            if (!this.animating) return;
            
            this.hue = (this.hue + ChameleonConfig.animationSpeed) % 360;
            currentHue = this.hue;
            this.applyTheme();
            
            animationFrame = requestAnimationFrame(animate);
        };
        
        animate();
        console.log('[Chameleon] 🌈 Animation started');
    }
    
    stopAnimation() {
        this.animating = false;
        if (animationFrame) {
            cancelAnimationFrame(animationFrame);
        }
        console.log('[Chameleon] ⏸️ Animation stopped');
    }
    
    toggleAnimation() {
        if (this.animating) {
            this.stopAnimation();
        } else {
            this.startAnimation();
        }
    }
}

// ============================================================================
// CHAMELEON CONTROL PANEL
// ============================================================================

function createChameleonControls() {
    const controls = document.createElement('div');
    controls.id = 'chameleon-controls';
    controls.style.cssText = `
        position: fixed;
        top: 50%;
        right: 20px;
        transform: translateY(-50%);
        background: rgba(10, 10, 30, 0.95);
        backdrop-filter: blur(30px);
        border: 2px solid var(--cyan);
        border-radius: 15px;
        padding: 20px;
        z-index: 999996;
        min-width: 300px;
        display: none;
        box-shadow: 0 10px 40px rgba(0,212,255,0.5);
    `;
    
    controls.innerHTML = `
        <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px; padding-bottom: 15px; border-bottom: 2px solid var(--cyan);">
            <h3 style="color: var(--cyan); margin: 0; font-size: 18px;">🦎 Chameleon Theme</h3>
            <button onclick="toggleChameleonControls()" style="background: var(--red); color: white; border: none; padding: 6px 12px; border-radius: 6px; cursor: pointer; font-weight: bold;">✕</button>
        </div>
        
        <!-- Color Preview -->
        <div style="margin-bottom: 20px; text-align: center;">
            <div id="color-preview" style="
                width: 100%;
                height: 80px;
                border-radius: 10px;
                background: linear-gradient(135deg, 
                    hsla(${ChameleonConfig.baseHue}, 80%, 60%, ${ChameleonConfig.transparency}),
                    hsla(${(ChameleonConfig.baseHue + 60) % 360}, 80%, 60%, ${ChameleonConfig.transparency})
                );
                border: 2px solid var(--cyan);
                display: flex;
                align-items: center;
                justify-content: center;
                font-size: 24px;
                font-weight: bold;
                text-shadow: 0 2px 10px rgba(0,0,0,0.5);
            ">BigDaddyG IDE</div>
        </div>
        
        <!-- Hue Slider -->
        <div style="margin-bottom: 20px;">
            <label style="color: #888; font-size: 12px; display: flex; justify-content: space-between; margin-bottom: 8px;">
                <span>🎨 Color (Hue)</span>
                <span id="hue-value" style="color: var(--cyan); font-weight: bold;">${ChameleonConfig.baseHue}°</span>
            </label>
            <input type="range" id="hue-slider" min="0" max="360" value="${ChameleonConfig.baseHue}" 
                oninput="updateHue(this.value)"
                style="
                    width: 100%;
                    height: 30px;
                    -webkit-appearance: none;
                    background: linear-gradient(to right, 
                        hsl(0, 80%, 60%),
                        hsl(60, 80%, 60%),
                        hsl(120, 80%, 60%),
                        hsl(180, 80%, 60%),
                        hsl(240, 80%, 60%),
                        hsl(300, 80%, 60%),
                        hsl(360, 80%, 60%)
                    );
                    border-radius: 15px;
                    outline: none;
                    cursor: pointer;
                ">
        </div>
        
        <!-- Transparency Slider -->
        <div style="margin-bottom: 20px;">
            <label style="color: #888; font-size: 12px; display: flex; justify-content: space-between; margin-bottom: 8px;">
                <span>✨ Transparency</span>
                <span id="transparency-value" style="color: var(--cyan); font-weight: bold;">${(ChameleonConfig.transparency * 100).toFixed(0)}%</span>
            </label>
            <input type="range" id="transparency-slider" min="0" max="100" value="${ChameleonConfig.transparency * 100}" 
                oninput="updateTransparency(this.value)"
                style="
                    width: 100%;
                    height: 8px;
                    -webkit-appearance: none;
                    background: linear-gradient(to right, 
                        transparent,
                        hsla(${ChameleonConfig.baseHue}, 80%, 60%, 1)
                    );
                    border-radius: 4px;
                    outline: none;
                    cursor: pointer;
                ">
        </div>
        
        <!-- Saturation Slider -->
        <div style="margin-bottom: 20px;">
            <label style="color: #888; font-size: 12px; display: flex; justify-content: space-between; margin-bottom: 8px;">
                <span>💫 Saturation</span>
                <span id="saturation-value" style="color: var(--cyan); font-weight: bold;">${ChameleonConfig.saturation}%</span>
            </label>
            <input type="range" id="saturation-slider" min="0" max="100" value="${ChameleonConfig.saturation}" 
                oninput="updateSaturation(this.value)"
                style="width: 100%; cursor: pointer;">
        </div>
        
        <!-- Lightness Slider -->
        <div style="margin-bottom: 20px;">
            <label style="color: #888; font-size: 12px; display: flex; justify-content: space-between; margin-bottom: 8px;">
                <span>🌟 Lightness</span>
                <span id="lightness-value" style="color: var(--cyan); font-weight: bold;">${ChameleonConfig.lightness}%</span>
            </label>
            <input type="range" id="lightness-slider" min="20" max="80" value="${ChameleonConfig.lightness}" 
                oninput="updateLightness(this.value)"
                style="width: 100%; cursor: pointer;">
        </div>
        
        <!-- Quick Presets -->
        <div style="margin-bottom: 20px;">
            <div style="color: #888; font-size: 12px; margin-bottom: 8px;">🎨 Quick Presets:</div>
            <div style="display: grid; grid-template-columns: repeat(3, 1fr); gap: 8px;">
                <button onclick="applyPreset('cyan')" style="padding: 8px; background: linear-gradient(135deg, #00d4ff, #00a0c8); border: none; border-radius: 6px; cursor: pointer; font-size: 11px; font-weight: bold; color: white;">Cyan</button>
                <button onclick="applyPreset('green')" style="padding: 8px; background: linear-gradient(135deg, #00ff88, #00c868); border: none; border-radius: 6px; cursor: pointer; font-size: 11px; font-weight: bold; color: white;">Green</button>
                <button onclick="applyPreset('purple')" style="padding: 8px; background: linear-gradient(135deg, #a855f7, #8b3fd8); border: none; border-radius: 6px; cursor: pointer; font-size: 11px; font-weight: bold; color: white;">Purple</button>
                <button onclick="applyPreset('orange')" style="padding: 8px; background: linear-gradient(135deg, #ff6b35, #d8551d); border: none; border-radius: 6px; cursor: pointer; font-size: 11px; font-weight: bold; color: white;">Orange</button>
                <button onclick="applyPreset('red')" style="padding: 8px; background: linear-gradient(135deg, #ff4757, #d8303f); border: none; border-radius: 6px; cursor: pointer; font-size: 11px; font-weight: bold; color: white;">Red</button>
                <button onclick="applyPreset('blue')" style="padding: 8px; background: linear-gradient(135deg, #4a69ff, #3451d8); border: none; border-radius: 6px; cursor: pointer; font-size: 11px; font-weight: bold; color: white;">Blue</button>
            </div>
        </div>
        
        <!-- Animation Toggle -->
        <div style="margin-bottom: 15px;">
            <button onclick="toggleRainbowMode()" id="rainbow-btn" style="
                width: 100%;
                padding: 12px;
                background: linear-gradient(90deg, 
                    hsl(0, 80%, 60%),
                    hsl(60, 80%, 60%),
                    hsl(120, 80%, 60%),
                    hsl(180, 80%, 60%),
                    hsl(240, 80%, 60%),
                    hsl(300, 80%, 60%),
                    hsl(360, 80%, 60%)
                );
                border: 2px solid var(--purple);
                border-radius: 8px;
                cursor: pointer;
                font-weight: bold;
                color: white;
                font-size: 13px;
                text-shadow: 0 2px 4px rgba(0,0,0,0.5);
            ">🌈 Rainbow Mode</button>
        </div>
        
        <!-- Reset Button -->
        <button onclick="resetChameleon()" style="
            width: 100%;
            padding: 10px;
            background: rgba(255,71,87,0.2);
            border: 1px solid var(--red);
            border-radius: 6px;
            cursor: pointer;
            color: var(--red);
            font-weight: bold;
            font-size: 12px;
        ">🔄 Reset to Default</button>
        
        <div style="margin-top: 15px; padding: 10px; background: rgba(0,212,255,0.1); border-radius: 8px; font-size: 10px; color: #888; text-align: center;">
            💡 Changes apply in real-time!<br>
            Perfect for 240Hz displays
        </div>
    `;
    
    document.body.appendChild(controls);
    
    // Add slider styles
    const sliderStyle = document.createElement('style');
    sliderStyle.textContent = `
        input[type="range"]::-webkit-slider-thumb {
            -webkit-appearance: none;
            width: 20px;
            height: 20px;
            background: var(--cyan);
            border-radius: 50%;
            cursor: pointer;
            box-shadow: 0 2px 10px rgba(0,212,255,0.5);
        }
        
        input[type="range"]::-webkit-slider-thumb:hover {
            background: var(--green);
            box-shadow: 0 4px 15px rgba(0,255,136,0.6);
        }
    `;
    document.head.appendChild(sliderStyle);
}

// ============================================================================
// CONTROL FUNCTIONS
// ============================================================================

function updateHue(value) {
    const hue = parseInt(value);
    document.getElementById('hue-value').textContent = hue + '°';
    
    if (window.chameleonTheme) {
        window.chameleonTheme.setHue(hue);
    }
    
    updateColorPreview();
}

function updateTransparency(value) {
    const alpha = parseInt(value) / 100;
    document.getElementById('transparency-value').textContent = value + '%';
    
    if (window.chameleonTheme) {
        window.chameleonTheme.setTransparency(alpha);
    }
    
    updateColorPreview();
}

function updateSaturation(value) {
    const sat = parseInt(value);
    document.getElementById('saturation-value').textContent = sat + '%';
    
    if (window.chameleonTheme) {
        window.chameleonTheme.setSaturation(sat);
    }
    
    updateColorPreview();
}

function updateLightness(value) {
    const light = parseInt(value);
    document.getElementById('lightness-value').textContent = light + '%';
    
    if (window.chameleonTheme) {
        window.chameleonTheme.setLightness(light);
    }
    
    updateColorPreview();
}

function updateColorPreview() {
    const preview = document.getElementById('color-preview');
    const hue = parseInt(document.getElementById('hue-slider').value);
    const alpha = parseInt(document.getElementById('transparency-slider').value) / 100;
    const sat = parseInt(document.getElementById('saturation-slider').value);
    const light = parseInt(document.getElementById('lightness-slider').value);
    
    if (preview) {
        preview.style.background = `linear-gradient(135deg, 
            hsla(${hue}, ${sat}%, ${light}%, ${alpha}),
            hsla(${(hue + 60) % 360}, ${sat}%, ${light}%, ${alpha})
        )`;
    }
    
    // Update transparency slider background
    const transparencySlider = document.getElementById('transparency-slider');
    if (transparencySlider) {
        transparencySlider.style.background = `linear-gradient(to right, 
            transparent,
            hsla(${hue}, ${sat}%, ${light}%, 1)
        )`;
    }
}

function applyPreset(preset) {
    const presets = {
        cyan: { hue: 180, sat: 80, light: 60 },
        green: { hue: 140, sat: 90, light: 55 },
        purple: { hue: 270, sat: 70, light: 60 },
        orange: { hue: 20, sat: 85, light: 60 },
        red: { hue: 350, sat: 80, light: 60 },
        blue: { hue: 220, sat: 75, light: 60 }
    };
    
    const config = presets[preset];
    if (!config) return;
    
    // Update sliders
    document.getElementById('hue-slider').value = config.hue;
    document.getElementById('saturation-slider').value = config.sat;
    document.getElementById('lightness-slider').value = config.light;
    
    // Update displays
    document.getElementById('hue-value').textContent = config.hue + '°';
    document.getElementById('saturation-value').textContent = config.sat + '%';
    document.getElementById('lightness-value').textContent = config.light + '%';
    
    // Apply theme
    if (window.chameleonTheme) {
        window.chameleonTheme.setHue(config.hue);
        window.chameleonTheme.setSaturation(config.sat);
        window.chameleonTheme.setLightness(config.light);
    }
    
    updateColorPreview();
    
    console.log(`[Chameleon] 🎨 Applied preset: ${preset}`);
}

function toggleRainbowMode() {
    if (window.chameleonTheme) {
        window.chameleonTheme.toggleAnimation();
        
        const btn = document.getElementById('rainbow-btn');
        if (btn) {
            if (window.chameleonTheme.animating) {
                btn.textContent = '⏸️ Stop Rainbow';
            } else {
                btn.textContent = '🌈 Rainbow Mode';
            }
        }
    }
}

function resetChameleon() {
    // Reset to defaults
    document.getElementById('hue-slider').value = 180;
    document.getElementById('transparency-slider').value = 100;
    document.getElementById('saturation-slider').value = 80;
    document.getElementById('lightness-slider').value = 60;
    
    updateHue(180);
    updateTransparency(100);
    updateSaturation(80);
    updateLightness(60);
    
    console.log('[Chameleon] 🔄 Reset to defaults');
}

function toggleChameleonControls() {
    const controls = document.getElementById('chameleon-controls');
    if (controls) {
        controls.style.display = controls.style.display === 'none' ? 'block' : 'none';
    }
}

// ============================================================================
// CHAMELEON TOGGLE BUTTON
// ============================================================================

function createChameleonToggleButton() {
    const button = document.createElement('button');
    button.id = 'chameleon-toggle-btn';
    button.innerHTML = '🦎';
    button.style.cssText = `
        position: fixed;
        bottom: 140px;
        right: 20px;
        width: 50px;
        height: 50px;
        background: linear-gradient(135deg, #a855f7, #ff6b35);
        border: 2px solid var(--purple);
        border-radius: 50%;
        color: white;
        font-size: 24px;
        cursor: pointer;
        z-index: 99996;
        box-shadow: 0 5px 20px rgba(168,85,247,0.5);
        transition: all 0.3s;
        display: flex;
        align-items: center;
        justify-content: center;
    `;
    
    button.onmouseover = () => {
        button.style.transform = 'scale(1.1) rotate(-15deg)';
        button.style.boxShadow = '0 8px 30px rgba(168,85,247,0.8)';
    };
    
    button.onmouseout = () => {
        button.style.transform = 'scale(1) rotate(0deg)';
        button.style.boxShadow = '0 5px 20px rgba(168,85,247,0.5)';
    };
    
    button.onclick = toggleChameleonControls;
    
    document.body.appendChild(button);
}

// ============================================================================
// INITIALIZATION
// ============================================================================

// Wait for DOM to be ready
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        window.chameleonTheme = new ChameleonTheme();
        createChameleonToggleButton();
        createChameleonControls();
        
        console.log('[Chameleon] ✅ Chameleon theme system initialized');
        console.log('[Chameleon] 🎨 Click 🦎 button to customize colors');
    });
} else {
    window.chameleonTheme = new ChameleonTheme();
    createChameleonToggleButton();
    createChameleonControls();
    
    console.log('[Chameleon] ✅ Chameleon theme system initialized');
    console.log('[Chameleon] 🎨 Click 🦎 button to customize colors');
}

// Export
if (typeof module !== 'undefined' && module.exports) {
    module.exports = { ChameleonTheme, ChameleonConfig };
}

