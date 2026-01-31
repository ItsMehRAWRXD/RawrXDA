#!/usr/bin/env node

/**
 * RawrZ Toggle CLI - Command line interface for managing toggles
 */

const toggleManager = require('../src/config/toggles');
const readline = require('readline');

const rl = readline.createInterface({
    input: process.stdin,
    output: process.stdout
});

function printHeader() {
    console.log(`
╔══════════════════════════════════════════════════════════════╗
║                    RawrZ Toggle CLI                         ║
║              Full Functionality Control                     ║
╚══════════════════════════════════════════════════════════════╝
`);
}

function printHelp() {
    console.log(`
Available Commands:
  list                    - List all toggles
  list <category>         - List toggles by category
  status                  - Show toggle status summary
  enable <toggle>         - Enable a toggle
  disable <toggle>        - Disable a toggle
  toggle <toggle>         - Toggle a toggle on/off
  enable-all              - Enable all toggles (full functionality)
  disable-security        - Disable all security toggles (DANGEROUS)
  save                    - Save current configuration
  help                    - Show this help
  exit                    - Exit the CLI

Categories: security, features, stealth, system, development, performance
`);
}

function printToggles(toggles) {
    if (Object.keys(toggles).length === 0) {
        console.log('No toggles found.');
        return;
    }

    console.log('\n┌─────────────────────────────────────────────────────────────────┐');
    console.log('│ Toggle Name                    │ Status  │ Category  │ Impact  │');
    console.log('├─────────────────────────────────────────────────────────────────┤');

    for (const [key, toggle] of Object.entries(toggles)) {
        const status = toggle.enabled ? '✅ ON ' : '❌ OFF';
        const category = toggle.category.padEnd(9);
        const impact = toggle.impact.padEnd(7);
        const name = key.padEnd(30);
        
        console.log(`│ ${name} │ ${status} │ ${category} │ ${impact} │`);
    }
    
    console.log('└─────────────────────────────────────────────────────────────────┘');
}

function printStatus() {
    const status = toggleManager.getStatus();
    
    console.log('\n📊 Toggle Status Summary:');
    console.log(`   Total Toggles: ${status.total}`);
    console.log(`   Enabled: ${status.enabled}`);
    console.log(`   Disabled: ${status.disabled}`);
    
    console.log('\n📁 By Category:');
    for (const [category, stats] of Object.entries(status.categories)) {
        console.log(`   ${category}: ${stats.enabled}/${stats.enabled + stats.disabled} enabled`);
    }
}

function askQuestion(question) {
    return new Promise((resolve) => {
        rl.question(question, (answer) => {
            resolve(answer.trim());
        });
    });
}

async function handleCommand(input) {
    const parts = input.split(' ');
    const command = parts[0].toLowerCase();
    const args = parts.slice(1);

    switch (command) {
        case 'list':
            if (args.length > 0) {
                const category = args[0];
                const toggles = toggleManager.getByCategory(category);
                console.log(`\n🔧 Toggles in category: ${category}`);
                printToggles(toggles);
            } else {
                console.log('\n🔧 All Toggles:');
                printToggles(toggleManager.getAll());
            }
            break;

        case 'status':
            printStatus();
            break;

        case 'enable':
            if (args.length === 0) {
                console.log('❌ Please specify a toggle to enable');
                break;
            }
            const toggleToEnable = args[0];
            if (toggleManager.enable(toggleToEnable)) {
                console.log(`✅ Enabled toggle: ${toggleToEnable}`);
            } else {
                console.log(`❌ Toggle not found: ${toggleToEnable}`);
            }
            break;

        case 'disable':
            if (args.length === 0) {
                console.log('❌ Please specify a toggle to disable');
                break;
            }
            const toggleToDisable = args[0];
            if (toggleManager.disable(toggleToDisable)) {
                console.log(`❌ Disabled toggle: ${toggleToDisable}`);
            } else {
                console.log(`❌ Toggle not found: ${toggleToDisable}`);
            }
            break;

        case 'toggle':
            if (args.length === 0) {
                console.log('❌ Please specify a toggle to toggle');
                break;
            }
            const toggleToToggle = args[0];
            const newState = toggleManager.toggle(toggleToToggle);
            console.log(`${newState ? '✅' : '❌'} Toggled ${toggleToToggle}: ${newState ? 'ON' : 'OFF'}`);
            break;

        case 'enable-all':
            const confirmEnableAll = await askQuestion('⚠️  Enable ALL toggles for full functionality? (y/N): ');
            if (confirmEnableAll.toLowerCase() === 'y') {
                const enabled = toggleManager.enableFullFunctionality();
                console.log(`✅ Enabled ${enabled} toggles for full functionality`);
            } else {
                console.log('❌ Operation cancelled');
            }
            break;

        case 'disable-security':
            const confirmDisableSecurity = await askQuestion('🚨 DANGER: Disable ALL security toggles? This removes safety restrictions! (y/N): ');
            if (confirmDisableSecurity.toLowerCase() === 'y') {
                const disabled = toggleManager.disableAllSecurity();
                console.log(`🚨 Disabled ${disabled} security toggles - DANGEROUS MODE ENABLED`);
            } else {
                console.log('❌ Operation cancelled');
            }
            break;

        case 'save':
            toggleManager.saveToFile();
            console.log('💾 Configuration saved to file');
            break;

        case 'help':
            printHelp();
            break;

        case 'exit':
            console.log('👋 Goodbye!');
            rl.close();
            process.exit(0);
            break;

        default:
            console.log(`❌ Unknown command: ${command}`);
            console.log('Type "help" for available commands');
    }
}

async function main() {
    printHeader();
    printHelp();
    
    console.log('💡 Tip: Type "status" to see current toggle configuration');
    console.log('💡 Tip: Type "list security" to see security-related toggles\n');

    while (true) {
        try {
            const input = await askQuestion('🔧 RawrZ> ');
            await handleCommand(input);
            console.log(); // Add spacing
        } catch (error) {
            console.error('❌ Error:', error.message);
        }
    }
}

// Handle Ctrl+C gracefully
process.on('SIGINT', () => {
    console.log('\n👋 Goodbye!');
    rl.close();
    process.exit(0);
});

// Run the CLI
main().catch(console.error);
