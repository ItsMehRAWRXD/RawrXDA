# Fix script tags in HTML files
param(
    [string]$Path = "d:\itsmehrawrxd-dependabot-npm_and_yarn-finished-local-deployment-npm_and_yarn-937dd3532d\public"
)

$files = Get-ChildItem -Path $Path -Filter "*.html" | Where-Object { $_.Name -notin @("panel.html", "beaconism-panel.html", "encryption-panel.html") }

foreach ($file in $files) {
    Write-Host "Fixing $($file.Name)..."

    $content = Get-Content $file.FullName -Raw

    # Fix the script tag - should be self-closing
    $content = $content -replace '<script src="agentic-beacon-framework\.js">[\s\S]*?</script>', '<script src="agentic-beacon-framework.js"></script>'

    # Add the JavaScript functions in a separate script tag before the closing body tag
    $agenticJS = @"
    <script>
        // ===== AGENTIC AUTONOMOUS WIN32 FUNCTIONS =====

        // Initialize agentic controls and beacon connectivity
        async function initializeAgenticControls() {
            try {
                // Initialize the AgenticBeaconManager
                await window.agenticBeaconManager.initialize();

                // Establish circular connectivity with other panels
                await window.agenticBeaconManager.establishCircularConnectivity();

                // Update beacon status display
                updateBeaconStatus();

                // Start autonomous monitoring
                startAutonomousMonitoring();

                // Update status indicator
                const indicator = document.getElementById('agentic-status-indicator');
                const text = document.getElementById('agentic-status-text');
                if (indicator) indicator.classList.remove('offline');
                if (text) text.textContent = 'Active';

                console.log('Agentic controls initialized successfully');
            } catch (error) {
                console.error('Failed to initialize agentic controls:', error);
                const indicator = document.getElementById('agentic-status-indicator');
                const text = document.getElementById('agentic-status-text');
                if (indicator) indicator.classList.add('offline');
                if (text) text.textContent = 'Error';
            }
        }

        // Update beacon connectivity status display
        function updateBeaconStatus() {
            const beaconGrid = document.getElementById('beacon-grid');
            if (!beaconGrid) return;

            const beacons = window.agenticBeaconManager.getBeaconStatus();
            let html = '';

            Object.entries(beacons).forEach(([type, status]) => {
                const statusClass = status.connected ? 'beacon-connected' : 'beacon-disconnected';
                const statusText = status.connected ? 'Connected' : 'Disconnected';

                html += `<div class="beacon-item ${statusClass}">` +
                        `<div class="beacon-icon">${getBeaconIcon(type)}</div>` +
                        `<div class="beacon-info">` +
                        `<div class="beacon-type">${type.toUpperCase()}</div>` +
                        `<div class="beacon-status">${statusText}</div>` +
                        `</div></div>`;
            });

            beaconGrid.innerHTML = html;
        }

        // Get icon for beacon type
        function getBeaconIcon(type) {
            const icons = {
                'encryption': '🔐',
                'native': '⚙️',
                'java': '☕',
                'dotnet': '🔷',
                'web': '🌐'
            };
            return icons[type] || '📡';
        }

        // Start autonomous monitoring loop
        function startAutonomousMonitoring() {
            setInterval(async () => {
                try {
                    // Check beacon connectivity
                    await window.agenticBeaconManager.checkBeaconHealth();

                    // Update status display
                    updateBeaconStatus();

                    // Perform autonomous actions if enabled
                    const checkbox = document.getElementById('autonomous-mode');
                    if (checkbox && checkbox.checked) {
                        await performAutonomousActions();
                    }
                } catch (error) {
                    console.error('Autonomous monitoring error:', error);
                }
            }, 5000);
        }

        // Perform autonomous agentic actions
        async function performAutonomousActions() {
            try {
                const systemStatus = await window.agenticBeaconManager.getSystemStatus();

                // Auto-heal disconnected beacons
                for (const [type, status] of Object.entries(systemStatus.beacons)) {
                    if (!status.connected) {
                        console.log(`Auto-healing ${type} beacon...`);
                        await window.agenticBeaconManager.reconnectBeacon(type);
                    }
                }

                // Auto-optimize performance
                if (systemStatus.performance.cpu > 80) {
                    await window.agenticBeaconManager.optimizePerformance('cpu');
                }

                if (systemStatus.performance.memory > 85) {
                    await window.agenticBeaconManager.optimizePerformance('memory');
                }
            } catch (error) {
                console.error('Autonomous action error:', error);
            }
        }

        // Execute Win32 agentic operation
        async function executeWin32Operation(operation, params = {}) {
            try {
                const result = await window.agenticBeaconManager.executeWin32Operation(operation, params);
                displayAgenticResult('Win32 Operation', operation, result);
                updateBeaconStatus();
                return result;
            } catch (error) {
                console.error(`Win32 operation failed: ${error.message}`);
                throw error;
            }
        }

        // Apply hot patch to target
        async function applyHotPatch(targetType, targetId, patchData) {
            try {
                const result = await window.agenticBeaconManager.applyHotPatch(targetType, targetId, patchData);
                displayAgenticResult('Hot Patch Applied', `${targetType}:${targetId}`, result);
                updateBeaconStatus();
                return result;
            } catch (error) {
                console.error(`Hot patch failed: ${error.message}`);
                throw error;
            }
        }

        // Display agentic operation results
        function displayAgenticResult(operation, details, result) {
            const output = document.getElementById('agentic-output');
            if (!output) return;

            const timestamp = new Date().toLocaleTimeString();
            const html = `<div class="agentic-result">` +
                        `<div class="agentic-header">` +
                        `<span class="agentic-timestamp">${timestamp}</span>` +
                        `<span class="agentic-operation">${operation}</span>` +
                        `</div>` +
                        `<div class="agentic-details">${details}</div>` +
                        `<div class="agentic-data">` +
                        `<pre>${JSON.stringify(result, null, 2)}</pre>` +
                        `</div></div>`;

            output.innerHTML = html + output.innerHTML;
        }

        // Manual beacon reconnection
        async function reconnectBeacon(beaconType) {
            try {
                const result = await window.agenticBeaconManager.reconnectBeacon(beaconType);
                displayAgenticResult('Beacon Reconnected', beaconType, result);
                updateBeaconStatus();
            } catch (error) {
                console.error(`Beacon reconnection failed: ${error.message}`);
            }
        }

        // Get comprehensive system status
        async function getSystemStatus() {
            try {
                const status = await window.agenticBeaconManager.getSystemStatus();
                displayAgenticResult('System Status', 'Full Report', status);
                return status;
            } catch (error) {
                console.error(`System status check failed: ${error.message}`);
                throw error;
            }
        }

        // Emergency system lockdown
        async function emergencyLockdown() {
            if (!confirm('Emergency lockdown? This will disconnect all beacons.')) {
                return;
            }

            try {
                const result = await window.agenticBeaconManager.emergencyLockdown();
                displayAgenticResult('Emergency Lockdown', 'Executed', result);

                const checkbox = document.getElementById('autonomous-mode');
                if (checkbox) checkbox.checked = false;

                updateBeaconStatus();
                alert('Emergency lockdown completed.');
            } catch (error) {
                console.error(`Emergency lockdown failed: ${error.message}`);
            }
        }

        // Toggle autonomous mode
        function toggleAutonomousMode() {
            const checkbox = document.getElementById('autonomous-mode');
            const status = checkbox.checked ? 'Autonomous Mode Enabled' : 'Autonomous Mode Disabled';
            displayAgenticResult('Mode Change', status, { autonomous: checkbox.checked });
        }

        // Initialize agentic controls on page load
        document.addEventListener('DOMContentLoaded', function () {
            initializeAgenticControls();
        });
    </script>
"@

    # Add the JavaScript before the closing body tag
    $content = $content -replace '</body>', "$agenticJS`n</body>"

    # Save the updated content
    Set-Content -Path $file.FullName -Value $content -Encoding UTF8

    Write-Host "  Fixed $($file.Name)"
}

Write-Host "Fix script completed!"