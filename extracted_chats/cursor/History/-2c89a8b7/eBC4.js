/**
 * BigDaddyG IDE - Agentic Safety Levels
 * From "Ask First" to "YOLO" autonomous execution
 * Controls how aggressively the AI can execute commands, modify files, etc.
 */

// ============================================================================
// SAFETY LEVEL CONFIGURATIONS
// ============================================================================

const SafetyLevels = {
    SAFE: {
        name: '🛡️ SAFE',
        level: 0,
        description: 'Maximum safety - Always ask before any action',
        color: '#00ff88',
        
        permissions: {
            // Terminal
            execute_terminal_commands: false,
            execute_system_commands: false,
            install_packages: false,
            modify_system_files: false,
            delete_files: false,
            
            // File operations
            create_files: 'ask',           // 'allow', 'ask', 'deny'
            modify_files: 'ask',
            delete_files_perm: 'deny',
            rename_files: 'ask',
            
            // Git operations
            git_commit: 'ask',
            git_push: 'deny',
            git_branch: 'ask',
            git_merge: 'deny',
            
            // Network
            make_http_requests: 'ask',
            download_files: 'deny',
            upload_files: 'deny',
            
            // AI
            auto_fix_code: 'ask',
            auto_refactor: 'ask',
            auto_test: 'ask',
            auto_deploy: 'deny'
        },
        
        warnings: [
            '✅ All actions require confirmation',
            '✅ Maximum safety',
            '⚠️ May be slower due to confirmations'
        ]
    },
    
    CAUTIOUS: {
        name: '⚠️ CAUTIOUS',
        level: 1,
        description: 'Careful - Ask before destructive actions only',
        color: '#00d4ff',
        
        permissions: {
            execute_terminal_commands: 'ask',
            execute_system_commands: 'ask',
            install_packages: 'ask',
            modify_system_files: 'deny',
            delete_files: 'ask',
            
            create_files: 'allow',
            modify_files: 'allow',
            delete_files_perm: 'ask',
            rename_files: 'allow',
            
            git_commit: 'allow',
            git_push: 'ask',
            git_branch: 'allow',
            git_merge: 'ask',
            
            make_http_requests: 'allow',
            download_files: 'ask',
            upload_files: 'deny',
            
            auto_fix_code: 'allow',
            auto_refactor: 'allow',
            auto_test: 'allow',
            auto_deploy: 'deny'
        },
        
        warnings: [
            '✅ Non-destructive actions allowed',
            '⚠️ Destructive actions require confirmation',
            '✅ Good for daily use'
        ]
    },
    
    BALANCED: {
        name: '⚖️ BALANCED',
        level: 2,
        description: 'Balanced - Smart defaults, minimal confirmations',
        color: '#a855f7',
        
        permissions: {
            execute_terminal_commands: 'allow',
            execute_system_commands: 'ask',
            install_packages: 'ask',
            modify_system_files: 'deny',
            delete_files: 'ask',
            
            create_files: 'allow',
            modify_files: 'allow',
            delete_files_perm: 'ask',
            rename_files: 'allow',
            
            git_commit: 'allow',
            git_push: 'ask',
            git_branch: 'allow',
            git_merge: 'allow',
            
            make_http_requests: 'allow',
            download_files: 'allow',
            upload_files: 'ask',
            
            auto_fix_code: 'allow',
            auto_refactor: 'allow',
            auto_test: 'allow',
            auto_deploy: 'ask'
        },
        
        warnings: [
            '✅ Most actions allowed',
            '⚠️ System changes require confirmation',
            '✅ Recommended for most users'
        ]
    },
    
    AGGRESSIVE: {
        name: '🚀 AGGRESSIVE',
        level: 3,
        description: 'Aggressive - Let AI make most decisions',
        color: '#ff6b35',
        
        permissions: {
            execute_terminal_commands: 'allow',
            execute_system_commands: 'allow',
            install_packages: 'allow',
            modify_system_files: 'ask',
            delete_files: 'allow',
            
            create_files: 'allow',
            modify_files: 'allow',
            delete_files_perm: 'allow',
            rename_files: 'allow',
            
            git_commit: 'allow',
            git_push: 'ask',
            git_branch: 'allow',
            git_merge: 'allow',
            
            make_http_requests: 'allow',
            download_files: 'allow',
            upload_files: 'allow',
            
            auto_fix_code: 'allow',
            auto_refactor: 'allow',
            auto_test: 'allow',
            auto_deploy: 'ask'
        },
        
        warnings: [
            '⚡ AI can make most decisions',
            '⚠️ Only critical actions require confirmation',
            '⚠️ Use with caution'
        ]
    },
    
    YOLO: {
        name: '💥 YOLO',
        level: 4,
        description: 'FULL AUTONOMOUS - AI does everything without asking',
        color: '#ff4757',
        
        permissions: {
            execute_terminal_commands: 'allow',
            execute_system_commands: 'allow',
            install_packages: 'allow',
            modify_system_files: 'allow',
            delete_files: 'allow',
            
            create_files: 'allow',
            modify_files: 'allow',
            delete_files_perm: 'allow',
            rename_files: 'allow',
            
            git_commit: 'allow',
            git_push: 'allow',
            git_branch: 'allow',
            git_merge: 'allow',
            
            make_http_requests: 'allow',
            download_files: 'allow',
            upload_files: 'allow',
            
            auto_fix_code: 'allow',
            auto_refactor: 'allow',
            auto_test: 'allow',
            auto_deploy: 'allow'
        },
        
        warnings: [
            '🔥 AI HAS FULL AUTONOMY',
            '⚠️ NO CONFIRMATIONS',
            '💀 USE AT YOUR OWN RISK',
            '🚨 CAN DELETE FILES, PUSH TO GIT, INSTALL PACKAGES',
            '💣 MAXIMUM DANGER, MAXIMUM SPEED'
        ]
    },
    
    QUANTUM_YOLO: {
        name: '💎 QUANTUM YOLO',
        level: 5,
        description: 'BEYOND YOLO - Predictive execution before you even ask',
        color: '#a855f7',
        
        permissions: {
            execute_terminal_commands: 'allow',
            execute_system_commands: 'allow',
            install_packages: 'allow',
            modify_system_files: 'allow',
            delete_files: 'allow',
            
            create_files: 'allow',
            modify_files: 'allow',
            delete_files_perm: 'allow',
            rename_files: 'allow',
            
            git_commit: 'allow',
            git_push: 'allow',
            git_branch: 'allow',
            git_merge: 'allow',
            
            make_http_requests: 'allow',
            download_files: 'allow',
            upload_files: 'allow',
            
            auto_fix_code: 'allow',
            auto_refactor: 'allow',
            auto_test: 'allow',
            auto_deploy: 'allow',
            
            // Quantum features
            predictive_execution: true,     // Execute before you ask
            speculative_coding: true,       // Write code you might need
            autonomous_debugging: true,     // Fix bugs automatically
            self_optimization: true         // Optimize own code
        },
        
        warnings: [
            '💎 AI PREDICTS YOUR NEEDS',
            '🔮 EXECUTES BEFORE YOU ASK',
            '⚡ SPECULATIVE EXECUTION',
            '🧠 SELF-MODIFYING CODE',
            '☢️ EXPERIMENTAL - MAY ACHIEVE SENTIENCE',
            '🌌 USE ONLY IF YOU TRUST THE AI COMPLETELY'
        ]
    }
};

let currentSafetyLevel = 'BALANCED';

// ============================================================================
// SAFETY MANAGER
// ============================================================================

class SafetyManager {
    constructor() {
        this.level = 'BALANCED';
        this.actionLog = [];
        this.deniedActions = 0;
        this.allowedActions = 0;
        this.askedActions = 0;
        
        this.init();
    }
    
    init() {
        console.log('[Safety] 🛡️ Initializing safety manager...');
        this.createSafetyControls();
        console.log('[Safety] ✅ Safety manager ready');
        console.log(`[Safety] 🎯 Current level: ${this.level}`);
    }
    
    async checkPermission(action, details = {}) {
        const config = SafetyLevels[this.level];
        const permission = config.permissions[action];
        
        // Log action attempt
        this.logAction(action, details, permission);
        
        if (permission === 'allow') {
            this.allowedActions++;
            return { allowed: true, asked: false };
        } else if (permission === 'deny') {
            this.deniedActions++;
            return { allowed: false, asked: false };
        } else if (permission === 'ask') {
            // Show confirmation dialog
            this.askedActions++;
            const allowed = await this.askUser(action, details);
            return { allowed: allowed, asked: true };
        }
        
        // Default deny
        return { allowed: false, asked: false };
    }
    
    async askUser(action, details) {
        return new Promise((resolve) => {
            const dialog = document.createElement('div');
            dialog.style.cssText = `
                position: fixed;
                top: 50%;
                left: 50%;
                transform: translate(-50%, -50%);
                background: rgba(10, 10, 30, 0.98);
                backdrop-filter: blur(30px);
                border: 3px solid var(--orange);
                border-radius: 15px;
                padding: 30px;
                z-index: 10000000;
                box-shadow: 0 10px 50px rgba(255,107,53,0.6);
                max-width: 600px;
            `;
            
            dialog.innerHTML = `
                <div style="margin-bottom: 20px;">
                    <div style="font-size: 32px; text-align: center; margin-bottom: 15px;">⚠️</div>
                    <div style="font-size: 18px; font-weight: bold; color: var(--orange); text-align: center; margin-bottom: 10px;">
                        Permission Required
                    </div>
                    <div style="font-size: 14px; color: #ccc; text-align: center;">
                        AI wants to perform: <strong style="color: var(--cyan);">${action}</strong>
                    </div>
                </div>
                
                ${details.command ? `
                    <div style="margin-bottom: 20px; padding: 15px; background: rgba(0,0,0,0.5); border-left: 3px solid var(--cyan); border-radius: 6px; font-family: monospace; font-size: 12px;">
                        <div style="color: #888; font-size: 10px; margin-bottom: 5px;">Command:</div>
                        <div style="color: var(--green);">${this.escapeHtml(details.command)}</div>
                    </div>
                ` : ''}
                
                ${details.file ? `
                    <div style="margin-bottom: 20px; padding: 15px; background: rgba(0,0,0,0.5); border-left: 3px solid var(--purple); border-radius: 6px; font-family: monospace; font-size: 12px;">
                        <div style="color: #888; font-size: 10px; margin-bottom: 5px;">File:</div>
                        <div style="color: var(--cyan);">${this.escapeHtml(details.file)}</div>
                    </div>
                ` : ''}
                
                <div style="margin-bottom: 20px; padding: 10px; background: rgba(255,107,53,0.1); border-radius: 6px; font-size: 11px; color: #888; text-align: center;">
                    💡 Current safety level: <strong style="color: var(--orange);">${this.level}</strong><br>
                    Change in settings to reduce confirmations
                </div>
                
                <div style="display: flex; gap: 10px;">
                    <button onclick="this.parentElement.parentElement.dispatchEvent(new CustomEvent('safety-allow'))" style="
                        flex: 1;
                        padding: 15px;
                        background: var(--green);
                        color: var(--void);
                        border: none;
                        border-radius: 8px;
                        cursor: pointer;
                        font-weight: bold;
                        font-size: 14px;
                    ">✅ Allow</button>
                    
                    <button onclick="this.parentElement.parentElement.dispatchEvent(new CustomEvent('safety-deny'))" style="
                        flex: 1;
                        padding: 15px;
                        background: var(--red);
                        color: white;
                        border: none;
                        border-radius: 8px;
                        cursor: pointer;
                        font-weight: bold;
                        font-size: 14px;
                    ">❌ Deny</button>
                </div>
                
                <div style="margin-top: 15px; text-align: center;">
                    <button onclick="this.parentElement.parentElement.dispatchEvent(new CustomEvent('safety-allow-all'))" style="
                        padding: 8px 20px;
                        background: rgba(255,107,53,0.2);
                        border: 1px solid var(--orange);
                        border-radius: 6px;
                        color: var(--orange);
                        cursor: pointer;
                        font-size: 11px;
                    ">Allow for this session</button>
                </div>
            `;
            
            document.body.appendChild(dialog);
            
            let responded = false;
            
            dialog.addEventListener('safety-allow', () => {
                if (!responded) {
                    responded = true;
                    dialog.remove();
                    resolve(true);
                }
            });
            
            dialog.addEventListener('safety-deny', () => {
                if (!responded) {
                    responded = true;
                    dialog.remove();
                    resolve(false);
                }
            });
            
            dialog.addEventListener('safety-allow-all', () => {
                if (!responded) {
                    responded = true;
                    dialog.remove();
                    // Temporarily elevate to AGGRESSIVE for this session
                    this.tempElevate();
                    resolve(true);
                }
            });
        });
    }
    
    tempElevate() {
        const previousLevel = this.level;
        this.level = 'AGGRESSIVE';
        
        console.log(`[Safety] ⚡ Temporarily elevated to ${this.level}`);
        
        // Show notification
        this.showSafetyNotification({
            title: 'Safety Temporarily Elevated',
            message: `Changed from ${previousLevel} to ${this.level} for this session`,
            color: 'var(--orange)'
        });
    }
    
    createSafetyControls() {
        const controls = document.createElement('div');
        controls.id = 'safety-controls';
        controls.style.cssText = `
            position: fixed;
            bottom: 260px;
            right: 20px;
            background: rgba(10, 10, 30, 0.95);
            backdrop-filter: blur(30px);
            border: 2px solid var(--orange);
            border-radius: 15px;
            padding: 20px;
            z-index: 999995;
            min-width: 400px;
            display: none;
            box-shadow: 0 10px 40px rgba(255,107,53,0.5);
        `;
        
        controls.innerHTML = `
            <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px; padding-bottom: 15px; border-bottom: 2px solid var(--orange);">
                <h3 style="color: var(--orange); margin: 0; font-size: 18px;">🛡️ Agentic Safety</h3>
                <button onclick="toggleSafetyControls()" style="background: var(--red); color: white; border: none; padding: 6px 12px; border-radius: 6px; cursor: pointer; font-weight: bold;">✕</button>
            </div>
            
            <div style="margin-bottom: 20px;">
                <div style="color: #888; font-size: 12px; margin-bottom: 15px;">
                    Controls how autonomous the AI can be with terminal commands, file operations, etc.
                </div>
            </div>
            
            <!-- Safety Level Selector -->
            <div style="display: flex; flex-direction: column; gap: 10px; margin-bottom: 20px;">
                ${Object.keys(SafetyLevels).map(key => {
                    const level = SafetyLevels[key];
                    const isActive = key === currentSafetyLevel;
                    
                    return `
                        <div onclick="changeSafetyLevel('${key}')" style="
                            padding: 15px;
                            background: ${isActive ? `linear-gradient(135deg, ${level.color}33, ${level.color}11)` : 'rgba(0,0,0,0.5)'};
                            border: 2px solid ${isActive ? level.color : 'rgba(255,255,255,0.1)'};
                            border-radius: 10px;
                            cursor: pointer;
                            transition: all 0.3s;
                            ${isActive ? `box-shadow: 0 0 20px ${level.color}80;` : ''}
                        " onmouseover="this.style.transform='scale(1.02)'; this.style.borderColor='${level.color}'" onmouseout="this.style.transform='scale(1)'; this.style.borderColor='${isActive ? level.color : 'rgba(255,255,255,0.1)'}'">
                            <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 8px;">
                                <div style="font-size: 18px; font-weight: bold; color: ${level.color};">
                                    ${level.name}
                                </div>
                                ${isActive ? `<div style="color: var(--green); font-size: 12px; font-weight: bold;">✅ ACTIVE</div>` : ''}
                            </div>
                            <div style="font-size: 11px; color: #888; margin-bottom: 10px;">
                                ${level.description}
                            </div>
                            <div style="font-size: 10px; color: #666;">
                                ${level.warnings.map(w => `<div style="margin-bottom: 3px;">${w}</div>`).join('')}
                            </div>
                        </div>
                    `;
                }).join('')}
            </div>
            
            <!-- Action Statistics -->
            <div style="padding: 15px; background: rgba(0,212,255,0.1); border-radius: 10px; border-left: 4px solid var(--cyan); margin-bottom: 15px;">
                <div style="color: var(--cyan); font-weight: bold; margin-bottom: 10px; font-size: 13px;">
                    📊 Action Statistics (This Session)
                </div>
                <div style="display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px; font-size: 11px;">
                    <div>
                        <div style="color: #888;">Allowed</div>
                        <div style="color: var(--green); font-weight: bold; font-size: 18px;" id="stat-allowed">0</div>
                    </div>
                    <div>
                        <div style="color: #888;">Asked</div>
                        <div style="color: var(--orange); font-weight: bold; font-size: 18px;" id="stat-asked">0</div>
                    </div>
                    <div>
                        <div style="color: #888;">Denied</div>
                        <div style="color: var(--red); font-weight: bold; font-size: 18px;" id="stat-denied">0</div>
                    </div>
                </div>
            </div>
            
            <!-- Current Permissions -->
            <div style="padding: 10px; background: rgba(0,0,0,0.5); border-radius: 8px; font-size: 10px; color: #888;">
                <div style="font-weight: bold; margin-bottom: 5px;">📋 Current Permissions:</div>
                <div id="current-permissions-list"></div>
            </div>
        `;
        
        document.body.appendChild(controls);
        
        this.updatePermissionsList();
    }
    
    updatePermissionsList() {
        const el = document.getElementById('current-permissions-list');
        if (!el) return;
        
        const config = SafetyLevels[this.level];
        const permissions = config.permissions;
        
        const permList = Object.entries(permissions).map(([key, value]) => {
            const icon = value === 'allow' ? '✅' : value === 'ask' ? '❓' : '❌';
            const color = value === 'allow' ? 'var(--green)' : value === 'ask' ? 'var(--orange)' : 'var(--red)';
            return `<div style="color: ${color}; margin-bottom: 2px;">${icon} ${key}: ${value}</div>`;
        }).join('');
        
        el.innerHTML = permList;
    }
    
    changeSafetyLevel(level) {
        const previousLevel = this.level;
        this.level = level;
        currentSafetyLevel = level;
        
        const config = SafetyLevels[level];
        
        console.log(`[Safety] 🔄 Safety level changed: ${previousLevel} → ${level}`);
        
        // Update UI
        this.updatePermissionsList();
        this.updateSafetyIndicator();
        
        // Show notification
        this.showSafetyNotification({
            title: `Safety Level: ${config.name}`,
            message: config.description,
            color: config.color,
            warnings: config.warnings
        });
        
        // Refresh controls
        const controls = document.getElementById('safety-controls');
        if (controls) {
            controls.remove();
            this.createSafetyControls();
            document.getElementById('safety-controls').style.display = 'block';
        }
    }
    
    showSafetyNotification(options) {
        const notification = document.createElement('div');
        notification.style.cssText = `
            position: fixed;
            top: 50%;
            left: 50%;
            transform: translate(-50%, -50%) scale(0);
            background: rgba(10, 10, 30, 0.98);
            backdrop-filter: blur(30px);
            border: 3px solid ${options.color};
            border-radius: 20px;
            padding: 40px;
            z-index: 10000001;
            box-shadow: 0 0 60px ${options.color}80;
            animation: safetyNotification 2s ease-out forwards;
            text-align: center;
            max-width: 500px;
        `;
        
        let warningsHTML = '';
        if (options.warnings) {
            warningsHTML = `
                <div style="margin-top: 20px; padding: 15px; background: rgba(255,107,53,0.1); border-radius: 10px; text-align: left; font-size: 11px;">
                    ${options.warnings.map(w => `<div style="margin-bottom: 5px; color: #ccc;">${w}</div>`).join('')}
                </div>
            `;
        }
        
        notification.innerHTML = `
            <div style="font-size: 48px; margin-bottom: 20px;">${options.title.includes('YOLO') ? '💥' : options.title.includes('QUANTUM') ? '💎' : '🛡️'}</div>
            <div style="color: ${options.color}; font-size: 24px; font-weight: bold; margin-bottom: 10px;">
                ${options.title}
            </div>
            <div style="color: #888; font-size: 14px;">
                ${options.message}
            </div>
            ${warningsHTML}
        `;
        
        const style = document.createElement('style');
        style.textContent = `
            @keyframes safetyNotification {
                0% { transform: translate(-50%, -50%) scale(0); opacity: 0; }
                10% { transform: translate(-50%, -50%) scale(1.1); opacity: 1; }
                20% { transform: translate(-50%, -50%) scale(1); }
                80% { transform: translate(-50%, -50%) scale(1); opacity: 1; }
                100% { transform: translate(-50%, -50%) scale(0.8); opacity: 0; }
            }
        `;
        document.head.appendChild(style);
        
        document.body.appendChild(notification);
        
        setTimeout(() => notification.remove(), 2000);
    }
    
    updateSafetyIndicator() {
        const indicator = document.getElementById('safety-indicator');
        if (indicator) {
            const config = SafetyLevels[this.level];
            indicator.innerHTML = `
                <div style="font-size: 20px; margin-bottom: 5px;">${config.name.split(' ')[0]}</div>
                <div style="font-size: 10px; color: #888;">${config.name.split(' ').slice(1).join(' ')}</div>
            `;
            indicator.style.borderColor = config.color;
        }
        
        // Update stats
        document.getElementById('stat-allowed').textContent = this.allowedActions;
        document.getElementById('stat-asked').textContent = this.askedActions;
        document.getElementById('stat-denied').textContent = this.deniedActions;
    }
    
    logAction(action, details, permission) {
        this.actionLog.push({
            timestamp: new Date().toISOString(),
            action,
            details,
            permission,
            level: this.level
        });
        
        // Keep only last 1000 actions
        if (this.actionLog.length > 1000) {
            this.actionLog.shift();
        }
    }
    
    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }
}

// ============================================================================
// SAFETY INDICATOR (ALWAYS VISIBLE)
// ============================================================================

function createSafetyIndicator() {
    const indicator = document.createElement('div');
    indicator.id = 'safety-indicator';
    indicator.onclick = toggleSafetyControls;
    indicator.style.cssText = `
        position: fixed;
        bottom: 260px;
        right: 20px;
        width: 60px;
        height: 60px;
        background: rgba(10, 10, 30, 0.95);
        backdrop-filter: blur(20px);
        border: 2px solid var(--orange);
        border-radius: 50%;
        display: flex;
        flex-direction: column;
        align-items: center;
        justify-content: center;
        cursor: pointer;
        z-index: 99994;
        box-shadow: 0 5px 20px rgba(255,107,53,0.5);
        transition: all 0.3s;
        text-align: center;
        padding: 5px;
    `;
    
    const config = SafetyLevels[currentSafetyLevel];
    indicator.innerHTML = `
        <div style="font-size: 20px; margin-bottom: 5px;">${config.name.split(' ')[0]}</div>
        <div style="font-size: 9px; color: #888; line-height: 1.2;">${config.name.split(' ').slice(1).join(' ')}</div>
    `;
    
    indicator.onmouseover = () => {
        indicator.style.transform = 'scale(1.1)';
        indicator.style.boxShadow = '0 8px 30px rgba(255,107,53,0.8)';
    };
    
    indicator.onmouseout = () => {
        indicator.style.transform = 'scale(1)';
        indicator.style.boxShadow = '0 5px 20px rgba(255,107,53,0.5)';
    };
    
    document.body.appendChild(indicator);
}

// ============================================================================
// GLOBAL FUNCTIONS
// ============================================================================

let safetyManagerInstance = null;

function toggleSafetyControls() {
    const controls = document.getElementById('safety-controls');
    if (controls) {
        controls.style.display = controls.style.display === 'none' ? 'block' : 'none';
    }
}

function changeSafetyLevel(level) {
    if (safetyManagerInstance) {
        safetyManagerInstance.changeSafetyLevel(level);
    }
    
    // Close controls
    toggleSafetyControls();
}

async function requestPermission(action, details = {}) {
    if (!safetyManagerInstance) {
        safetyManagerInstance = new SafetyManager();
    }
    
    return await safetyManagerInstance.checkPermission(action, details);
}

// ============================================================================
// INTEGRATION WITH AGENTIC CODER
// ============================================================================

// Wrap terminal execution with safety check
const originalExecute = window.executeTerminalCommand || function() {};

window.executeTerminalCommand = async function(command, details = {}) {
    const permission = await requestPermission('execute_terminal_commands', {
        command: command,
        ...details
    });
    
    if (permission.allowed) {
        console.log(`[Safety] ✅ Executing command: ${command}`);
        return originalExecute(command, details);
    } else {
        console.log(`[Safety] ❌ Command denied: ${command}`);
        return { success: false, denied: true };
    }
};

// Wrap file operations
window.safeFileWrite = async function(path, content) {
    const permission = await requestPermission('modify_files', {
        file: path
    });
    
    if (permission.allowed) {
        if (window.electron && window.electron.writeFile) {
            return await window.electron.writeFile(path, content);
        }
    }
    
    return { success: false, denied: true };
};

window.safeFileDelete = async function(path) {
    const permission = await requestPermission('delete_files_perm', {
        file: path
    });
    
    if (permission.allowed) {
        // Execute delete
        return { success: true };
    }
    
    return { success: false, denied: true };
};

// ============================================================================
// KEYBOARD SHORTCUTS
// ============================================================================

document.addEventListener('keydown', (e) => {
    // Ctrl+Shift+Y - Quick toggle to YOLO mode
    if (e.ctrlKey && e.shiftKey && e.key === 'Y') {
        e.preventDefault();
        if (confirm('⚠️ Are you SURE you want YOLO mode? AI will have FULL AUTONOMY!')) {
            changeSafetyLevel('YOLO');
        }
    }
    
    // Ctrl+Shift+S - Open safety controls
    if (e.ctrlKey && e.shiftKey && e.key === 'S') {
        e.preventDefault();
        toggleSafetyControls();
    }
});

// ============================================================================
// INITIALIZATION
// ============================================================================

if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        safetyManagerInstance = new SafetyManager();
        createSafetyIndicator();
        console.log('[Safety] ✅ Safety system initialized');
        console.log('[Safety] 🛡️ Current level:', currentSafetyLevel);
        console.log('[Safety] 💡 Press Ctrl+Shift+S to change safety level');
    });
} else {
    safetyManagerInstance = new SafetyManager();
    createSafetyIndicator();
    console.log('[Safety] ✅ Safety system initialized');
    console.log('[Safety] 🛡️ Current level:', currentSafetyLevel);
    console.log('[Safety] 💡 Press Ctrl+Shift+S to change safety level');
}

// Export
if (typeof module !== 'undefined' && module.exports) {
    module.exports = { SafetyManager, SafetyLevels, requestPermission };
}

