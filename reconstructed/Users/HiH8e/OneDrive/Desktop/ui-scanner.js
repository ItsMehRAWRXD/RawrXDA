const puppeteer = require('puppeteer');
const path = require('path');
const fs = require('fs');

async function scanUIBlocking() {
    const htmlFilePath = path.resolve('c:\\Users\\HiH8e\\OneDrive\\Desktop\\IDEre2.html');
    const fileUrl = `file://${htmlFilePath}`;

    const browser = await puppeteer.launch({ 
        headless: false,
        defaultViewport: { width: 1920, height: 1080 }
    });
    const page = await browser.newPage();

    const report = {
        timestamp: new Date().toISOString(),
        issues: [],
        overlappingElements: [],
        hiddenElements: [],
        zIndexConflicts: [],
        cssBlocking: [],
        layoutShifts: []
    };

    // Listen for console errors
    page.on('console', msg => {
        const text = msg.text();
        if (text.includes('❌') || text.includes('⚠️') || text.includes('error')) {
            report.issues.push({ type: 'console', message: text });
        }
    });

    try {
        await page.goto(fileUrl, { waitUntil: 'domcontentloaded', timeout: 30000 });
        
        // Wait for page to stabilize
        await page.evaluate(() => new Promise(resolve => setTimeout(resolve, 5000)));

        // Scan for UI blocking issues
        const uiAnalysis = await page.evaluate(() => {
            const results = {
                activityBar: {},
                sidebar: {},
                statusBar: {},
                menuBar: {},
                panels: {},
                editors: {},
                overlaps: [],
                hidden: [],
                zIndexIssues: []
            };

            // Helper function to get element info
            function getElementInfo(el) {
                if (!el) return null;
                const rect = el.getBoundingClientRect();
                const styles = window.getComputedStyle(el);
                return {
                    id: el.id,
                    className: el.className,
                    rect: {
                        x: rect.x,
                        y: rect.y,
                        width: rect.width,
                        height: rect.height,
                        top: rect.top,
                        left: rect.left,
                        right: rect.right,
                        bottom: rect.bottom
                    },
                    computed: {
                        display: styles.display,
                        visibility: styles.visibility,
                        opacity: styles.opacity,
                        zIndex: styles.zIndex,
                        position: styles.position,
                        overflow: styles.overflow,
                        pointerEvents: styles.pointerEvents,
                        backgroundColor: styles.backgroundColor,
                        color: styles.color
                    }
                };
            }

            // Check for overlapping elements
            function checkOverlap(el1, el2) {
                const r1 = el1.getBoundingClientRect();
                const r2 = el2.getBoundingClientRect();
                return !(r1.right < r2.left || 
                         r1.left > r2.right || 
                         r1.bottom < r2.top || 
                         r1.top > r2.bottom);
            }

            // Scan Activity Bar
            const activityBar = document.querySelector('.activity-bar, #activity-bar, [class*="activity"]');
            if (activityBar) {
                results.activityBar = getElementInfo(activityBar);
                results.activityBar.buttons = Array.from(activityBar.querySelectorAll('button, .activity-item, .action-item'))
                    .map(btn => getElementInfo(btn));
            } else {
                results.activityBar = { error: 'Activity bar not found' };
            }

            // Scan Sidebar
            const sidebar = document.querySelector('.sidebar, #sidebar, .side-panel');
            if (sidebar) {
                results.sidebar = getElementInfo(sidebar);
                results.sidebar.tabs = Array.from(sidebar.querySelectorAll('.tab, .sidebar-tab'))
                    .map(tab => getElementInfo(tab));
            } else {
                results.sidebar = { error: 'Sidebar not found' };
            }

            // Scan Status Bar
            const statusBar = document.querySelector('.status-bar, #status-bar, .footer');
            if (statusBar) {
                results.statusBar = getElementInfo(statusBar);
                results.statusBar.items = Array.from(statusBar.querySelectorAll('.status-item, .status-bar-item'))
                    .map(item => getElementInfo(item));
            } else {
                results.statusBar = { error: 'Status bar not found' };
            }

            // Scan Menu Bar
            const menuBar = document.querySelector('.menu-bar, #menu-bar, .top-bar');
            if (menuBar) {
                results.menuBar = getElementInfo(menuBar);
                results.menuBar.items = Array.from(menuBar.querySelectorAll('.menu-item, button'))
                    .map(item => getElementInfo(item));
            } else {
                results.menuBar = { error: 'Menu bar not found' };
            }

            // Scan Terminal/Panel
            const panel = document.querySelector('.terminal, #terminal, .panel, .bottom-panel');
            if (panel) {
                results.panels = getElementInfo(panel);
            } else {
                results.panels = { error: 'Panel not found' };
            }

            // Scan Editor area
            const editor = document.querySelector('.editor, #editor, .monaco-editor, textarea');
            if (editor) {
                results.editors = getElementInfo(editor);
            } else {
                results.editors = { error: 'Editor not found' };
            }

            // Find all interactive elements
            const allInteractive = Array.from(document.querySelectorAll('button, a, input, textarea, select, [tabindex], [onclick], .clickable'));
            
            // Check for overlapping elements
            for (let i = 0; i < allInteractive.length; i++) {
                for (let j = i + 1; j < allInteractive.length; j++) {
                    if (checkOverlap(allInteractive[i], allInteractive[j])) {
                        const z1 = parseInt(window.getComputedStyle(allInteractive[i]).zIndex) || 0;
                        const z2 = parseInt(window.getComputedStyle(allInteractive[j]).zIndex) || 0;
                        results.overlaps.push({
                            element1: getElementInfo(allInteractive[i]),
                            element2: getElementInfo(allInteractive[j]),
                            zIndexConflict: z1 === z2
                        });
                    }
                }
            }

            // Find hidden/invisible elements that should be visible
            const potentiallyHidden = Array.from(document.querySelectorAll('[id], [class]'));
            potentiallyHidden.forEach(el => {
                const styles = window.getComputedStyle(el);
                const rect = el.getBoundingClientRect();
                
                if (
                    styles.display === 'none' ||
                    styles.visibility === 'hidden' ||
                    parseFloat(styles.opacity) === 0 ||
                    rect.width === 0 ||
                    rect.height === 0
                ) {
                    // Check if it's supposed to be visible (has content or is interactive)
                    if (el.textContent.trim() || 
                        el.tagName === 'BUTTON' || 
                        el.tagName === 'INPUT' ||
                        el.hasAttribute('onclick')) {
                        results.hidden.push({
                            info: getElementInfo(el),
                            reason: styles.display === 'none' ? 'display:none' :
                                   styles.visibility === 'hidden' ? 'visibility:hidden' :
                                   parseFloat(styles.opacity) === 0 ? 'opacity:0' :
                                   'zero dimensions'
                        });
                    }
                }
            });

            // Find z-index conflicts
            const zIndexElements = Array.from(document.querySelectorAll('[style*="z-index"], .modal, .overlay, .dropdown, .popup'));
            const zIndexMap = {};
            zIndexElements.forEach(el => {
                const z = parseInt(window.getComputedStyle(el).zIndex) || 0;
                if (!zIndexMap[z]) zIndexMap[z] = [];
                zIndexMap[z].push(getElementInfo(el));
            });
            
            Object.keys(zIndexMap).forEach(z => {
                if (zIndexMap[z].length > 1) {
                    results.zIndexIssues.push({
                        zIndex: z,
                        elements: zIndexMap[z]
                    });
                }
            });

            return results;
        });

        report.uiAnalysis = uiAnalysis;

        // Check for CSS blocking
        const cssResources = await page.evaluate(() => {
            return Array.from(document.styleSheets).map(sheet => {
                try {
                    return {
                        href: sheet.href,
                        disabled: sheet.disabled,
                        ruleCount: sheet.cssRules ? sheet.cssRules.length : 0
                    };
                } catch (e) {
                    return {
                        href: sheet.href,
                        error: 'Cannot access rules (CORS?)'
                    };
                }
            });
        });
        report.cssResources = cssResources;

        // Take screenshots of key UI elements
        const screenshots = {};
        
        try {
            await page.screenshot({ 
                path: 'c:\\Users\\HiH8e\\OneDrive\\Desktop\\ui-full-scan.png',
                fullPage: true 
            });
            screenshots.fullPage = 'ui-full-scan.png';
        } catch (e) {
            report.issues.push({ type: 'screenshot', message: `Failed to capture full page: ${e.message}` });
        }

        // Analyze layout shifts
        const layoutShifts = await page.evaluate(() => {
            return new Promise((resolve) => {
                const shifts = [];
                const observer = new PerformanceObserver((list) => {
                    for (const entry of list.getEntries()) {
                        if (entry.hadRecentInput) continue;
                        shifts.push({
                            value: entry.value,
                            sources: entry.sources.map(s => ({
                                node: s.node ? s.node.tagName : 'unknown',
                                previousRect: s.previousRect,
                                currentRect: s.currentRect
                            }))
                        });
                    }
                });
                
                try {
                    observer.observe({ entryTypes: ['layout-shift'] });
                    setTimeout(() => {
                        observer.disconnect();
                        resolve(shifts);
                    }, 3000);
                } catch (e) {
                    resolve([{ error: 'PerformanceObserver not supported' }]);
                }
            });
        });
        report.layoutShifts = layoutShifts;

    } catch (error) {
        report.issues.push({ 
            type: 'critical', 
            message: `Failed to analyze page: ${error.message}` 
        });
    }

    // Generate detailed report
    const reportPath = 'c:\\Users\\HiH8e\\OneDrive\\Desktop\\ui-scan-report.json';
    fs.writeFileSync(reportPath, JSON.stringify(report, null, 2));

    // Generate human-readable summary
    const summary = generateSummary(report);
    const summaryPath = 'c:\\Users\\HiH8e\\OneDrive\\Desktop\\ui-scan-summary.txt';
    fs.writeFileSync(summaryPath, summary);

    console.log('\n' + summary);
    console.log(`\nDetailed report saved to: ${reportPath}`);
    console.log(`Summary saved to: ${summaryPath}`);

    await browser.close();
}

function generateSummary(report) {
    let summary = '=== UI BLOCKING & OVERLAP SCAN REPORT ===\n\n';
    summary += `Scan Time: ${report.timestamp}\n\n`;

    // Activity Bar Analysis
    summary += '--- ACTIVITY BAR ---\n';
    if (report.uiAnalysis.activityBar.error) {
        summary += `❌ ${report.uiAnalysis.activityBar.error}\n`;
    } else {
        const ab = report.uiAnalysis.activityBar;
        summary += `✓ Found at: (${ab.rect.x}, ${ab.rect.y})\n`;
        summary += `  Size: ${ab.rect.width}x${ab.rect.height}\n`;
        summary += `  Display: ${ab.computed.display}, Visibility: ${ab.computed.visibility}\n`;
        summary += `  Z-Index: ${ab.computed.zIndex}, Position: ${ab.computed.position}\n`;
        summary += `  Buttons found: ${ab.buttons ? ab.buttons.length : 0}\n`;
        if (ab.buttons) {
            ab.buttons.forEach((btn, i) => {
                if (btn.computed.display === 'none' || btn.computed.visibility === 'hidden') {
                    summary += `  ⚠️ Button ${i} is hidden!\n`;
                }
            });
        }
    }

    // Sidebar Analysis
    summary += '\n--- SIDEBAR ---\n';
    if (report.uiAnalysis.sidebar.error) {
        summary += `❌ ${report.uiAnalysis.sidebar.error}\n`;
    } else {
        const sb = report.uiAnalysis.sidebar;
        summary += `✓ Found at: (${sb.rect.x}, ${sb.rect.y})\n`;
        summary += `  Size: ${sb.rect.width}x${sb.rect.height}\n`;
        summary += `  Display: ${sb.computed.display}, Visibility: ${sb.computed.visibility}\n`;
        summary += `  Tabs found: ${sb.tabs ? sb.tabs.length : 0}\n`;
    }

    // Status Bar Analysis
    summary += '\n--- STATUS BAR ---\n';
    if (report.uiAnalysis.statusBar.error) {
        summary += `❌ ${report.uiAnalysis.statusBar.error}\n`;
    } else {
        const stb = report.uiAnalysis.statusBar;
        summary += `✓ Found at: (${stb.rect.x}, ${stb.rect.y})\n`;
        summary += `  Size: ${stb.rect.width}x${stb.rect.height}\n`;
        summary += `  Items found: ${stb.items ? stb.items.length : 0}\n`;
    }

    // Menu Bar Analysis
    summary += '\n--- MENU BAR ---\n';
    if (report.uiAnalysis.menuBar.error) {
        summary += `❌ ${report.uiAnalysis.menuBar.error}\n`;
    } else {
        const mb = report.uiAnalysis.menuBar;
        summary += `✓ Found at: (${mb.rect.x}, ${mb.rect.y})\n`;
        summary += `  Size: ${mb.rect.width}x${mb.rect.height}\n`;
        summary += `  Items found: ${mb.items ? mb.items.length : 0}\n`;
    }

    // Overlapping Elements
    summary += '\n--- OVERLAPPING ELEMENTS ---\n';
    if (report.uiAnalysis.overlaps.length === 0) {
        summary += '✓ No overlapping interactive elements detected\n';
    } else {
        summary += `⚠️ Found ${report.uiAnalysis.overlaps.length} overlapping elements:\n`;
        report.uiAnalysis.overlaps.slice(0, 10).forEach((overlap, i) => {
            summary += `  ${i + 1}. ${overlap.element1.className || overlap.element1.id} overlaps ${overlap.element2.className || overlap.element2.id}\n`;
            if (overlap.zIndexConflict) {
                summary += `     🔴 Z-index conflict detected!\n`;
            }
        });
        if (report.uiAnalysis.overlaps.length > 10) {
            summary += `  ... and ${report.uiAnalysis.overlaps.length - 10} more (see JSON report)\n`;
        }
    }

    // Hidden Elements
    summary += '\n--- HIDDEN/INVISIBLE ELEMENTS ---\n';
    if (report.uiAnalysis.hidden.length === 0) {
        summary += '✓ No unexpectedly hidden elements\n';
    } else {
        summary += `⚠️ Found ${report.uiAnalysis.hidden.length} hidden elements:\n`;
        report.uiAnalysis.hidden.slice(0, 15).forEach((hidden, i) => {
            summary += `  ${i + 1}. ${hidden.info.className || hidden.info.id} - Reason: ${hidden.reason}\n`;
        });
        if (report.uiAnalysis.hidden.length > 15) {
            summary += `  ... and ${report.uiAnalysis.hidden.length - 15} more (see JSON report)\n`;
        }
    }

    // Z-Index Conflicts
    summary += '\n--- Z-INDEX CONFLICTS ---\n';
    if (report.uiAnalysis.zIndexIssues.length === 0) {
        summary += '✓ No z-index conflicts\n';
    } else {
        summary += `⚠️ Found ${report.uiAnalysis.zIndexIssues.length} z-index conflicts:\n`;
        report.uiAnalysis.zIndexIssues.forEach((conflict, i) => {
            summary += `  ${i + 1}. ${conflict.elements.length} elements at z-index ${conflict.zIndex}\n`;
        });
    }

    // Layout Shifts
    summary += '\n--- LAYOUT SHIFTS ---\n';
    if (report.layoutShifts.length === 0) {
        summary += '✓ No significant layout shifts detected\n';
    } else {
        summary += `⚠️ Detected ${report.layoutShifts.length} layout shifts\n`;
    }

    // Console Issues
    summary += '\n--- CONSOLE ERRORS/WARNINGS ---\n';
    if (report.issues.length === 0) {
        summary += '✓ No console issues\n';
    } else {
        summary += `Found ${report.issues.length} console messages:\n`;
        report.issues.slice(0, 20).forEach((issue, i) => {
            summary += `  ${i + 1}. [${issue.type}] ${issue.message}\n`;
        });
        if (report.issues.length > 20) {
            summary += `  ... and ${report.issues.length - 20} more (see JSON report)\n`;
        }
    }

    summary += '\n=== END OF REPORT ===\n';
    return summary;
}

scanUIBlocking().catch(console.error);
