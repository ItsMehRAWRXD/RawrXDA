const { spawn } = require('child_process');
const path = require('path');

class SandboxRunner {
    static execute(language, code) {
        return new Promise((resolve, reject) => {
            const docker = spawn('docker', [
                'run', '--rm',
                '--memory=256m', '--cpus=0.5',
                '--network=none', '--read-only',
                '--tmpfs=/tmp:noexec,nosuid,size=100m',
                '-i', 'pi-engine-pro'
            ]);

            let output = '';
            let error = '';

            docker.stdout.on('data', (data) => output += data);
            docker.stderr.on('data', (data) => error += data);
            
            docker.on('close', (code) => {
                resolve({ exitCode: code, output, error });
            });

            docker.stdin.write(code);
            docker.stdin.end();
        });
    }
}

// VS Code integration
if (process.argv[2]) {
    const fs = require('fs');
    const code = fs.readFileSync(process.argv[2], 'utf8');
    const ext = path.extname(process.argv[2]);
    const language = ext === '.java' ? 'java' : ext === '.py' ? 'python' : 'cpp';
    
    SandboxRunner.execute(language, code)
        .then(result => console.log(result.output))
        .catch(err => console.error(err));
}