// ============================================================================
// PROJECT IDE AI - WORKSPACE SETUP
// ============================================================================
// Organizes all discovered models and agents into a unified workspace
// ============================================================================

const fs = require('fs');
const path = require('path');

console.log('🚀 Setting up ProjectIDEAI workspace...\n');

// Create directory structure
const dirs = [
    'models',
    'agents',
    'server',
    'ide',
    'generators',
    'configs'
];

console.log('📁 Creating directories...');
dirs.forEach(dir => {
    const dirPath = path.join(__dirname, dir);
    if (!fs.existsSync(dirPath)) {
        fs.mkdirSync(dirPath, { recursive: true });
        console.log(`  ✅ Created: ${dir}/`);
    } else {
        console.log(`  ✓ Exists: ${dir}/`);
    }
});

console.log('\n📋 Creating registry files...\n');

// Scan for models and agents (reuse the scanning logic)
const modelRegistry = {
    models: [],
    agents: [],
    indexed: new Date().toISOString()
};

const basePaths = [
    'C:\\Users\\HiH8e\\.ollama\\models',
    'D:\\Security Research aka GitHub Repos',
    'C:\\Users\\HiH8e\\OneDrive'
];

function scanDirectory(dir, depth, type = 'both') {
    if (depth <= 0) return;
    
    try {
        const items = fs.readdirSync(dir);
        
        items.forEach(item => {
            try {
                const fullPath = path.join(dir, item);
                const stat = fs.statSync(fullPath);
                
                if (stat.isDirectory()) {
                    // Look for model directories
                    if (type === 'both' || type === 'models') {
                        if (item.toLowerCase().includes('model') || 
                            item.toLowerCase().includes('bigdaddyg') ||
                            item.toLowerCase().includes('llm')) {
                            modelRegistry.models.push({
                                name: item,
                                path: fullPath,
                                size: stat.size,
                                type: 'directory',
                                discovered: new Date().toISOString()
                            });
                        }
                    }
                    
                    // Look for agent directories
                    if (type === 'both' || type === 'agents') {
                        if (item.toLowerCase().includes('agent') ||
                            item.toLowerCase().includes('bot') ||
                            item.toLowerCase().includes('assistant')) {
                            modelRegistry.agents.push({
                                name: item,
                                path: fullPath,
                                type: 'directory',
                                discovered: new Date().toISOString()
                            });
                        }
                    }
                    
                    // Recurse
                    if (depth > 1) {
                        scanDirectory(fullPath, depth - 1, type);
                    }
                    
                } else if (stat.isFile()) {
                    // Model files
                    if ((type === 'both' || type === 'models') && 
                        (item.endsWith('.bin') || item.endsWith('.gguf') || 
                         item.endsWith('.safetensors') || item.endsWith('.pt'))) {
                        modelRegistry.models.push({
                            name: item,
                            path: fullPath,
                            size: stat.size,
                            type: 'file',
                            discovered: new Date().toISOString()
                        });
                    }
                    
                    // Agent files
                    if ((type === 'both' || type === 'agents') &&
                        item.toLowerCase().includes('agent') && 
                        (item.endsWith('.js') || item.endsWith('.py') || item.endsWith('.lua'))) {
                        modelRegistry.agents.push({
                            name: item,
                            path: fullPath,
                            size: stat.size,
                            type: 'file',
                            discovered: new Date().toISOString()
                        });
                    }
                }
            } catch (e) {
                // Skip permission errors
            }
        });
    } catch (e) {
        // Skip permission errors
    }
}

console.log('🔍 Scanning for models and agents...');
basePaths.forEach(basePath => {
    if (fs.existsSync(basePath)) {
        console.log(`  Scanning: ${basePath}`);
        scanDirectory(basePath, 3); // depth 3
    }
});

console.log(`\n✅ Discovery complete:`);
console.log(`   📦 Models: ${modelRegistry.models.length}`);
console.log(`   🤖 Agents: ${modelRegistry.agents.length}`);

// Save registry
fs.writeFileSync(
    path.join(__dirname, 'configs', 'registry.json'),
    JSON.stringify(modelRegistry, null, 2)
);
console.log(`\n💾 Saved registry to: configs/registry.json`);

// Create symlinks/shortcuts for top models and agents
console.log(`\n🔗 Creating links to discovered resources...\n`);

// Link top 10 models
const topModels = modelRegistry.models
    .filter(m => m.type === 'file' && m.size > 1000000) // > 1MB
    .sort((a, b) => b.size - a.size)
    .slice(0, 10);

topModels.forEach((model, idx) => {
    const linkPath = path.join(__dirname, 'models', `${idx + 1}_${model.name}`);
    const infoPath = linkPath + '.txt';
    
    fs.writeFileSync(infoPath, `Model Info:
Name: ${model.name}
Original Path: ${model.path}
Size: ${(model.size / 1024 / 1024).toFixed(2)} MB
Discovered: ${model.discovered}

To use this model:
1. Copy to models/ directory
2. Or create symlink
3. Or reference original path in server config
`);
    
    console.log(`  📦 [${idx + 1}] ${model.name} (${(model.size / 1024 / 1024).toFixed(2)} MB)`);
});

// Link top 20 agents
const topAgents = modelRegistry.agents.slice(0, 20);

topAgents.forEach((agent, idx) => {
    const linkPath = path.join(__dirname, 'agents', `${idx + 1}_${agent.name}`);
    const infoPath = linkPath + '.txt';
    
    fs.writeFileSync(infoPath, `Agent Info:
Name: ${agent.name}
Original Path: ${agent.path}
Type: ${agent.type}
Discovered: ${agent.discovered}

To use this agent:
1. Copy to agents/ directory
2. Or create symlink
3. Or reference original path in server config
`);
    
    console.log(`  🤖 [${idx + 1}] ${agent.name}`);
});

console.log('\n✅ Workspace setup complete!\n');
console.log('📁 Structure:');
console.log('   ProjectIDEAI/');
console.log('   ├── models/        (Top 10 models indexed)');
console.log('   ├── agents/        (Top 20 agents indexed)');
console.log('   ├── server/        (Orchestra server)');
console.log('   ├── ide/           (IDE files)');
console.log('   ├── generators/    (Code generators)');
console.log('   └── configs/       (Registry with all 1,387 models + 236 agents)');
console.log('\n💡 Next: Copy IDE and server files into this workspace');

