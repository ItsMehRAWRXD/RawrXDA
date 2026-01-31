const { exec } = require('child_process');
const fs = require('fs');

class IDEInstaller {
    async installToolchain() {
        console.log('Setting up IDE toolchain...');
        
        // Install .NET
        await this.runCommand('install-dotnet.bat');
        
        // Install MinGW + NASM
        await this.runCommand('install-mingw-nasm.bat');
        
        // Configure IDE
        this.configureIDE();
    }
    
    configureIDE() {
        const config = {
            toolchain: {
                dotnet: { path: '%USERPROFILE%\\.dotnet\\dotnet.exe' },
                gcc: { path: 'C:\\mingw64\\bin\\gcc.exe' },
                nasm: { path: 'C:\\nasm\\nasm-2.16.01\\nasm.exe' }
            },
            buildModes: ['dotnet', 'asm', 'native']
        };
        
        fs.writeFileSync('ide-toolchain.json', JSON.stringify(config, null, 2));
    }
    
    runCommand(cmd) {
        return new Promise((resolve, reject) => {
            exec(cmd, (error, stdout) => {
                if (error) reject(error);
                else resolve(stdout);
            });
        });
    }
}

new IDEInstaller().installToolchain();