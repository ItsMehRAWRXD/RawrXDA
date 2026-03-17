class FeatureShowcase {
    constructor(ide) {
        this.ide = ide;
        this.features = {
            'AI & Automation': [
                { name: 'Multi-AI Integration', icon: '🤖', action: () => this.ide.multiAI.renderProviderSelector() },
                { name: 'Context-Aware Copilot', icon: '🧠', action: () => this.ide.contextAwareCopilot?.show() },
                { name: 'Agent Mode', icon: '🎯', action: () => this.ide.toggleAgenticMode(true) },
                { name: 'Voice Coding', icon: '🎤', action: () => this.ide.toggleVoiceCoding() },
                { name: 'AI Code Generator', icon: '✨', action: () => this.ide.aiGenerateFunction() },
                { name: 'AI Code Reviewer', icon: '👁️', action: () => this.ide.aiReviewCode() },
                { name: 'AI Refactorer', icon: '🔄', action: () => this.ide.aiRefactorCode() },
                { name: 'Natural Language Programming', icon: '💬', action: () => this.ide.naturalLanguageProgramming?.show() }
            ],
            'Development Tools': [
                { name: 'Multi-Language Compiler', icon: '⚙️', action: () => this.ide.scanForCompilers() },
                { name: 'Debugger', icon: '🐛', action: () => this.ide.startDebug() },
                { name: 'Test Runner', icon: '🧪', action: () => this.ide.runTests() },
                { name: 'Code Profiler', icon: '📊', action: () => this.ide.profileCode() },
                { name: 'Assembler', icon: '🔧', action: () => this.ide.assembleFile() },
                { name: 'Transpiler', icon: '🔀', action: () => this.ide.showTranspiler() },
                { name: 'Roslyn Compiler', icon: '🏗️', action: () => this.ide.compileWithRoslyn() },
                { name: 'Bootstrap Compiler', icon: '🚀', action: () => this.ide.bootstrapCompiler() }
            ],
            'Browser & Computer Use': [
                { name: 'Computer Use Browser', icon: '🌐', action: () => this.ide.computerBrowser.show() },
                { name: 'Browser Engine', icon: '🔍', action: () => this.ide.browser?.createTab() },
                { name: 'Screenshot Tool', icon: '📸', action: () => this.ide.computerBrowser.takeScreenshot() },
                { name: 'Web Automation', icon: '🤖', action: () => this.ide.computerBrowser.navigate('https://google.com') }
            ],
            'Collaboration': [
                { name: 'Live Share', icon: '👥', action: () => this.ide.createLiveSession() },
                { name: 'Code Review', icon: '📝', action: () => this.ide.addReviewComment() },
                { name: 'Study Buddy Matching', icon: '🤝', action: () => this.ide.studyBuddyMatching.showStudyBuddyMatching() },
                { name: 'Code Review Exchange', icon: '🔄', action: () => this.ide.codeReviewExchange.showCodeReviewExchange() },
                { name: 'Mentor Marketplace', icon: '👨‍🏫', action: () => this.ide.mentorMarketplace.showMentorMarketplace() }
            ],
            'Cloud & DevOps': [
                { name: 'Docker Manager', icon: '🐳', action: () => this.ide.buildDockerImage() },
                { name: 'AWS Integration', icon: '☁️', action: () => this.ide.awsIntegration?.show() },
                { name: 'Cloud Sync', icon: '☁️', action: () => this.ide.syncWorkspace() },
                { name: 'Remote Dev (SSH)', icon: '🔐', action: () => this.ide.connectSSH() },
                { name: 'WSL Integration', icon: '🐧', action: () => this.ide.connectWSL() },
                { name: 'Container Dev', icon: '📦', action: () => this.ide.containerDev.showContainerPanel() }
            ],
            'Database & APIs': [
                { name: 'Database Manager', icon: '🗄️', action: () => this.ide.connectDatabase() },
                { name: 'SQL Query Editor', icon: '📊', action: () => this.ide.showQueryEditor() },
                { name: 'REST Client', icon: '🌐', action: () => this.ide.restClient.show() },
                { name: 'API Designer', icon: '🎨', action: () => this.ide.apiDesigner.showDesigner() },
                { name: 'API Doc Generator', icon: '📚', action: () => this.ide.generateAPIDocs() }
            ],
            'Learning & Education': [
                { name: 'Interactive Tutorials', icon: '📖', action: () => this.ide.tutorialSystem.showTutorialList() },
                { name: 'Code Reading Practice', icon: '👓', action: () => this.ide.codeReadingPractice.showCodeReadingPractice() },
                { name: 'Concept Mapping', icon: '🗺️', action: () => this.ide.conceptMapping.showConceptMapping() },
                { name: 'Progress Tracker', icon: '📈', action: () => this.ide.progressTracker.showProgress() },
                { name: 'Coding Challenges', icon: '🏆', action: () => this.ide.codingChallenges?.showChallenges() },
                { name: 'Learning Mode', icon: '🎓', action: () => this.ide.learningMode.toggle() },
                { name: 'Stuck Helper', icon: '🆘', action: () => this.ide.stuckHelper?.show() }
            ],
            'Advanced Features': [
                { name: '3D Code Visualizer', icon: '🎮', action: () => this.ide.show3DView() },
                { name: 'Blockchain Tools', icon: '⛓️', action: () => this.ide.compileContract() },
                { name: 'IoT Manager', icon: '📡', action: () => this.ide.scanIoTDevices() },
                { name: 'Semantic Search', icon: '🔍', action: () => this.ide.semanticSearch() },
                { name: 'Code Intelligence', icon: '🧠', action: () => this.ide.codeIntelligence?.analyze() },
                { name: 'Performance Predictor', icon: '⚡', action: () => this.ide.predictPerformance() },
                { name: 'Architecture Suggestions', icon: '🏛️', action: () => this.ide.suggestArchitecture() }
            ],
            'Enterprise': [
                { name: 'Security Scanner', icon: '🔒', action: () => this.ide.runSecurityScan() },
                { name: 'Compliance Checker', icon: '✅', action: () => this.ide.checkCompliance() },
                { name: 'Audit Logs', icon: '📋', action: () => this.ide.showAuditLogs() },
                { name: 'License Manager', icon: '📜', action: () => this.ide.checkLicenses() },
                { name: 'Enterprise Dashboard', icon: '📊', action: () => this.ide.showEnterpriseDashboard() }
            ],
            'Productivity': [
                { name: 'Snippet Manager', icon: '📝', action: () => this.ide.snippetManager.showSnippetPanel() },
                { name: 'Task Automation', icon: '⚡', action: () => this.ide.taskAutomation.showTaskPanel() },
                { name: 'Quick Open', icon: '⚡', action: () => this.ide.quickOpen.show() },
                { name: 'Command Palette', icon: '🎨', action: () => this.ide.commandPalette.show() },
                { name: 'Split Editor', icon: '⊞', action: () => this.ide.splitEditor.toggle() },
                { name: 'Markdown Preview', icon: '📄', action: () => this.ide.markdownPreview.toggle() }
            ],
            'Unique Features': [
                { name: 'Layout Builder', icon: '🎨', action: () => layoutBuilder.show() },
                { name: 'Media Integration', icon: '🎵', action: () => this.ide.mediaIntegration.show() },
                { name: 'Visual Package Browser', icon: '📦', action: () => this.ide.pkgBrowser.show() },
                { name: 'Visual Build Config', icon: '⚙️', action: () => this.ide.buildConfig.show() },
                { name: 'Project Wizard', icon: '🧙', action: () => this.ide.wizard.show() },
                { name: 'Panic Button', icon: '🚨', action: () => this.ide.panicButton?.activate() },
                { name: 'Success Celebration', icon: '🎉', action: () => this.ide.successCelebration.celebrate('feature') }
            ]
        };
    }

    show() {
        const panel = document.createElement('div');
        panel.style.cssText = 'position:fixed;top:0;left:0;right:0;bottom:0;background:rgba(0,0,0,0.95);z-index:10000;overflow-y:auto;';
        
        let html = `
            <div style="background:#2d2d30;padding:20px;border-bottom:3px solid #ff9900;position:sticky;top:0;z-index:1;">
                <div style="display:flex;justify-content:space-between;align-items:center;">
                    <div>
                        <h1 style="color:#ff9900;margin:0;font-size:28px;">🚀 MyCoPilot++ Features</h1>
                        <p style="color:#ccc;margin:5px 0 0 0;">Explore all ${this.getTotalFeatures()} advanced features</p>
                    </div>
                    <button onclick="this.parentElement.parentElement.parentElement.remove()" style="background:#f44;border:none;color:#fff;padding:10px 20px;cursor:pointer;border-radius:4px;font-weight:bold;font-size:16px;">Close</button>
                </div>
            </div>
            <div style="padding:20px;display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:20px;">
        `;
        
        Object.entries(this.features).forEach(([category, features]) => {
            html += `
                <div style="background:#2d2d30;border:1px solid #3e3e42;border-radius:8px;padding:15px;">
                    <h2 style="color:#ff9900;margin:0 0 15px 0;font-size:18px;border-bottom:2px solid #ff9900;padding-bottom:8px;">${category}</h2>
                    <div style="display:flex;flex-direction:column;gap:8px;">
            `;
            
            features.forEach((feature, i) => {
                html += `
                    <button onclick="featureShowcase.runFeature('${category}', ${i})" 
                            style="background:#1e1e1e;border:1px solid #3e3e42;color:#fff;padding:10px;cursor:pointer;border-radius:4px;text-align:left;display:flex;align-items:center;gap:10px;transition:all 0.2s;"
                            onmouseover="this.style.background='#404040';this.style.borderColor='#ff9900'"
                            onmouseout="this.style.background='#1e1e1e';this.style.borderColor='#3e3e42'">
                        <span style="font-size:20px;">${feature.icon}</span>
                        <span>${feature.name}</span>
                    </button>
                `;
            });
            
            html += `
                    </div>
                </div>
            `;
        });
        
        html += '</div>';
        panel.innerHTML = html;
        document.body.appendChild(panel);
    }

    getTotalFeatures() {
        return Object.values(this.features).reduce((sum, cat) => sum + cat.length, 0);
    }

    runFeature(category, index) {
        const feature = this.features[category][index];
        if (this.ide && typeof this.ide.addAIMessage === "function") {
            this.ide.addAIMessage('system', `Launching: ${feature.name}`);
        } else {
            console.log(`Launching: ${feature.name}`);
        }
        
        try {
            feature.action();
        } catch (error) {
            console.error(error);
            if (this.ide && typeof this.ide.addAIMessage === "function") {
                this.ide.addAIMessage('system', `Feature "${feature.name}" is available but requires additional setup`);
            } else {
                console.log(`Feature "${feature.name}" is available but requires additional setup`);
            }
        }
    }

    showQuickAccess() {
        const favorites = JSON.parse(localStorage.getItem('favoriteFeatures') || '[]');
        
        const panel = document.createElement('div');
        panel.style.cssText = 'position:fixed;top:50%;left:50%;transform:translate(-50%,-50%);background:#2d2d30;border:2px solid #ff9900;padding:20px;z-index:10000;border-radius:8px;min-width:400px;max-height:80vh;overflow-y:auto;';
        panel.innerHTML = `
            <div style="display:flex;justify-content:space-between;margin-bottom:15px;">
                <span style="color:#ff9900;font-weight:bold;font-size:18px;">⚡ Quick Access</span>
                <button onclick="this.parentElement.parentElement.remove()" style="background:#444;border:none;color:#fff;padding:4px 8px;cursor:pointer;border-radius:4px;">X</button>
            </div>
            <div style="margin-bottom:15px;">
                <button onclick="featureShowcase.show()" style="width:100%;padding:10px;background:#007ACC;border:none;color:#fff;cursor:pointer;border-radius:4px;margin-bottom:8px;">🚀 All Features</button>
                <button onclick="layoutBuilder.show()" style="width:100%;padding:10px;background:#ff9900;border:none;color:#fff;cursor:pointer;border-radius:4px;">🎨 Layout Builder</button>
            </div>
            <div style="color:#ccc;font-size:12px;margin-top:15px;padding-top:15px;border-top:1px solid #444;">
                <strong>Tip:</strong> Press Ctrl+Shift+F to open feature search
            </div>
        `;
        document.body.appendChild(panel);
    }
}

let featureShowcase, layoutBuilder;
document.addEventListener('DOMContentLoaded', () => {
    setTimeout(() => {
        if (window.ide) {
            featureShowcase = new FeatureShowcase(window.ide);
            layoutBuilder = new LayoutBuilder(window.ide);
            window.featureShowcase = featureShowcase;
            window.layoutBuilder = layoutBuilder;
            
            document.addEventListener('keydown', (e) => {
                if (e.ctrlKey && e.shiftKey && e.key === 'F') {
                    e.preventDefault();
                    featureShowcase.show();
                }
                if (e.ctrlKey && e.shiftKey && e.key === 'L') {
                    e.preventDefault();
                    layoutBuilder.show();
                }
            });
        }
    }, 500);
});
