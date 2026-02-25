/**
 * BigDaddyG IDE - Mouse Ripple Effect
 * Beautiful ripple effect that follows your cursor
 * Optimized for 4K 240Hz displays
 */

// ============================================================================
// RIPPLE CONFIGURATION
// ============================================================================

const RippleConfig = {
    // Visual settings
    maxRipples: 50,                // Maximum active ripples
    rippleLifetime: 1500,          // Ripple duration (ms)
    rippleSize: 200,               // Maximum ripple size (px)
    rippleSpeed: 0.8,              // Expansion speed
    opacity: 0.3,                  // Starting opacity
    
    // Colors
    colors: [
        'rgba(0, 212, 255, OPACITY)',     // Cyan
        'rgba(0, 255, 136, OPACITY)',     // Green
        'rgba(168, 85, 247, OPACITY)',    // Purple
        'rgba(255, 107, 53, OPACITY)',    // Orange
        'rgba(255, 71, 87, OPACITY)'      // Red
    ],
    
    // Performance
    enabled: true,
    quality: 'high',               // 'low', 'medium', 'high'
    throttleMs: 16,                // ~60fps (adjust for higher refresh)
    
    // Behavior
    spawnOnMove: true,
    spawnOnClick: true,
    trailEffect: true,
    pulseEffect: false,
    glow: true
};

// ============================================================================
// RIPPLE SYSTEM
// ============================================================================

class RippleSystem {
    constructor() {
        this.canvas = null;
        this.ctx = null;
        this.ripples = [];
        this.lastSpawnTime = 0;
        this.mouseX = 0;
        this.mouseY = 0;
        this.animationFrame = null;
        this.colorIndex = 0;
        
        this.init();
    }
    
    init() {
        // Create canvas overlay
        this.canvas = document.createElement('canvas');
        this.canvas.id = 'ripple-canvas';
        this.canvas.style.cssText = `
            position: fixed;
            top: 0;
            left: 0;
            width: 100vw;
            height: 100vh;
            pointer-events: none;
            z-index: 999999;
        `;
        
        document.body.appendChild(this.canvas);
        
        this.ctx = this.canvas.getContext('2d', { alpha: true });
        this.resize();
        
        // Event listeners
        window.addEventListener('resize', () => this.resize());
        document.addEventListener('mousemove', (e) => this.onMouseMove(e));
        document.addEventListener('click', (e) => this.onMouseClick(e));
        
        // Start animation loop
        this.animate();
        
        console.log('[Ripple] 🌊 Mouse ripple effect initialized');
    }
    
    resize() {
        const dpr = window.devicePixelRatio || 1;
        this.canvas.width = window.innerWidth * dpr;
        this.canvas.height = window.innerHeight * dpr;
        this.canvas.style.width = window.innerWidth + 'px';
        this.canvas.style.height = window.innerHeight + 'px';
        this.ctx.scale(dpr, dpr);
    }
    
    onMouseMove(e) {
        if (!RippleConfig.enabled || !RippleConfig.spawnOnMove) return;
        
        this.mouseX = e.clientX;
        this.mouseY = e.clientY;
        
        const now = Date.now();
        if (now - this.lastSpawnTime < RippleConfig.throttleMs) return;
        
        this.createRipple(e.clientX, e.clientY, 'move');
        this.lastSpawnTime = now;
    }
    
    onMouseClick(e) {
        if (!RippleConfig.enabled || !RippleConfig.spawnOnClick) return;
        
        // Create larger ripple on click
        this.createRipple(e.clientX, e.clientY, 'click');
    }
    
    createRipple(x, y, type = 'move') {
        // Remove oldest ripple if at max
        if (this.ripples.length >= RippleConfig.maxRipples) {
            this.ripples.shift();
        }
        
        // Cycle through colors
        const color = RippleConfig.colors[this.colorIndex];
        this.colorIndex = (this.colorIndex + 1) % RippleConfig.colors.length;
        
        const ripple = {
            x: x,
            y: y,
            size: 0,
            maxSize: type === 'click' ? RippleConfig.rippleSize * 1.5 : RippleConfig.rippleSize,
            opacity: type === 'click' ? RippleConfig.opacity * 1.5 : RippleConfig.opacity,
            color: color,
            birthTime: Date.now(),
            type: type,
            speed: type === 'click' ? RippleConfig.rippleSpeed * 1.2 : RippleConfig.rippleSpeed
        };
        
        this.ripples.push(ripple);
    }
    
    animate() {
        if (!RippleConfig.enabled) {
            this.animationFrame = requestAnimationFrame(() => this.animate());
            return;
        }
        
        // Clear canvas
        this.ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
        
        // Update and draw ripples
        const now = Date.now();
        
        this.ripples = this.ripples.filter(ripple => {
            const age = now - ripple.birthTime;
            
            // Remove if expired
            if (age > RippleConfig.rippleLifetime) {
                return false;
            }
            
            // Update ripple
            ripple.size += ripple.speed * (ripple.maxSize / 100);
            
            // Calculate opacity (fade out)
            const lifePercent = age / RippleConfig.rippleLifetime;
            const currentOpacity = ripple.opacity * (1 - lifePercent);
            
            // Draw ripple
            this.drawRipple(ripple, currentOpacity);
            
            return true;
        });
        
        this.animationFrame = requestAnimationFrame(() => this.animate());
    }
    
    drawRipple(ripple, opacity) {
        const ctx = this.ctx;
        
        // Main ripple circle
        ctx.beginPath();
        ctx.arc(ripple.x, ripple.y, ripple.size, 0, Math.PI * 2);
        
        // Color with opacity
        const color = ripple.color.replace('OPACITY', opacity.toFixed(2));
        ctx.strokeStyle = color;
        ctx.lineWidth = ripple.type === 'click' ? 3 : 2;
        ctx.stroke();
        
        // Glow effect
        if (RippleConfig.glow) {
            ctx.shadowBlur = 20;
            ctx.shadowColor = color;
            ctx.stroke();
            ctx.shadowBlur = 0;
        }
        
        // Inner circle (pulse effect)
        if (RippleConfig.pulseEffect && ripple.size > 20) {
            ctx.beginPath();
            ctx.arc(ripple.x, ripple.y, ripple.size * 0.5, 0, Math.PI * 2);
            ctx.strokeStyle = ripple.color.replace('OPACITY', (opacity * 0.5).toFixed(2));
            ctx.lineWidth = 1;
            ctx.stroke();
        }
        
        // Trail effect (gradient fill)
        if (RippleConfig.trailEffect && ripple.size > 10) {
            const gradient = ctx.createRadialGradient(
                ripple.x, ripple.y, 0,
                ripple.x, ripple.y, ripple.size
            );
            gradient.addColorStop(0, ripple.color.replace('OPACITY', (opacity * 0.3).toFixed(2)));
            gradient.addColorStop(0.5, ripple.color.replace('OPACITY', (opacity * 0.1).toFixed(2)));
            gradient.addColorStop(1, ripple.color.replace('OPACITY', '0'));
            
            ctx.fillStyle = gradient;
            ctx.fill();
        }
    }
    
    toggle() {
        RippleConfig.enabled = !RippleConfig.enabled;
        
        if (!RippleConfig.enabled) {
            this.ripples = [];
            this.ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
        }
        
        console.log(`[Ripple] ${RippleConfig.enabled ? '🌊 Enabled' : '🛑 Disabled'}`);
    }
    
    setQuality(quality) {
        RippleConfig.quality = quality;
        
        switch(quality) {
            case 'low':
                RippleConfig.maxRipples = 20;
                RippleConfig.glow = false;
                RippleConfig.trailEffect = false;
                RippleConfig.throttleMs = 32; // ~30fps
                break;
            case 'medium':
                RippleConfig.maxRipples = 35;
                RippleConfig.glow = true;
                RippleConfig.trailEffect = false;
                RippleConfig.throttleMs = 16; // ~60fps
                break;
            case 'high':
                RippleConfig.maxRipples = 50;
                RippleConfig.glow = true;
                RippleConfig.trailEffect = true;
                RippleConfig.throttleMs = 8; // ~120fps
                break;
            case 'ultra':
                RippleConfig.maxRipples = 100;
                RippleConfig.glow = true;
                RippleConfig.trailEffect = true;
                RippleConfig.pulseEffect = true;
                RippleConfig.throttleMs = 4; // ~240fps
                break;
        }
        
        console.log(`[Ripple] 🎨 Quality: ${quality}`);
    }
    
    destroy() {
        if (this.animationFrame) {
            cancelAnimationFrame(this.animationFrame);
        }
        
        if (this.canvas) {
            this.canvas.remove();
        }
        
        console.log('[Ripple] 🗑️ Ripple system destroyed');
    }
}

// ============================================================================
// RIPPLE CONTROL PANEL
// ============================================================================

function createRippleControls() {
    const controls = document.createElement('div');
    controls.id = 'ripple-controls';
    controls.style.cssText = `
        position: fixed;
        bottom: 80px;
        right: 20px;
        background: rgba(10, 10, 30, 0.95);
        backdrop-filter: blur(20px);
        border: 2px solid var(--cyan);
        border-radius: 12px;
        padding: 15px;
        z-index: 999997;
        display: none;
        min-width: 250px;
    `;
    
    controls.innerHTML = `
        <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 15px; padding-bottom: 10px; border-bottom: 1px solid var(--cyan);">
            <div style="color: var(--cyan); font-weight: bold; font-size: 14px;">🌊 Ripple Effects</div>
            <button onclick="toggleRippleControls()" style="background: var(--red); color: white; border: none; padding: 4px 8px; border-radius: 4px; cursor: pointer; font-size: 12px;">✕</button>
        </div>
        
        <div style="margin-bottom: 12px;">
            <label style="color: #888; font-size: 11px; display: block; margin-bottom: 5px;">Quality:</label>
            <select id="ripple-quality" onchange="changeRippleQuality(this.value)" style="width: 100%; padding: 8px; background: rgba(0,0,0,0.5); color: white; border: 1px solid var(--cyan); border-radius: 6px; font-size: 12px;">
                <option value="low">Low (30 FPS)</option>
                <option value="medium">Medium (60 FPS)</option>
                <option value="high" selected>High (120 FPS)</option>
                <option value="ultra">Ultra (240 FPS)</option>
            </select>
        </div>
        
        <div style="margin-bottom: 12px;">
            <label style="display: flex; align-items: center; cursor: pointer;">
                <input type="checkbox" id="ripple-enabled" checked onchange="toggleRipple()" style="margin-right: 8px;">
                <span style="color: #ccc; font-size: 12px;">Enable Ripples</span>
            </label>
        </div>
        
        <div style="margin-bottom: 12px;">
            <label style="display: flex; align-items: center; cursor: pointer;">
                <input type="checkbox" id="ripple-glow" checked onchange="toggleRippleGlow()" style="margin-right: 8px;">
                <span style="color: #ccc; font-size: 12px;">Glow Effect</span>
            </label>
        </div>
        
        <div style="margin-bottom: 12px;">
            <label style="display: flex; align-items: center; cursor: pointer;">
                <input type="checkbox" id="ripple-trail" checked onchange="toggleRippleTrail()" style="margin-right: 8px;">
                <span style="color: #ccc; font-size: 12px;">Trail Effect</span>
            </label>
        </div>
        
        <div style="margin-bottom: 12px;">
            <label style="display: flex; align-items: center; cursor: pointer;">
                <input type="checkbox" id="ripple-pulse" onchange="toggleRipplePulse()" style="margin-right: 8px;">
                <span style="color: #ccc; font-size: 12px;">Pulse Effect</span>
            </label>
        </div>
        
        <div style="margin-bottom: 12px;">
            <label style="color: #888; font-size: 11px; display: block; margin-bottom: 5px;">Size:</label>
            <input type="range" id="ripple-size" min="50" max="400" value="200" onchange="updateRippleSize(this.value)" style="width: 100%;">
            <div style="text-align: center; color: #666; font-size: 10px; margin-top: 3px;">
                <span id="ripple-size-value">200</span>px
            </div>
        </div>
        
        <div style="margin-bottom: 12px;">
            <label style="color: #888; font-size: 11px; display: block; margin-bottom: 5px;">Opacity:</label>
            <input type="range" id="ripple-opacity" min="0.1" max="1" step="0.1" value="0.3" onchange="updateRippleOpacity(this.value)" style="width: 100%;">
            <div style="text-align: center; color: #666; font-size: 10px; margin-top: 3px;">
                <span id="ripple-opacity-value">0.3</span>
            </div>
        </div>
        
        <div style="padding: 10px; background: rgba(0,212,255,0.1); border-radius: 6px; font-size: 10px; color: #888; text-align: center;">
            Move your mouse to see ripples!<br>
            Click for larger ripples
        </div>
    `;
    
    document.body.appendChild(controls);
}

function toggleRippleControls() {
    const controls = document.getElementById('ripple-controls');
    if (controls) {
        controls.style.display = controls.style.display === 'none' ? 'block' : 'none';
    }
}

// ============================================================================
// CONTROL FUNCTIONS
// ============================================================================

function changeRippleQuality(quality) {
    if (window.rippleSystem) {
        window.rippleSystem.setQuality(quality);
    }
}

function toggleRipple() {
    if (window.rippleSystem) {
        window.rippleSystem.toggle();
    }
}

function toggleRippleGlow() {
    const checkbox = document.getElementById('ripple-glow');
    RippleConfig.glow = checkbox.checked;
}

function toggleRippleTrail() {
    const checkbox = document.getElementById('ripple-trail');
    RippleConfig.trailEffect = checkbox.checked;
}

function toggleRipplePulse() {
    const checkbox = document.getElementById('ripple-pulse');
    RippleConfig.pulseEffect = checkbox.checked;
}

function updateRippleSize(value) {
    RippleConfig.rippleSize = parseInt(value);
    document.getElementById('ripple-size-value').textContent = value;
}

function updateRippleOpacity(value) {
    RippleConfig.opacity = parseFloat(value);
    document.getElementById('ripple-opacity-value').textContent = value;
}

// ============================================================================
// RIPPLE TOGGLE BUTTON
// ============================================================================

function createRippleToggleButton() {
    const button = document.createElement('button');
    button.id = 'ripple-toggle-btn';
    button.innerHTML = '🌊';
    button.style.cssText = `
        position: fixed;
        bottom: 80px;
        right: 20px;
        width: 50px;
        height: 50px;
        background: linear-gradient(135deg, #00d4ff, #00ff88);
        border: 2px solid var(--cyan);
        border-radius: 50%;
        color: white;
        font-size: 24px;
        cursor: pointer;
        z-index: 99997;
        box-shadow: 0 5px 20px rgba(0,212,255,0.5);
        transition: all 0.3s;
        display: flex;
        align-items: center;
        justify-content: center;
    `;
    
    button.onmouseover = () => {
        button.style.transform = 'scale(1.1) rotate(15deg)';
        button.style.boxShadow = '0 8px 30px rgba(0,212,255,0.8)';
    };
    
    button.onmouseout = () => {
        button.style.transform = 'scale(1) rotate(0deg)';
        button.style.boxShadow = '0 5px 20px rgba(0,212,255,0.5)';
    };
    
    button.onclick = toggleRippleControls;
    
    document.body.appendChild(button);
}

// ============================================================================
// AUTO-ADJUST FOR HIGH REFRESH RATE
// ============================================================================

function detectRefreshRate() {
    let lastTime = performance.now();
    let frameCount = 0;
    let refreshRate = 60;
    
    function measure() {
        const now = performance.now();
        const delta = now - lastTime;
        
        if (delta >= 1000) {
            refreshRate = frameCount;
            frameCount = 0;
            lastTime = now;
            
            // Adjust ripple quality based on refresh rate
            if (refreshRate >= 240) {
                changeRippleQuality('ultra');
                console.log('[Ripple] 🚀 Detected 240Hz+ display - Ultra quality enabled');
            } else if (refreshRate >= 120) {
                changeRippleQuality('high');
                console.log('[Ripple] ⚡ Detected 120Hz+ display - High quality enabled');
            }
            
            return; // Stop measuring after first detection
        }
        
        frameCount++;
        requestAnimationFrame(measure);
    }
    
    requestAnimationFrame(measure);
}

// ============================================================================
// INITIALIZATION
// ============================================================================

// Wait for DOM to be ready
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        window.rippleSystem = new RippleSystem();
        createRippleToggleButton();
        createRippleControls();
        detectRefreshRate();
        
        console.log('[Ripple] ✅ Mouse ripple system initialized');
        console.log('[Ripple] 💡 Click 🌊 button to configure');
    });
} else {
    window.rippleSystem = new RippleSystem();
    createRippleToggleButton();
    createRippleControls();
    detectRefreshRate();
    
    console.log('[Ripple] ✅ Mouse ripple system initialized');
    console.log('[Ripple] 💡 Click 🌊 button to configure');
}

// Export
if (typeof module !== 'undefined' && module.exports) {
    module.exports = { RippleSystem, RippleConfig };
}

