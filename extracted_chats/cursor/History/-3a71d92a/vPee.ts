// BigDaddyGEngine/ui/terminal/ProcessExecutor.ts
// Process Execution Engine with Shell Integration

export interface ProcessOutput {
  stdout: string;
  stderr: string;
  exitCode: number;
  duration: number;
}

export interface ProcessJob {
  id: string;
  command: string;
  args: string[];
  cwd: string;
  status: 'running' | 'completed' | 'error';
  output: ProcessOutput;
  startTime: number;
}

export class ProcessExecutor {
  private jobs: Map<string, ProcessJob> = new Map();
  private activeJob: ProcessJob | null = null;

  /**
   * Execute a command asynchronously
   */
  async execute(command: string, args: string[] = [], cwd: string = '/'): Promise<ProcessOutput> {
    const jobId = this.createJobId();
    const startTime = Date.now();

    const job: ProcessJob = {
      id: jobId,
      command,
      args,
      cwd,
      status: 'running',
      output: { stdout: '', stderr: '', exitCode: 0, duration: 0 },
      startTime
    };

    this.jobs.set(jobId, job);
    this.activeJob = job;

    try {
      // Simulate command execution
      const output = await this.runCommand(command, args, cwd);
      
      job.status = output.exitCode === 0 ? 'completed' : 'error';
      job.output = {
        ...output,
        duration: Date.now() - startTime
      };

      return output;
    } catch (error) {
      job.status = 'error';
      job.output = {
        stdout: '',
        stderr: error instanceof Error ? error.message : 'Unknown error',
        exitCode: 1,
        duration: Date.now() - startTime
      };
      
      return job.output;
    }
  }

  /**
   * Stream command output in real-time
   */
  async executeStream(
    command: string,
    args: string[] = [],
    cwd: string = '/',
    onOutput?: (chunk: string) => void
  ): Promise<void> {
    const jobId = this.createJobId();
    const startTime = Date.now();

    const job: ProcessJob = {
      id: jobId,
      command,
      args,
      cwd,
      status: 'running',
      output: { stdout: '', stderr: '', exitCode: 0, duration: 0 },
      startTime
    };

    this.jobs.set(jobId, job);
    this.activeJob = job;

    // Simulate streaming output
    const output = await this.runCommandStream(command, args, cwd, onOutput);
    
    job.status = output.exitCode === 0 ? 'completed' : 'error';
    job.output = {
      ...output,
      duration: Date.now() - startTime
    };
  }

  /**
   * Kill a running process
   */
  kill(jobId: string): boolean {
    const job = this.jobs.get(jobId);
    if (job && job.status === 'running') {
      job.status = 'error';
      job.output.exitCode = 130; // SIGINT
      return true;
    }
    return false;
  }

  /**
   * Get process status
   */
  getStatus(jobId: string): ProcessJob | null {
    return this.jobs.get(jobId) || null;
  }

  /**
   * Get all jobs
   */
  getAllJobs(): ProcessJob[] {
    return Array.from(this.jobs.values());
  }

  private async runCommand(command: string, args: string[], cwd: string): Promise<ProcessOutput> {
    // Simulate command execution
    // In a real implementation, this would use Web Workers or WASM
    return new Promise((resolve) => {
      setTimeout(() => {
        resolve({
          stdout: `${command} executed successfully\n`,
          stderr: '',
          exitCode: 0,
          duration: 0
        });
      }, 100);
    });
  }

  private async runCommandStream(
    command: string,
    args: string[],
    cwd: string,
    onOutput?: (chunk: string) => void
  ): Promise<ProcessOutput> {
    // Simulate streaming output
    return new Promise((resolve) => {
      const chunks = ['Starting...\n', 'Processing...\n', 'Done!\n'];
      let index = 0;

      const interval = setInterval(() => {
        if (index < chunks.length) {
          onOutput?.(chunks[index]);
          index++;
        } else {
          clearInterval(interval);
          resolve({
            stdout: chunks.join(''),
            stderr: '',
            exitCode: 0,
            duration: 0
          });
        }
      }, 200);
    });
  }

  private createJobId(): string {
    return `job-${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
  }
}

export function getGlobalProcessExecutor(): ProcessExecutor {
  return globalProcessExecutor;
}

const globalProcessExecutor = new ProcessExecutor();
