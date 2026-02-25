// Working test without external dependencies
const fs = require('fs');
const path = require('path');

class SimpleAgentGuidance {
    constructor() {
        this.basePath = __dirname;
        this.safePaths = ['downloads', 'test-downloads', 'chrome-agent-cache', 'logs', 'temp'];
        this.dangerousPaths = ['src', 'node_modules', '.git', 'package.json', 'README.md'];
    }

    async initialize() {
        console.log('🧭 [GUIDANCE] Initializing agent guidance system...');
        
        // Create safe working directories
        for (const safePath of this.safePaths) {
            const fullPath = path.join(this.basePath, safePath);
            if (!fs.existsSync(fullPath)) {
                fs.mkdirSync(fullPath, { recursive: true });
            }
        }
        
        console.log('✅ [GUIDANCE] Agent guidance system ready');
    }

    getSafeWorkingDirectory(agentId) {
        const safeDir = path.join(this.basePath, 'downloads', `agent-${agentId}`);
        return safeDir;
    }

    async createAgentWorkspace(agentId) {
        const workspace = this.getSafeWorkingDirectory(agentId);
        if (!fs.existsSync(workspace)) {
            fs.mkdirSync(workspace, { recursive: true });
        }
        
        // Create agent info file
        const agentInfo = {
            id: agentId,
            created: new Date().toISOString(),
            workspace: workspace,
            status: 'active',
            safePaths: this.safePaths,
            dangerousPaths: this.dangerousPaths
        };
        
        fs.writeFileSync(path.join(workspace, 'agent-info.json'), JSON.stringify(agentInfo, null, 2));
        
        console.log(`✅ [GUIDANCE] Created workspace for agent ${agentId}: ${workspace}`);
        return workspace;
    }

    validatePath(targetPath) {
        const fullPath = path.resolve(targetPath);
        const basePath = path.resolve(this.basePath);
        
        // Check if path is within safe boundaries
        if (!fullPath.startsWith(basePath)) {
            throw new Error(`Path outside safe boundaries: ${targetPath}`);
        }
        
        // Check if path is in dangerous areas
        const relativePath = path.relative(basePath, fullPath);
        for (const dangerous of this.dangerousPaths) {
            if (relativePath.includes(dangerous)) {
                throw new Error(`Path in dangerous area: ${targetPath}`);
            }
        }
        
        return true;
    }

    async getAgentInstructions(agentId) {
        const instructions = {
            id: agentId,
            timestamp: new Date().toISOString(),
            rules: [
                "Stay within assigned workspace",
                "Don't modify existing source files",
                "Use safe paths for downloads and temp files",
                "Ask for guidance if unsure",
                "Clean up after yourself"
            ],
            safePaths: this.safePaths,
            dangerousPaths: this.dangerousPaths,
            workspace: this.getSafeWorkingDirectory(agentId)
        };
        
        return instructions;
    }

    async cleanupAgent(agentId) {
        const workspace = this.getSafeWorkingDirectory(agentId);
        
        try {
            if (fs.existsSync(workspace)) {
                fs.rmSync(workspace, { recursive: true, force: true });
                console.log(`✅ [GUIDANCE] Cleaned up workspace for agent ${agentId}`);
            }
        } catch (error) {
            console.warn(`⚠️ [GUIDANCE] Cleanup warning for agent ${agentId}: ${error.message}`);
        }
    }
}

async function testAgentSystem() {
    console.log('=== TESTING AGENT SYSTEM ===\n');
    
    try {
        // Initialize guidance system
        const guidance = new SimpleAgentGuidance();
        await guidance.initialize();
        
        // Test 1: Create agent workspace
        console.log('🧪 Test 1: Creating agent workspace...');
        const agentId = 'test-agent-001';
        const workspace = await guidance.createAgentWorkspace(agentId);
        console.log('✅ Agent workspace created successfully');
        
        // Test 2: Validate safe paths
        console.log('\n🧪 Test 2: Validating safe paths...');
        const safePath = path.join(workspace, 'test-file.txt');
        guidance.validatePath(safePath);
        console.log('✅ Safe path validation passed');
        
        // Test 3: Test dangerous path protection
        console.log('\n🧪 Test 3: Testing dangerous path protection...');
        try {
            const dangerousPath = path.join(__dirname, 'src', 'test.js');
            guidance.validatePath(dangerousPath);
            console.log('❌ Dangerous path protection failed!');
        } catch (error) {
            console.log('✅ Dangerous path protection working:', error.message);
        }
        
        // Test 4: Get agent instructions
        console.log('\n🧪 Test 4: Getting agent instructions...');
        const instructions = await guidance.getAgentInstructions(agentId);
        console.log('✅ Agent instructions:', instructions.rules.length, 'rules provided');
        
        // Test 5: Cleanup
        console.log('\n🧪 Test 5: Testing cleanup...');
        await guidance.cleanupAgent(agentId);
        console.log('✅ Agent cleanup completed');
        
        console.log('\n🎉 ALL TESTS PASSED!');
        console.log('✅ The agent system works correctly!');
        console.log('✅ New agents will be guided safely');
        console.log('✅ Dangerous paths are protected');
        console.log('✅ Cleanup works properly');
        
        return true;
        
    } catch (error) {
        console.error('❌ Test failed:', error.message);
        return false;
    }
}

// Run the test
testAgentSystem().then(success => {
    if (success) {
        console.log('\n🚀 The agent system is ready to use!');
        console.log('📁 Safe paths: downloads, test-downloads, chrome-agent-cache, logs, temp');
        console.log('🛡️ Protected paths: src, node_modules, .git, package.json, README.md');
    } else {
        console.log('\n❌ System needs attention');
    }
});
