import * as cp from 'child_process';
import * as path from 'path';

export class LlamaBridge {
  private exePath: string;

  constructor(ggufPath: string, ctxLen: number) {
    // path to llama-cli.exe (build from llama.cpp)
    this.exePath = path.join(__dirname, '..', 'lib', 'llama-cli.exe');
  }

  async complete(prompt: string, maxTokens = 80): Promise<string> {
    return new Promise((resolve, reject) => {
      const args = [
        '-m', 'C:\\Franken\\BigDaddyG-NO-REFUSE-Q2_K.gguf', // hardcoded for now
        '-p', prompt,
        '-n', maxTokens.toString(),
        '--temp', '0.3',
        '-c', '4096'
      ];
      const proc = cp.spawn(this.exePath, args, { stdio: ['pipe', 'pipe', 'pipe'] });
      let output = '';
      proc.stdout.on('data', (data: Buffer) => output += data.toString());
      proc.stderr.on('data', (data: Buffer) => console.error(data.toString()));
      proc.on('close', (code: number | null) => {
        if (code === 0) {
          resolve(output.trim());
        } else {
          reject(new Error(`llama-cli exited with code ${code}`));
        }
      });
      proc.on('error', reject);
    });
  }

  dispose() { /* no-op */ }
}