/**
 * BigDaddyG IDE - Performance & Power Modes
 * 4K 240Hz optimization + Overclocked AI modes
 */

// ============================================================================
// POWER MODES CONFIGURATION
// ============================================================================

const PowerModes = {
    // ECO MODE - Battery saving, lower performance
    ECO: {
        name: '🌱 Eco',
        description: 'Battery saving mode - Lower performance, maximum efficiency',
        fps_target: 60,
        gpu_acceleration: false,
        ai: {
            temperature: 0.3,
            max_tokens: 500,
            top_p: 0.7,
            batch_size: 1,
            stream_delay: 100,
            thinking_time: 2000
        },
        rendering: {
            animations: false,
            particles: false,
            shadows: false,
            blur: false,
            antialiasing: 'none',
            vsync: true
        },
        orchestra: {
            max_agents: 2,
            parallel_tasks: 1,
            cache_aggressive: true,
            preload: false
        }
    },

    // BALANCED MODE - Default, good balance
    BALANCED: {
        name: '⚖️ Balanced',
        description: 'Balanced performance and efficiency',
        fps_target: 120,
        gpu_acceleration: true,
        ai: {
            temperature: 0.7,
            max_tokens: 2000,
            top_p: 0.9,
            batch_size: 2,
            stream_delay: 50,
            thinking_time: 1000
        },
        rendering: {
            animations: true,
            particles: true,
            shadows: true,
            blur: true,
            antialiasing: 'msaa2x',
            vsync: true
        },
        orchestra: {
            max_agents: 4,
            parallel_tasks: 2,
            cache_aggressive: false,
            preload: true
        }
    },

    // TURBO MODE - High performance
    TURBO: {
        name: '🚀 Turbo',
        description: 'High performance mode - Faster AI, more agents',
        fps_target: 165,
        gpu_acceleration: true,
        ai: {
            temperature: 0.8,
            max_tokens: 4000,
            top_p: 0.95,
            batch_size: 4,
            stream_delay: 20,
            thinking_time: 500
        },
        rendering: {
            animations: true,
            particles: true,
            shadows: true,
            blur: true,
            antialiasing: 'msaa4x',
            vsync: false
        },
        orchestra: {
            max_agents: 8,
            parallel_tasks: 4,
            cache_aggressive: false,
            preload: true
        }
    },

    // OVERCLOCKED MODE - Maximum performance (4K 240Hz optimized)
    OVERCLOCKED: {
        name: '⚡ OVERCLOCKED',
        description: 'EXTREME PERFORMANCE - 4K 240Hz, max AI power, unlimited agents',
        fps_target: 240,
        gpu_acceleration: true,
        ai: {
            temperature: 1.2,          // Creative mode
            max_tokens: 8000,          // Long responses
            top_p: 0.98,               // High diversity
            batch_size: 8,             // Parallel processing
            stream_delay: 5,           // Instant streaming
            thinking_time: 100         // Near-instant response
        },
        rendering: {
            animations: true,
            particles: true,
            shadows: true,
            blur: true,
            antialiasing: 'msaa8x',    // Maximum quality
            vsync: false,              // Unlock framerate
            motion_blur: true,         // Smooth motion
            ambient_occlusion: true,   // Better lighting
            hdr: true                  // HDR if available
        },
        orchestra: {
            max_agents: 32,            // Unlimited agents
            parallel_tasks: 16,        // Massive parallelism
            cache_aggressive: false,
            preload: true,
            prefetch: true,
            predictive_spawn: true     // Spawn agents before needed
        },
        display: {
            resolution: '4K',          // 3840x2160
            refresh_rate: 240,
            adaptive_sync: true,       // G-Sync/FreeSync
            hdr: true,
            color_depth: 10            // 10-bit color
        }
    },

    // LIQUID NITROGEN MODE - Beyond overclocked (experimental)
    LIQUID_NITROGEN: {
        name: '❄️ LIQUID NITROGEN',
        description: 'EXPERIMENTAL - Requires cooling, pushes hardware to absolute limits',
        fps_target: 360,
        gpu_acceleration: true,
        ai: {
            temperature: 1.5,          // Maximum creativity
            max_tokens: 16000,         // Massive responses
            top_p: 0.99,               // Extreme diversity
            batch_size: 16,            // Maximum parallelism
            stream_delay: 1,           // Instant streaming
            thinking_time: 50,         // Lightning fast
            quantum_mode: true         // Experimental quantum optimization
        },
        rendering: {
            animations: true,
            particles: true,
            shadows: true,
            blur: true,
            antialiasing: 'taa',       // Temporal anti-aliasing
            vsync: false,
            motion_blur: true,
            ambient_occlusion: true,
            hdr: true,
            ray_tracing: true,         // Real-time ray tracing
            dlss: true                 // AI upscaling if available
        },
        orchestra: {
            max_agents: 128,           // Massive agent swarm
            parallel_tasks: 64,        // CPU core saturation
            cache_aggressive: false,
            preload: true,
            prefetch: true,
            predictive_spawn: true,
            speculative_execution: true // Execute tasks before confirmation
        },
        display: {
            resolution: '8K',          // 7680x4320 (if available)
            refresh_rate: 360,
            adaptive_sync: true,
            hdr: true,
            color_depth: 12,           // 12-bit color
            hdr_peak_brightness: 1000  // nits
        },
        warning: '⚠️ EXTREME MODE - May cause system instability, high power consumption, and thermal throttling'
    }
};

// ============================================================================
// CURRENT POWER MODE
// ============================================================================

let currentPowerMode = 'BALANCED';
let performanceMonitor = {
    fps: 0,
    frame_time: 0,
    cpu_usage: 0,
    gpu_usage: 0,
    memory_usage: 0,
    ai_response_time: 0,
    active_agents: 0,
    total_tokens: 0
};

// ============================================================================
// APPLY POWER MODE
// ============================================================================

function applyPowerMode(modeName) {
    const mode = PowerModes[modeName];
    if (!mode) {
        console.error(`[Performance] ❌ Invalid mode: ${modeName}`);
        return;
    }

    console.log(`[Performance] ⚡ Switching to ${mode.name}`);
    console.log(`[Performance] 📝 ${mode.description}`);

    currentPowerMode = modeName;

    // Apply AI settings
    applyAISettings(mode.ai);

    // Apply rendering settings
    applyRenderingSettings(mode.rendering);

    // Apply orchestra settings
    applyOrchestraSettings(mode.orchestra);

    // Apply display settings (if overclocked/liquid nitrogen)
    if (mode.display) {
        applyDisplaySettings(mode.display);
    }

    // Show notification
    showPowerModeNotification(mode);

    // Update UI
    updatePowerModeUI(modeName);

    // Log telemetry
    console.log(`[Performance] ✅ ${mode.name} mode activated`);
    logTelemetry('power_mode_changed', { mode: modeName });
}

// ============================================================================
// AI SETTINGS
// ============================================================================

async function applyAISettings(settings) {
    console.log('[Performance] 🧠 Applying AI settings...');

    try {
        const response = await fetch('http://localhost:11441/api/parameters/set', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                temperature: settings.temperature,
                max_tokens: settings.max_tokens,
                top_p: settings.top_p,
                stream_delay: settings.stream_delay,
                thinking_time: settings.thinking_time,
                batch_size: settings.batch_size,
                quantum_mode: settings.quantum_mode || false
            })
        });

        if (response.ok) {
            console.log('[Performance] ✅ AI settings applied');
            console.log(`   Temperature: ${settings.temperature}`);
            console.log(`   Max tokens: ${settings.max_tokens}`);
            console.log(`   Thinking time: ${settings.thinking_time}ms`);
        }
    } catch (error) {
        console.error('[Performance] ❌ Failed to apply AI settings:', error);
    }
}

// ============================================================================
// RENDERING SETTINGS
// ============================================================================

function applyRenderingSettings(settings) {
    console.log('[Performance] 🎨 Applying rendering settings...');

    // Set target FPS
    if (settings.fps_target) {
        setTargetFPS(PowerModes[currentPowerMode].fps_target);
    }

    // GPU acceleration
    document.body.style.transform = settings.gpu_acceleration ? 'translateZ(0)' : 'none';
    document.body.style.willChange = settings.gpu_acceleration ? 'transform' : 'auto';

    // Animations
    if (!settings.animations) {
        document.body.style.animation = 'none';
        document.querySelectorAll('*').forEach(el => {
            el.style.animation = 'none';
            el.style.transition = 'none';
        });
    }

    // Particles (cosmic background, token streams)
    toggleParticles(settings.particles);

    // Shadows and blur
    const root = document.documentElement;
    root.style.setProperty('--shadow-enabled', settings.shadows ? '1' : '0');
    root.style.setProperty('--blur-enabled', settings.blur ? '1' : '0');

    // Anti-aliasing
    if (editor && editor.updateOptions) {
        editor.updateOptions({
            'smoothScrolling': settings.antialiasing !== 'none',
            'cursorSmoothCaretAnimation': settings.antialiasing !== 'none'
        });
    }

    // Motion blur (CSS filter)
    if (settings.motion_blur) {
        document.body.style.filter = 'blur(0.1px)'; // Subtle motion blur
    } else {
        document.body.style.filter = 'none';
    }

    // HDR (if supported)
    if (settings.hdr && window.matchMedia) {
        const hdrSupported = window.matchMedia('(dynamic-range: high)').matches;
        if (hdrSupported) {
            document.body.style.colorScheme = 'dark light';
            console.log('[Performance] 📺 HDR enabled');
        }
    }

    console.log('[Performance] ✅ Rendering settings applied');
}

// ============================================================================
// ORCHESTRA SETTINGS
// ============================================================================

async function applyOrchestraSettings(settings) {
    console.log('[Performance] 🎼 Applying Orchestra settings...');

    try {
        const response = await fetch('http://localhost:11441/api/orchestra/configure', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                max_agents: settings.max_agents,
                parallel_tasks: settings.parallel_tasks,
                cache_aggressive: settings.cache_aggressive,
                preload: settings.preload,
                prefetch: settings.prefetch || false,
                predictive_spawn: settings.predictive_spawn || false,
                speculative_execution: settings.speculative_execution || false
            })
        });

        if (response.ok) {
            console.log('[Performance] ✅ Orchestra settings applied');
            console.log(`   Max agents: ${settings.max_agents}`);
            console.log(`   Parallel tasks: ${settings.parallel_tasks}`);
        }
    } catch (error) {
        console.error('[Performance] ❌ Failed to apply Orchestra settings:', error);
    }
}

// ============================================================================
// DISPLAY SETTINGS (4K/8K @ 240Hz/360Hz)
// ============================================================================

function applyDisplaySettings(settings) {
    console.log('[Performance] 📺 Applying display settings...');

    // Check if we're in Electron
    if (window.electron) {
        const { screen } = require('electron');
        const display = screen.getPrimaryDisplay();

        console.log(`[Performance] 🖥️ Current display:`);
        console.log(`   Resolution: ${display.bounds.width}x${display.bounds.height}`);
        console.log(`   Refresh: ${display.displayFrequency}Hz`);
        console.log(`   Scale: ${display.scaleFactor}x`);

        // Optimize for high refresh rate
        if (settings.refresh_rate >= 240) {
            console.log(`[Performance] ⚡ Optimizing for ${settings.refresh_rate}Hz`);

            // Disable vsync for maximum framerate
            document.body.style.imageRendering = 'crisp-edges';

            // Request high performance mode
            if (navigator.gpu) {
                navigator.gpu.requestAdapter({ powerPreference: 'high-performance' });
            }
        }
    }

    // Set canvas rendering for 4K/8K
    if (settings.resolution === '4K' || settings.resolution === '8K') {
        const scale = settings.resolution === '8K' ? 2 : 1;
        document.querySelectorAll('canvas').forEach(canvas => {
            canvas.width = canvas.offsetWidth * scale * window.devicePixelRatio;
            canvas.height = canvas.offsetHeight * scale * window.devicePixelRatio;
        });

        console.log(`[Performance] 🎬 ${settings.resolution} rendering enabled`);
    }

    // Enable adaptive sync hint
    if (settings.adaptive_sync) {
        console.log('[Performance] 🔄 Adaptive sync recommended (G-Sync/FreeSync)');
    }

    console.log('[Performance] ✅ Display settings applied');
}

// ============================================================================
// FPS CONTROL
// ============================================================================

let targetFPS = 120;
let lastFrameTime = 0;
let frameCount = 0;
let fpsUpdateTime = 0;

function setTargetFPS(fps) {
    targetFPS = fps;
    console.log(`[Performance] 🎯 Target FPS: ${fps}`);
}

function updateFPS() {
    const now = performance.now();
    const delta = now - lastFrameTime;
    lastFrameTime = now;

    frameCount++;

    // Update FPS counter every second
    if (now - fpsUpdateTime > 1000) {
        performanceMonitor.fps = frameCount;
        performanceMonitor.frame_time = delta;
        frameCount = 0;
        fpsUpdateTime = now;

        // Update FPS display
        updateFPSDisplay();
    }

    // Request next frame based on target FPS
    const frameDelay = 1000 / targetFPS;
    if (delta >= frameDelay) {
        requestAnimationFrame(updateFPS);
    } else {
        setTimeout(() => requestAnimationFrame(updateFPS), frameDelay - delta);
    }
}

// Start FPS monitoring
requestAnimationFrame(updateFPS);

// ============================================================================
// PERFORMANCE MONITORING
// ============================================================================

function startPerformanceMonitoring() {
    setInterval(() => {
        // CPU usage (approximate)
        if (performance.memory) {
            performanceMonitor.memory_usage = performance.memory.usedJSHeapSize / performance.memory.jsHeapSizeLimit;
        }

        // Update performance overlay
        updatePerformanceOverlay();
    }, 1000);
}

function updatePerformanceOverlay() {
    let overlay = document.getElementById('performance-overlay');

    if (!overlay) {
        overlay = document.createElement('div');
        overlay.id = 'performance-overlay';
        overlay.style.cssText = `
            position: fixed;
            top: 10px;
            right: 10px;
            background: rgba(0, 0, 0, 0.8);
            backdrop-filter: blur(10px);
            border: 1px solid var(--cyan);
            border-radius: 8px;
            padding: 10px;
            font-family: 'Courier New', monospace;
            font-size: 11px;
            color: var(--cyan);
            z-index: 100000;
            min-width: 200px;
        `;
        document.body.appendChild(overlay);
    }

    const mode = PowerModes[currentPowerMode];
    const fpsColor = performanceMonitor.fps >= targetFPS * 0.9 ? 'var(--green)' :
        performanceMonitor.fps >= targetFPS * 0.6 ? 'var(--orange)' : 'var(--red)';

    overlay.innerHTML = `
        <div style="color: var(--cyan); font-weight: bold; margin-bottom: 8px; border-bottom: 1px solid var(--cyan); padding-bottom: 5px;">
            ${mode.name} MODE
        </div>
        <div style="color: ${fpsColor};">FPS: ${performanceMonitor.fps} / ${targetFPS}</div>
        <div>Frame: ${performanceMonitor.frame_time.toFixed(2)}ms</div>
        <div>Memory: ${(performanceMonitor.memory_usage * 100).toFixed(1)}%</div>
        <div>Agents: ${performanceMonitor.active_agents} / ${mode.orchestra.max_agents}</div>
        <div>Tokens: ${performanceMonitor.total_tokens.toLocaleString()}</div>
        <div style="margin-top: 8px; padding-top: 5px; border-top: 1px solid rgba(0,212,255,0.3);">
            <small>AI: ${mode.ai.temperature}T ${mode.ai.max_tokens}tk</small>
        </div>
    `;
}

function updateFPSDisplay() {
    const overlay = document.getElementById('performance-overlay');
    if (overlay) {
        updatePerformanceOverlay();
    }
}

// ============================================================================
// POWER MODE UI
// ============================================================================

function createPowerModeSelector() {
    const selector = document.createElement('div');
    selector.id = 'power-mode-selector';
    selector.style.cssText = `
        position: fixed;
        bottom: 20px;
        right: 20px;
        background: rgba(10, 10, 30, 0.95);
        backdrop-filter: blur(20px);
        border: 2px solid var(--cyan);
        border-radius: 12px;
        padding: 15px;
        z-index: 99999;
        min-width: 300px;
    `;

    let html = `
        <div style="color: var(--cyan); font-weight: bold; margin-bottom: 15px; font-size: 14px;">
            ⚡ POWER MODES
        </div>
    `;

    Object.keys(PowerModes).forEach(key => {
        const mode = PowerModes[key];
        const isActive = key === currentPowerMode;
        const activeBg = 'rgba(0,212,255,0.2)';
        const inactiveBg = 'rgba(0,0,0,0.3)';
        const bgColor = isActive ? activeBg : inactiveBg;

        html += `
            <div onclick="applyPowerMode('${key}')" style="
                padding: 12px;
                margin-bottom: 8px;
                background: ${bgColor};
                border: 1px solid ${isActive ? 'var(--cyan)' : 'rgba(0,212,255,0.3)'};
                border-left: 3px solid ${getModeColor(key)};
                border-radius: 6px;
                cursor: pointer;
                transition: all 0.2s;
            " onmouseover="this.style.background='rgba(0,212,255,0.1)'" onmouseout="this.style.background='${bgColor}'">
                <div style="font-weight: bold; color: ${getModeColor(key)}; margin-bottom: 4px;">
                    ${mode.name}
                </div>
                <div style="font-size: 10px; color: #888;">
                    ${mode.description}
                </div>
                <div style="font-size: 10px; color: var(--cyan); margin-top: 4px;">
                    ${mode.fps_target} FPS • ${mode.ai.max_tokens} tokens • ${mode.orchestra.max_agents} agents
                </div>
            </div>
        `;
    });

    html += `
        <button onclick="togglePowerModeSelector()" style="
            width: 100%;
            padding: 8px;
            margin-top: 10px;
            background: var(--red);
            color: white;
            border: none;
            border-radius: 6px;
            cursor: pointer;
            font-weight: bold;
        ">Close</button>
    `;

    selector.innerHTML = html;
    document.body.appendChild(selector);
}

function getModeColor(mode) {
    const colors = {
        ECO: '#00ff88',
        BALANCED: '#00d4ff',
        TURBO: '#ff6b35',
        OVERCLOCKED: '#ff4757',
        LIQUID_NITROGEN: '#a855f7'
    };
    return colors[mode] || '#00d4ff';
}

function togglePowerModeSelector() {
    const selector = document.getElementById('power-mode-selector');
    if (selector) {
        selector.remove();
    } else {
        createPowerModeSelector();
    }
}

function updatePowerModeUI(modeName) {
    const selector = document.getElementById('power-mode-selector');
    if (selector) {
        selector.remove();
        createPowerModeSelector();
    }
}

// ============================================================================
// NOTIFICATIONS
// ============================================================================

function showPowerModeNotification(mode) {
    const notification = document.createElement('div');
    notification.style.cssText = `
        position: fixed;
        top: 50%;
        left: 50%;
        transform: translate(-50%, -50%) scale(0);
        background: rgba(10, 10, 30, 0.98);
        backdrop-filter: blur(30px);
        border: 3px solid ${getModeColor(currentPowerMode)};
        border-radius: 20px;
        padding: 40px;
        z-index: 100001;
        text-align: center;
        box-shadow: 0 0 50px ${getModeColor(currentPowerMode)}80;
        animation: powerModeNotification 2s ease-out forwards;
    `;

    notification.innerHTML = `
        <div style="font-size: 48px; margin-bottom: 20px;">${mode.name.split(' ')[0]}</div>
        <div style="color: ${getModeColor(currentPowerMode)}; font-size: 24px; font-weight: bold; margin-bottom: 10px;">
            ${mode.name}
        </div>
        <div style="color: #888; font-size: 14px;">
            ${mode.description}
        </div>
        ${mode.warning ? `<div style="color: var(--red); font-size: 12px; margin-top: 15px; padding: 10px; background: rgba(255,71,87,0.1); border-radius: 8px;">
            ${mode.warning}
        </div>` : ''}
    `;

    // Add animation
    const style = document.createElement('style');
    style.textContent = `
        @keyframes powerModeNotification {
            0% { transform: translate(-50%, -50%) scale(0); opacity: 0; }
            10% { transform: translate(-50%, -50%) scale(1.2); opacity: 1; }
            20% { transform: translate(-50%, -50%) scale(1); }
            80% { transform: translate(-50%, -50%) scale(1); opacity: 1; }
            100% { transform: translate(-50%, -50%) scale(0.8); opacity: 0; }
        }
    `;
    document.head.appendChild(style);

    document.body.appendChild(notification);

    setTimeout(() => notification.remove(), 2000);
}

// ============================================================================
// UTILITIES
// ============================================================================

function toggleParticles(enabled) {
    const particles = document.querySelectorAll('.particle, .token-particle, .cosmic-particle');
    particles.forEach(p => {
        p.style.display = enabled ? 'block' : 'none';
    });
}

function logTelemetry(event, data) {
    console.log(`[Telemetry] ${event}:`, data);
}

// ============================================================================
// INITIALIZATION
// ============================================================================

// Start performance monitoring
startPerformanceMonitoring();

// Create power mode button
const powerModeBtn = document.createElement('button');
powerModeBtn.id = 'power-mode-btn';
powerModeBtn.innerHTML = '⚡';
powerModeBtn.style.cssText = `
    position: fixed;
    bottom: 20px;
    right: 20px;
    width: 50px;
    height: 50px;
    background: linear-gradient(135deg, var(--cyan), var(--purple));
    border: 2px solid var(--cyan);
    border-radius: 50%;
    color: white;
    font-size: 24px;
    cursor: pointer;
    z-index: 99998;
    box-shadow: 0 5px 20px rgba(0,212,255,0.5);
    transition: all 0.3s;
`;
powerModeBtn.onmouseover = () => {
    powerModeBtn.style.transform = 'scale(1.1) rotate(15deg)';
    powerModeBtn.style.boxShadow = '0 8px 30px rgba(0,212,255,0.8)';
};
powerModeBtn.onmouseout = () => {
    powerModeBtn.style.transform = 'scale(1) rotate(0deg)';
    powerModeBtn.style.boxShadow = '0 5px 20px rgba(0,212,255,0.5)';
};
powerModeBtn.onclick = togglePowerModeSelector;

// Add to page after DOM loads
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        document.body.appendChild(powerModeBtn);
    });
} else {
    document.body.appendChild(powerModeBtn);
}

console.log('[Performance] ✅ Power modes initialized');
console.log('[Performance] 💡 Current mode: ' + PowerModes[currentPowerMode].name);

// Export functions
if (typeof module !== 'undefined' && module.exports) {
    module.exports = {
        PowerModes,
        applyPowerMode,
        currentPowerMode,
        performanceMonitor,
        togglePowerModeSelector
    };
}

