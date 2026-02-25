/**
 * AI Code Review with Security Analysis
 * Revolutionary: Comprehensive code review combining style, security, and best practices
 * Generates detailed reports with actionable fixes
 */

class AICodeReviewSecurity {
    constructor(editor) {
        this.editor = editor;
        this.reviewPanel = null;
        this.currentReview = null;
        this.severityStats = { critical: 0, high: 0, medium: 0, low: 0, info: 0 };
        
        console.log('[CodeReview] 🔍 AI Code Review & Security Analysis initialized');
    }
    
    // ========================================================================
    // REVIEW PANEL
    // ========================================================================
    
    createReviewPanel() {
        const panel = document.createElement('div');
        panel.id = 'ai-code-review-panel';
        panel.style.cssText = `
            position: fixed;
            right: 20px;
            top: 60px;
            width: 500px;
            height: calc(100vh - 80px);
            background: rgba(10, 10, 30, 0.98);
            backdrop-filter: blur(20px);
            border: 2px solid var(--orange);
            border-radius: 15px;
            z-index: 9999;
            display: flex;
            flex-direction: column;
            box-shadow: 0 10px 50px rgba(255, 165, 0, 0.4);
        `;
        
        panel.innerHTML = `
            <!-- Header -->
            <div style="padding: 20px; background: rgba(0, 0, 0, 0.5); border-bottom: 2px solid var(--orange);">
                <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px;">
                    <h2 style="margin: 0; color: var(--orange); font-size: 18px;">🔍 AI Code Review</h2>
                    <button onclick="window.aiCodeReview.closePanel()" style="background: none; border: none; color: #888; font-size: 24px; cursor: pointer;">×</button>
                </div>
                
                <div style="display: flex; gap: 10px;">
                    <button onclick="window.aiCodeReview.startFullReview()" style="flex: 1; background: var(--orange); color: #000; border: none; padding: 10px; border-radius: 6px; cursor: pointer; font-weight: bold; font-size: 12px;">🚀 Full Review</button>
                    <button onclick="window.aiCodeReview.startSecurityScan()" style="flex: 1; background: rgba(255, 71, 87, 0.2); border: 1px solid var(--red); color: var(--red); padding: 10px; border-radius: 6px; cursor: pointer; font-size: 12px;">🛡️ Security Scan</button>
                </div>
            </div>
            
            <!-- Stats Bar -->
            <div style="padding: 15px 20px; background: rgba(0, 0, 0, 0.3); border-bottom: 1px solid rgba(255, 165, 0, 0.3); display: flex; justify-content: space-around; font-size: 11px;">
                <div style="text-align: center;">
                    <div style="color: #ff4757; font-weight: bold; font-size: 16px;" id="stat-critical">0</div>
                    <div style="color: #888;">Critical</div>
                </div>
                <div style="text-align: center;">
                    <div style="color: var(--red); font-weight: bold; font-size: 16px;" id="stat-high">0</div>
                    <div style="color: #888;">High</div>
                </div>
                <div style="text-align: center;">
                    <div style="color: var(--orange); font-weight: bold; font-size: 16px;" id="stat-medium">0</div>
                    <div style="color: #888;">Medium</div>
                </div>
                <div style="text-align: center;">
                    <div style="color: #ffa726; font-weight: bold; font-size: 16px;" id="stat-low">0</div>
                    <div style="color: #888;">Low</div>
                </div>
                <div style="text-align: center;">
                    <div style="color: var(--cyan); font-weight: bold; font-size: 16px;" id="stat-info">0</div>
                    <div style="color: #888;">Info</div>
                </div>
            </div>
            
            <!-- Tabs -->
            <div style="display: flex; background: rgba(0, 0, 0, 0.3); border-bottom: 1px solid rgba(255, 165, 0, 0.3);">
                <button class="review-tab active" data-tab="issues" onclick="window.aiCodeReview.switchTab('issues')">⚠️ Issues</button>
                <button class="review-tab" data-tab="security" onclick="window.aiCodeReview.switchTab('security')">🛡️ Security</button>
                <button class="review-tab" data-tab="suggestions" onclick="window.aiCodeReview.switchTab('suggestions')">💡 Suggestions</button>
                <button class="review-tab" data-tab="report" onclick="window.aiCodeReview.switchTab('report')">📄 Report</button>
            </div>
            
            <!-- Content -->
            <div style="flex: 1; overflow-y: auto; padding: 20px;">
                <!-- Issues Tab -->
                <div id="review-tab-issues" class="review-tab-content">
                    <div id="review-issues-list" style="text-align: center; padding: 60px 20px; color: #888;">
                        <div style="font-size: 48px; margin-bottom: 15px;">📋</div>
                        <div style="font-size: 14px;">Click "Full Review" to start analysis</div>
                    </div>
                </div>
                
                <!-- Security Tab -->
                <div id="review-tab-security" class="review-tab-content" style="display: none;">
                    <div id="review-security-list"></div>
                </div>
                
                <!-- Suggestions Tab -->
                <div id="review-tab-suggestions" class="review-tab-content" style="display: none;">
                    <div id="review-suggestions-list"></div>
                </div>
                
                <!-- Report Tab -->
                <div id="review-tab-report" class="review-tab-content" style="display: none;">
                    <div id="review-report-content"></div>
                </div>
            </div>
            
            <!-- Footer -->
            <div style="padding: 15px 20px; background: rgba(0, 0, 0, 0.5); border-top: 1px solid rgba(255, 165, 0, 0.3); display: flex; gap: 10px;">
                <button onclick="window.aiCodeReview.exportReport()" style="flex: 1; background: rgba(0, 212, 255, 0.2); border: 1px solid var(--cyan); color: var(--cyan); padding: 10px; border-radius: 6px; cursor: pointer; font-size: 12px;">💾 Export</button>
                <button onclick="window.aiCodeReview.applyAllFixes()" style="flex: 1; background: rgba(0, 255, 136, 0.2); border: 1px solid var(--green); color: var(--green); padding: 10px; border-radius: 6px; cursor: pointer; font-size: 12px;">✨ Auto-Fix</button>
            </div>
        `;
        
        // Add CSS
        const style = document.createElement('style');
        style.textContent = `
            .review-tab {
                background: transparent;
                border: none;
                color: #888;
                padding: 12px 15px;
                cursor: pointer;
                font-size: 11px;
                transition: all 0.2s;
                border-bottom: 2px solid transparent;
                flex: 1;
            }
            
            .review-tab:hover {
                background: rgba(255, 165, 0, 0.1);
                color: #fff;
            }
            
            .review-tab.active {
                color: var(--orange);
                border-bottom: 2px solid var(--orange);
                font-weight: bold;
            }
        `;
        document.head.appendChild(style);
        
        document.body.appendChild(panel);
        this.reviewPanel = panel;
        
        console.log('[CodeReview] ✅ Review panel created');
    }
    
    // ========================================================================
    // REVIEW EXECUTION
    // ========================================================================
    
    async startFullReview() {
        const code = this.editor.getValue();
        if (!code || code.trim().length < 10) {
            alert('Please write some code first!');
            return;
        }
        
        console.log('[CodeReview] 🚀 Starting full code review...');
        
        // Reset stats
        this.severityStats = { critical: 0, high: 0, medium: 0, low: 0, info: 0 };
        this.updateStats();
        
        // Show loading
        document.getElementById('review-issues-list').innerHTML = `
            <div style="text-align: center; padding: 40px 20px;">
                <div style="font-size: 48px; margin-bottom: 15px;">🔄</div>
                <div style="color: var(--orange); font-size: 14px; font-weight: bold;">AI is reviewing your code...</div>
                <div style="color: #888; font-size: 11px; margin-top: 10px;">This may take a moment</div>
            </div>
        `;
        
        try {
            const response = await fetch('http://localhost:11441/api/chat', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    message: `Perform a comprehensive code review on this code. Analyze:

**1. Code Quality:**
- Readability and maintainability
- Naming conventions
- Code organization
- DRY principle violations

**2. Security:**
- SQL injection risks
- XSS vulnerabilities
- CSRF issues
- Insecure crypto usage
- Hardcoded secrets
- Authentication/authorization flaws

**3. Performance:**
- Inefficient algorithms
- Memory leaks
- Unnecessary computations
- N+1 query problems

**4. Best Practices:**
- Error handling
- Input validation
- Logging practices
- Documentation

Respond ONLY in JSON:
{
  "issues": [
    {
      "line": 10,
      "severity": "critical|high|medium|low|info",
      "category": "security|performance|quality|best-practice",
      "title": "Brief title",
      "description": "Detailed explanation",
      "fix": "How to fix",
      "code_example": "Fixed code snippet"
    }
  ],
  "summary": "Overall assessment",
  "score": 85
}

Code:
\`\`\`
${code}
\`\`\``,
                    model: 'BigDaddyG:Security'
                })
            });
            
            const data = await response.json();
            const review = this.parseReviewResponse(data.response);
            
            this.currentReview = review;
            this.displayReview(review);
            
            console.log('[CodeReview] ✅ Review complete');
            
        } catch (error) {
            console.error('[CodeReview] ❌ Review error:', error);
            document.getElementById('review-issues-list').innerHTML = `
                <div style="text-align: center; padding: 40px 20px; color: var(--red);">
                    <div style="font-size: 48px; margin-bottom: 15px;">❌</div>
                    <div style="font-size: 14px;">Review failed: ${error.message}</div>
                </div>
            `;
        }
    }
    
    async startSecurityScan() {
        const code = this.editor.getValue();
        
        console.log('[CodeReview] 🛡️ Starting security scan...');
        
        // Similar to full review but focused on security
        // Implementation similar to above but with security-focused prompt
        this.startFullReview(); // For now, reuse full review
    }
    
    parseReviewResponse(response) {
        try {
            const jsonMatch = response.match(/\{[\s\S]*"issues"[\s\S]*\}/);
            if (jsonMatch) {
                return JSON.parse(jsonMatch[0]);
            }
        } catch (e) {
            console.warn('[CodeReview] Could not parse JSON, using fallback');
        }
        
        // Fallback
        return {
            issues: [],
            summary: 'Review completed but results could not be parsed.',
            score: 50
        };
    }
    
    // ========================================================================
    // DISPLAY FUNCTIONS
    // ========================================================================
    
    displayReview(review) {
        // Update stats
        review.issues.forEach(issue => {
            this.severityStats[issue.severity]++;
        });
        this.updateStats();
        
        // Display issues
        const issuesList = document.getElementById('review-issues-list');
        const securityList = document.getElementById('review-security-list');
        const suggestionsList = document.getElementById('review-suggestions-list');
        const reportContent = document.getElementById('review-report-content');
        
        // Sort by severity
        const severityOrder = { critical: 0, high: 1, medium: 2, low: 3, info: 4 };
        const sorted = review.issues.sort((a, b) => severityOrder[a.severity] - severityOrder[b.severity]);
        
        // Issues tab
        if (sorted.length === 0) {
            issuesList.innerHTML = `
                <div style="text-align: center; padding: 60px 20px; color: var(--green);">
                    <div style="font-size: 64px; margin-bottom: 15px;">✅</div>
                    <div style="font-size: 16px; font-weight: bold;">No Issues Found!</div>
                    <div style="font-size: 12px; color: #888; margin-top: 10px;">Your code looks great!</div>
                </div>
            `;
        } else {
            issuesList.innerHTML = sorted.map(issue => this.renderIssue(issue)).join('');
        }
        
        // Security tab
        const securityIssues = sorted.filter(i => i.category === 'security');
        securityList.innerHTML = securityIssues.length > 0 
            ? securityIssues.map(issue => this.renderIssue(issue)).join('')
            : '<div style="text-align: center; padding: 40px; color: var(--green);">✅ No security issues detected</div>';
        
        // Suggestions tab
        const improvements = sorted.filter(i => i.severity === 'info' || i.severity === 'low');
        suggestionsList.innerHTML = improvements.length > 0
            ? improvements.map(issue => this.renderIssue(issue)).join('')
            : '<div style="text-align: center; padding: 40px; color: #888;">No suggestions at this time</div>';
        
        // Report tab
        reportContent.innerHTML = this.generateReport(review);
    }
    
    renderIssue(issue) {
        const severityColors = {
            critical: '#ff4757',
            high: 'var(--red)',
            medium: 'var(--orange)',
            low: '#ffa726',
            info: 'var(--cyan)'
        };
        
        const categoryIcons = {
            security: '🛡️',
            performance: '⚡',
            quality: '✨',
            'best-practice': '📚'
        };
        
        const color = severityColors[issue.severity];
        const icon = categoryIcons[issue.category] || '⚠️';
        
        return `
            <div onclick="window.aiCodeReview.jumpToLine(${issue.line})" style="margin-bottom: 15px; padding: 15px; background: rgba(0, 0, 0, 0.3); border-left: 4px solid ${color}; border-radius: 8px; cursor: pointer; transition: all 0.2s;" onmouseover="this.style.background='rgba(0,0,0,0.5)'" onmouseout="this.style.background='rgba(0,0,0,0.3)'">
                <div style="display: flex; justify-content: space-between; align-items: start; margin-bottom: 10px;">
                    <div style="display: flex; align-items: center; gap: 8px;">
                        <span style="font-size: 18px;">${icon}</span>
                        <div>
                            <div style="color: ${color}; font-weight: bold; font-size: 13px; text-transform: uppercase;">${issue.severity}</div>
                            <div style="color: #888; font-size: 10px;">${issue.category}</div>
                        </div>
                    </div>
                    <span style="color: #888; font-size: 11px;">Line ${issue.line}</span>
                </div>
                
                <div style="color: #fff; font-size: 13px; font-weight: bold; margin-bottom: 8px;">${issue.title}</div>
                <div style="color: #ccc; font-size: 12px; line-height: 1.5; margin-bottom: 12px;">${issue.description}</div>
                
                <div style="background: rgba(0, 255, 136, 0.05); padding: 10px; border-left: 2px solid var(--green); border-radius: 4px; margin-bottom: 10px;">
                    <div style="color: var(--green); font-size: 11px; font-weight: bold; margin-bottom: 5px;">💡 Fix:</div>
                    <div style="color: #aaa; font-size: 11px; line-height: 1.4;">${issue.fix}</div>
                </div>
                
                ${issue.code_example ? `
                    <details style="margin-top: 10px;">
                        <summary style="color: var(--cyan); font-size: 11px; cursor: pointer; user-select: none;">📝 View Fixed Code</summary>
                        <pre style="background: rgba(0, 0, 0, 0.5); padding: 10px; border-radius: 4px; overflow-x: auto; margin-top: 8px; font-size: 11px; line-height: 1.4;"><code>${this.escapeHtml(issue.code_example)}</code></pre>
                    </details>
                ` : ''}
            </div>
        `;
    }
    
    generateReport(review) {
        const totalIssues = review.issues.length;
        const score = review.score || 0;
        const scoreColor = score >= 80 ? 'var(--green)' : score >= 60 ? 'var(--orange)' : 'var(--red)';
        
        return `
            <div style="margin-bottom: 20px;">
                <h3 style="color: var(--orange); font-size: 16px; margin-bottom: 15px;">📊 Code Quality Report</h3>
                
                <div style="background: rgba(0, 0, 0, 0.3); padding: 20px; border-radius: 8px; text-align: center; margin-bottom: 20px;">
                    <div style="font-size: 48px; font-weight: bold; color: ${scoreColor}; margin-bottom: 10px;">${score}/100</div>
                    <div style="color: #888; font-size: 13px;">Overall Code Quality Score</div>
                </div>
                
                <div style="background: rgba(0, 0, 0, 0.3); padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                    <h4 style="color: var(--cyan); font-size: 13px; margin-bottom: 12px;">Summary</h4>
                    <div style="color: #ccc; font-size: 12px; line-height: 1.6;">${review.summary}</div>
                </div>
                
                <div style="background: rgba(0, 0, 0, 0.3); padding: 15px; border-radius: 8px;">
                    <h4 style="color: var(--cyan); font-size: 13px; margin-bottom: 12px;">Issues Breakdown</h4>
                    <div style="font-size: 12px; color: #ccc;">
                        <div style="display: flex; justify-content: space-between; padding: 8px 0; border-bottom: 1px solid rgba(255, 255, 255, 0.1);">
                            <span>Total Issues:</span>
                            <strong style="color: var(--orange);">${totalIssues}</strong>
                        </div>
                        <div style="display: flex; justify-content: space-between; padding: 8px 0; border-bottom: 1px solid rgba(255, 255, 255, 0.1);">
                            <span>Critical:</span>
                            <strong style="color: #ff4757;">${this.severityStats.critical}</strong>
                        </div>
                        <div style="display: flex; justify-content: space-between; padding: 8px 0; border-bottom: 1px solid rgba(255, 255, 255, 0.1);">
                            <span>High:</span>
                            <strong style="color: var(--red);">${this.severityStats.high}</strong>
                        </div>
                        <div style="display: flex; justify-content: space-between; padding: 8px 0; border-bottom: 1px solid rgba(255, 255, 255, 0.1);">
                            <span>Medium:</span>
                            <strong style="color: var(--orange);">${this.severityStats.medium}</strong>
                        </div>
                        <div style="display: flex; justify-content: space-between; padding: 8px 0; border-bottom: 1px solid rgba(255, 255, 255, 0.1);">
                            <span>Low:</span>
                            <strong style="color: #ffa726;">${this.severityStats.low}</strong>
                        </div>
                        <div style="display: flex; justify-content: space-between; padding: 8px 0;">
                            <span>Info:</span>
                            <strong style="color: var(--cyan);">${this.severityStats.info}</strong>
                        </div>
                    </div>
                </div>
            </div>
        `;
    }
    
    updateStats() {
        document.getElementById('stat-critical').textContent = this.severityStats.critical;
        document.getElementById('stat-high').textContent = this.severityStats.high;
        document.getElementById('stat-medium').textContent = this.severityStats.medium;
        document.getElementById('stat-low').textContent = this.severityStats.low;
        document.getElementById('stat-info').textContent = this.severityStats.info;
    }
    
    // ========================================================================
    // ACTIONS
    // ========================================================================
    
    switchTab(tabName) {
        // Update tabs
        document.querySelectorAll('.review-tab').forEach(tab => {
            tab.classList.remove('active');
        });
        document.querySelector(`.review-tab[data-tab="${tabName}"]`).classList.add('active');
        
        // Update content
        document.querySelectorAll('.review-tab-content').forEach(content => {
            content.style.display = 'none';
        });
        document.getElementById(`review-tab-${tabName}`).style.display = 'block';
    }
    
    jumpToLine(lineNumber) {
        this.editor.revealLineInCenter(lineNumber);
        this.editor.setPosition({ lineNumber, column: 1 });
        this.editor.focus();
    }
    
    exportReport() {
        if (!this.currentReview) return;
        
        const markdown = this.generateMarkdownReport(this.currentReview);
        const blob = new Blob([markdown], { type: 'text/markdown' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = `code-review-${Date.now()}.md`;
        a.click();
        
        console.log('[CodeReview] 💾 Report exported');
    }
    
    generateMarkdownReport(review) {
        let md = `# Code Review Report\n\n`;
        md += `**Generated:** ${new Date().toLocaleString()}\n`;
        md += `**Score:** ${review.score}/100\n\n`;
        md += `## Summary\n\n${review.summary}\n\n`;
        md += `## Issues\n\n`;
        
        review.issues.forEach(issue => {
            md += `### ${issue.title} (Line ${issue.line})\n`;
            md += `**Severity:** ${issue.severity.toUpperCase()}\n`;
            md += `**Category:** ${issue.category}\n\n`;
            md += `${issue.description}\n\n`;
            md += `**Fix:** ${issue.fix}\n\n`;
            if (issue.code_example) {
                md += `\`\`\`\n${issue.code_example}\n\`\`\`\n\n`;
            }
            md += `---\n\n`;
        });
        
        return md;
    }
    
    applyAllFixes() {
        alert('Auto-fix feature coming soon! This will automatically apply all suggested fixes to your code.');
        console.log('[CodeReview] 🔧 Auto-fix requested (feature in development)');
    }
    
    closePanel() {
        if (this.reviewPanel) {
            this.reviewPanel.remove();
            this.reviewPanel = null;
        }
    }
    
    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }
}

// ============================================================================
// INITIALIZATION
// ============================================================================

window.AICodeReviewSecurity = AICodeReviewSecurity;

setTimeout(() => {
    if (window.editor) {
        window.aiCodeReview = new AICodeReviewSecurity(window.editor);
        
        // Keyboard shortcut: Ctrl+Shift+R
        document.addEventListener('keydown', (e) => {
            if (e.ctrlKey && e.shiftKey && e.key === 'R') {
                e.preventDefault();
                if (!window.aiCodeReview.reviewPanel) {
                    window.aiCodeReview.createReviewPanel();
                }
            }
        });
        
        console.log('[CodeReview] 🎯 Ready! Press Ctrl+Shift+R for code review');
    }
}, 2000);

console.log('[CodeReview] 📦 AI Code Review & Security module loaded');

