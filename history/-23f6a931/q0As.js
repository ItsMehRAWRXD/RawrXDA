#!/usr/bin/env node

/*
  CyberForge Advanced Payload CLI
  Command-line interface for enhanced file type support and payload generation
*/

import { AdvancedPayloadBuilder } from './engines/advanced-payload-builder.js';
import { FileTypeManager } from './engines/file-types/file-type-manager.js';
import { PayloadResearchFramework } from './engines/research/payload-research-framework.js';

const builder = new AdvancedPayloadBuilder();

// Display banner
function displayBanner() {
  console.log(`
╔═══════════════════════════════════════════════════════════════════════════════╗
║                    CyberForge Advanced Security Suite v2.0.0                 ║
║                        Enhanced File Type Support System                     ║
╠═══════════════════════════════════════════════════════════════════════════════╣
║  Comprehensive payload generation with support for:                          ║
║  • x86 & x64 Windows Executables (PE32/PE32+)                               ║
║  • Windows DLLs (x86/x64) with reflective loading                           ║
║  • Linux Executables (ELF32/ELF64) and Shared Libraries                     ║
║  • Raw Shellcode (x86/x64) for direct injection                             ║
║  • Research Framework Compatible Payloads                                    ║
║  • Educational Security Tool Demonstrations                                  ║
║                                                                              ║
║  ⚠️  FOR AUTHORIZED RESEARCH AND EDUCATIONAL PURPOSES ONLY ⚠️                ║
╚═══════════════════════════════════════════════════════════════════════════════╝
`);
}

// Display help information
function displayHelp() {
  console.log(`
🚀 CyberForge Advanced Payload CLI - Usage Guide

BASIC USAGE:
  node payload-cli.js [command] [options]

COMMANDS:
  build          Generate a new payload
  list-targets   Show all supported targets
  research       Generate research framework payload
  validate       Validate payload configuration
  help           Show this help message

BUILD OPTIONS:
  --target <target>          Target type (required)
  --purpose <purpose>        Build purpose (required)
  --output <path>           Output directory (default: ./output)
  --encryption <level>      Encryption level (basic|advanced|quantum)
  --polymorphic <level>     Polymorphic level (low|medium|high|extreme)
  --anti-detection         Enable anti-detection features
  --research               Enable research mode with ethical safeguards
  --authorized             Confirm authorized use (required for research)

SUPPORTED TARGETS:
  📁 Windows Executables:
     • windows-x86-executable    - 32-bit Windows PE executable
     • windows-x64-executable    - 64-bit Windows PE executable
     • windows-x86-dll          - 32-bit Windows DLL
     • windows-x64-dll          - 64-bit Windows DLL
     • windows-driver           - Windows kernel driver

  📁 Linux Executables:
     • linux-x86-executable     - 32-bit Linux ELF executable
     • linux-x64-executable     - 64-bit Linux ELF executable
     • linux-shared-library     - Linux shared object (.so)

  📁 Cross-Platform:
     • shellcode-x86            - 32-bit position-independent shellcode
     • shellcode-x64            - 64-bit position-independent shellcode

  📁 Research Frameworks:
     • research-stealer          - Educational stealer demonstration
     • research-rat              - Educational RAT simulation
     • research-c2-client        - C2 framework compatibility test
     • research-http-loader      - HTTP loader demonstration

EXAMPLES:
  # Generate 64-bit Windows executable for research
  node payload-cli.js build --target windows-x64-executable --purpose "Security research" --research --authorized

  # Generate Linux shellcode with high polymorphism
  node payload-cli.js build --target shellcode-x64 --purpose "Penetration testing" --polymorphic high

  # Generate research stealer for educational purposes
  node payload-cli.js research --target research-stealer --purpose "Student demonstration" --framework custom --authorized

  # List all available targets
  node payload-cli.js list-targets

RESEARCH MODE REQUIREMENTS:
  • Must specify --authorized flag to confirm authorized use
  • Must provide legitimate research purpose
  • Payloads include ethical safeguards and monitoring
  • Subject to research-only license terms

SECURITY FEATURES:
  • Quantum-resistant encryption algorithms
  • Advanced polymorphic code generation
  • Anti-detection and evasion techniques
  • Comprehensive audit logging
  • Ethical safeguards for research payloads

For detailed documentation, visit: https://github.com/cyberforge/advanced-security-suite
`);
}

// List supported targets
function listTargets() {
  const targets = builder.supportedTargets;
  
  console.log(`
🎯 SUPPORTED PAYLOAD TARGETS

📦 WINDOWS EXECUTABLES:
`);
  
  Object.entries(targets).forEach(([key, target]) => {
    if (target.platform === 'windows') {
      console.log(`   • ${key.padEnd(30)} - ${target.fileType} (${target.architecture})`);
      console.log(`     Capabilities: ${target.capabilities.join(', ')}`);
      if (target.researchType) {
        console.log(`     Research Type: ${target.researchType}`);
      }
      console.log('');
    }
  });

  console.log(`📦 LINUX EXECUTABLES:`);
  Object.entries(targets).forEach(([key, target]) => {
    if (target.platform === 'linux') {
      console.log(`   • ${key.padEnd(30)} - ${target.fileType} (${target.architecture})`);
      console.log(`     Capabilities: ${target.capabilities.join(', ')}`);
      console.log('');
    }
  });

  console.log(`📦 CROSS-PLATFORM:`);
  Object.entries(targets).forEach(([key, target]) => {
    if (target.platform === 'any') {
      console.log(`   • ${key.padEnd(30)} - ${target.fileType} (${target.architecture})`);
      console.log(`     Capabilities: ${target.capabilities.join(', ')}`);
      console.log('');
    }
  });

  console.log(`📦 RESEARCH FRAMEWORKS:`);
  Object.entries(targets).forEach(([key, target]) => {
    if (target.researchType) {
      console.log(`   • ${key.padEnd(30)} - ${target.fileType} (${target.architecture})`);
      console.log(`     Research: ${target.researchType}`);
      console.log(`     Capabilities: ${target.capabilities.join(', ')}`);
      console.log('');
    }
  });
}

// Build payload
async function buildPayload(args) {
  const config = parseArgs(args);
  
  // Validate required arguments
  if (!config.target) {
    console.error('❌ Error: --target is required');
    console.log('Use "node payload-cli.js list-targets" to see available targets');
    process.exit(1);
  }

  if (!config.purpose) {
    console.error('❌ Error: --purpose is required');
    console.log('Example: --purpose "Security research and education"');
    process.exit(1);
  }

  // Check research mode requirements
  if (config.research && !config.authorized) {
    console.error('❌ Error: Research mode requires --authorized flag');
    console.log('This confirms you are authorized to generate research payloads');
    process.exit(1);
  }

  console.log('🚀 Starting payload build process...');
  console.log(`📋 Target: ${config.target}`);
  console.log(`📝 Purpose: ${config.purpose}`);
  
  try {
    const result = await builder.buildPayload(config);
    
    if (result.success) {
      console.log('\n✅ BUILD SUCCESSFUL!');
      console.log('═══════════════════════════════════════');
      console.log(`📦 Build ID: ${result.buildId}`);
      console.log(`⏱️  Build Time: ${result.buildTime}s`);
      console.log(`📊 Package Size: ${builder.formatBytes(result.package.totalSize)}`);
      console.log(`🏛️  Architecture: ${result.config.architecture}`);
      console.log(`🖥️  Platform: ${result.config.platform}`);
      console.log(`📦 File Type: ${result.config.fileType}`);
      
      if (result.config.research) {
        console.log('\n🔬 RESEARCH MODE ENABLED');
        console.log('✅ Ethical safeguards applied');
        console.log('✅ Audit logging enabled');
        console.log('✅ Research compliance verified');
      }

      console.log('\n📄 GENERATED FILES:');
      console.log('• Executable payload');
      console.log('• Documentation and README');
      console.log('• Testing and validation tools');
      if (result.config.generateLoader) {
        console.log('• Payload loader scripts');
      }

      console.log('\n⚠️  IMPORTANT REMINDERS:');
      console.log('• Use only for authorized purposes');
      console.log('• Follow all applicable laws and regulations');
      console.log('• Maintain proper audit trails');
      console.log('• Respect ethical guidelines');

    } else {
      console.error('\n❌ BUILD FAILED!');
      console.error('═══════════════════════════════════════');
      console.error(`🚨 Error: ${result.error}`);
      console.error(`⏱️  Failed after: ${result.buildTime}s`);
    }

  } catch (error) {
    console.error('\n💥 UNEXPECTED ERROR!');
    console.error('═══════════════════════════════════════');
    console.error(`🚨 ${error.message}`);
    process.exit(1);
  }
}

// Generate research payload
async function generateResearchPayload(args) {
  const config = parseArgs(args);
  
  if (!config.authorized) {
    console.error('❌ Error: Research payloads require --authorized flag');
    process.exit(1);
  }

  if (!config.target || !config.target.startsWith('research-')) {
    console.error('❌ Error: Research mode requires a research-* target');
    console.log('Available research targets: research-stealer, research-rat, research-c2-client, research-http-loader');
    process.exit(1);
  }

  config.research = true;
  config.authorizedUse = true;
  config.researchPurpose = config.purpose;

  await buildPayload(['build', ...args.slice(1)]);
}

// Validate configuration
function validateConfig(args) {
  const config = parseArgs(args);
  
  console.log('🔍 Validating payload configuration...');
  
  try {
    // Check target
    if (!config.target) {
      throw new Error('Target not specified');
    }
    
    if (!builder.supportedTargets[config.target]) {
      throw new Error(`Unsupported target: ${config.target}`);
    }
    
    // Check purpose
    if (!config.purpose) {
      throw new Error('Purpose not specified');
    }
    
    // Check research requirements
    if (config.research || config.target.startsWith('research-')) {
      if (!config.authorized) {
        throw new Error('Research payloads require authorization confirmation');
      }
    }

    console.log('✅ Configuration is valid!');
    console.log(`📋 Target: ${config.target}`);
    console.log(`📝 Purpose: ${config.purpose}`);
    console.log(`🔒 Security: ${config.encryption || 'advanced'}`);
    console.log(`🧬 Polymorphic: ${config.polymorphic || 'high'}`);

  } catch (error) {
    console.error(`❌ Configuration error: ${error.message}`);
    process.exit(1);
  }
}

// Parse command line arguments
function parseArgs(args) {
  const config = {};
  
  for (let i = 0; i < args.length; i++) {
    const arg = args[i];
    const nextArg = args[i + 1];
    
    switch (arg) {
      case '--target':
        config.target = nextArg;
        i++;
        break;
      case '--purpose':
        config.purpose = nextArg;
        i++;
        break;
      case '--output':
        config.output = nextArg;
        i++;
        break;
      case '--encryption':
        config.encryptionLevel = nextArg;
        i++;
        break;
      case '--polymorphic':
        config.polymorphicLevel = nextArg;
        i++;
        break;
      case '--framework':
        config.framework = nextArg;
        i++;
        break;
      case '--research':
        config.research = true;
        break;
      case '--authorized':
        config.authorized = true;
        config.authorizedUse = true;
        break;
      case '--anti-detection':
        config.antiDetection = true;
        break;
      case '--generate-loader':
        config.generateLoader = true;
        break;
    }
  }
  
  return config;
}

// Main CLI handler
async function main() {
  const args = process.argv.slice(2);
  
  if (args.length === 0) {
    displayBanner();
    displayHelp();
    return;
  }

  const command = args[0];
  
  switch (command) {
    case 'build':
      displayBanner();
      await buildPayload(args.slice(1));
      break;
      
    case 'research':
      displayBanner();
      await generateResearchPayload(args.slice(1));
      break;
      
    case 'list-targets':
      displayBanner();
      listTargets();
      break;
      
    case 'validate':
      displayBanner();
      validateConfig(args.slice(1));
      break;
      
    case 'help':
    case '--help':
    case '-h':
      displayBanner();
      displayHelp();
      break;
      
    default:
      console.error(`❌ Unknown command: ${command}`);
      console.log('Use "node payload-cli.js help" for usage information');
      process.exit(1);
  }
}

// Handle uncaught errors
process.on('unhandledRejection', (error) => {
  console.error('\n💥 UNHANDLED ERROR!');
  console.error('═══════════════════════════════════════');
  console.error(error);
  process.exit(1);
});

process.on('uncaughtException', (error) => {
  console.error('\n💥 UNCAUGHT EXCEPTION!');
  console.error('═══════════════════════════════════════');
  console.error(error);
  process.exit(1);
});

// Run the CLI
if (import.meta.url === `file://${process.argv[1]}`) {
  main().catch(error => {
    console.error('\n💥 CLI ERROR!');
    console.error('═══════════════════════════════════════');
    console.error(error);
    process.exit(1);
  });
}