"use strict";
/* ---------------------------------------------------------------------------------------------
 *  Copyright (c) Microsoft Corporation. All rights reserved.
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || function (mod) {
    if (mod && mod.__esModule) return mod;
    var result = {};
    if (mod != null) for (var k in mod) if (k !== "default" && Object.prototype.hasOwnProperty.call(mod, k)) __createBinding(result, mod, k);
    __setModuleDefault(result, mod);
    return result;
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.isProcessAlive = exports.ProgramPidWrapper = exports.Terminal = void 0;
const Path = __importStar(require("path"));
const FS = __importStar(require("fs"));
const CP = __importStar(require("child_process"));
const events_1 = require("events");
class Terminal {
    static launchInTerminal(dir, args, envVars) {
        return this.terminalService().launchInTerminal(dir, args, envVars);
    }
    static killTree(processId) {
        return this.terminalService().killTree(processId);
    }
    /*
     * Is the given runtime executable on the PATH.
     */
    static isOnPath(program) {
        return this.terminalService().isOnPath(program);
    }
    static terminalService() {
        if (!this._terminalService) {
            if (process.platform === 'win32') {
                this._terminalService = new WindowsTerminalService();
            }
            else if (process.platform === 'darwin') {
                this._terminalService = new MacTerminalService();
            }
            else if (process.platform === 'linux') {
                this._terminalService = new LinuxTerminalService();
            }
            else {
                this._terminalService = new DefaultTerminalService();
            }
        }
        return this._terminalService;
    }
}
exports.Terminal = Terminal;
class ProgramPidWrapper extends events_1.EventEmitter {
    constructor(pid) {
        super();
        this.pid = pid;
        if (pid) {
            this.startPollLoop(pid);
        }
    }
    kill(signal) {
        this.startPollLoop(this.pid, ProgramPidWrapper.killConfirmInterval);
        Terminal.killTree(this.pid).catch(err => {
            // ignore
        });
        return true;
    }
    startPollLoop(processId, interval = ProgramPidWrapper.terminationPollInterval) {
        if (this.loop) {
            clearInterval(this.loop.timer);
        }
        const loop = {
            processId,
            timer: setInterval(() => {
                if (!isProcessAlive(processId)) {
                    clearInterval(loop.timer);
                    this.emit('exit');
                }
            }, interval),
        };
        this.loop = loop;
    }
}
exports.ProgramPidWrapper = ProgramPidWrapper;
/**
 * How often to check and see if the process exited.
 */
ProgramPidWrapper.terminationPollInterval = 1000;
/**
 * How often to check and see if the process exited after we send a close signal.
 */
ProgramPidWrapper.killConfirmInterval = 200;
function isProcessAlive(processId) {
    try {
        // kill with signal=0 just test for whether the proc is alive. It throws if not.
        process.kill(processId, 0);
        return true;
    }
    catch {
        return false;
    }
}
exports.isProcessAlive = isProcessAlive;
class DefaultTerminalService {
    launchInTerminal(dir, args, envVars) {
        throw new Error('launchInTerminal not implemented');
    }
    killTree(pid) {
        // on linux and OS X we kill all direct and indirect child processes as well
        return new Promise((resolve, reject) => {
            try {
                const cmd = Path.join(__dirname, './terminateProcess.sh');
                const result = CP.spawnSync(cmd, [pid.toString()]);
                if (result.error) {
                    reject(result.error);
                }
                else {
                    resolve(undefined);
                }
            }
            catch (err) {
                reject(err);
            }
        });
    }
    isOnPath(program) {
        /*
        var which = FS.existsSync(DefaultTerminalService.WHICH) ? DefaultTerminalService.WHICH : DefaultTerminalService.WHERE;
        var cmd = Utils.format('{0} \'{1}\'', which, program);

        try {
            CP.execSync(cmd);

            return process.ExitCode == 0;
        }
        catch (Exception) {
            // ignore
        }

        return false;
        */
        return true;
    }
}
DefaultTerminalService.TERMINAL_TITLE = 'VS Code Console';
class WindowsTerminalService extends DefaultTerminalService {
    launchInTerminal(dir, args, envVars) {
        return new Promise((resolve, reject) => {
            const title = `"${dir} - ${WindowsTerminalService.TERMINAL_TITLE}"`;
            const command = `""${args.join('" "')}" & pause"`; // use '|' to only pause on non-zero exit code
            const cmdArgs = ['/c', 'start', title, '/wait', 'cmd.exe', '/c', command];
            // merge environment variables into a copy of the process.env
            const env = extendObject(extendObject({}, process.env), envVars);
            const options = {
                cwd: dir,
                env: env,
                windowsVerbatimArguments: true,
            };
            const cmd = CP.spawn(WindowsTerminalService.CMD, cmdArgs, options);
            cmd.on('error', reject);
            resolve(cmd);
        });
    }
    killTree(pid) {
        // when killing a process in Windows its child processes are *not* killed but become root processes.
        // Therefore we use TASKKILL.EXE
        return new Promise((resolve, reject) => {
            const cmd = `taskkill /F /T /PID ${pid}`;
            try {
                CP.execSync(cmd);
                resolve(undefined);
            }
            catch (err) {
                reject(err);
            }
        });
    }
}
WindowsTerminalService.CMD = 'cmd.exe';
class LinuxTerminalService extends DefaultTerminalService {
    launchInTerminal(dir, args, envVars) {
        return new Promise((resolve, reject) => {
            if (!FS.existsSync(LinuxTerminalService.LINUX_TERM)) {
                reject(new Error(`Cannot find '${LinuxTerminalService.LINUX_TERM}' for launching the node program. See http://go.microsoft.com/fwlink/?linkID=534832#_20002`));
                return;
            }
            const bashCommand = `cd "${dir}"; "${args.join('" "')}"; echo; read -p "${LinuxTerminalService.WAIT_MESSAGE}" -n1;`;
            const termArgs = [
                '--title',
                `"${LinuxTerminalService.TERMINAL_TITLE}"`,
                '-x',
                'bash',
                '-c',
                `''${bashCommand}''`, // wrapping argument in two sets of ' because node is so "friendly" that it removes one set...
            ];
            // merge environment variables into a copy of the process.env
            const env = extendObject(extendObject({}, process.env), envVars);
            const options = {
                env: env,
            };
            const cmd = CP.spawn(LinuxTerminalService.LINUX_TERM, termArgs, options);
            cmd.on('error', reject);
            cmd.on('exit', (code) => {
                if (code === 0) {
                    // OK
                    resolve(undefined); // since cmd is not the terminal process but just a launcher, we do not pass it in the resolve to the caller
                }
                else {
                    reject(new Error(`exit code: ${code}`));
                }
            });
        });
    }
}
LinuxTerminalService.LINUX_TERM = '/usr/bin/gnome-terminal'; // private const string LINUX_TERM = "/usr/bin/x-terminal-emulator";
LinuxTerminalService.WAIT_MESSAGE = 'Press any key to continue...';
class MacTerminalService extends DefaultTerminalService {
    launchInTerminal(dir, args, envVars) {
        return new Promise((resolve, reject) => {
            // first fix the PATH so that 'runtimePath' can be found if installed with 'brew'
            // Utilities.FixPathOnOSX();
            // On OS X we do not launch the program directly but we launch an AppleScript that creates (or reuses) a Terminal window
            // and then launches the program inside that window.
            const osaArgs = [
                Path.join(__dirname, './TerminalHelper.scpt'),
                '-t',
                MacTerminalService.TERMINAL_TITLE,
                '-w',
                dir,
            ];
            for (const a of args) {
                osaArgs.push('-pa');
                osaArgs.push(a);
            }
            if (envVars) {
                for (const key in envVars) {
                    osaArgs.push('-e');
                    osaArgs.push(key + '=' + envVars[key]);
                }
            }
            let stderr = '';
            const osa = CP.spawn(MacTerminalService.OSASCRIPT, osaArgs);
            osa.on('error', reject);
            osa.stderr.on('data', (data) => {
                stderr += data.toString();
            });
            osa.on('exit', (code) => {
                if (code === 0) {
                    // OK
                    resolve(undefined); // since cmd is not the terminal process but just the osa tool, we do not pass it in the resolve to the caller
                }
                else {
                    if (stderr) {
                        reject(new Error(stderr));
                    }
                    else {
                        reject(new Error(`exit code: ${code}`));
                    }
                }
            });
        });
    }
}
MacTerminalService.OSASCRIPT = '/usr/bin/osascript'; // osascript is the AppleScript interpreter on OS X
// ---- private utilities ----
function extendObject(objectCopy, object) {
    return { ...objectCopy, ...object };
}
//# sourceMappingURL=terminal.js.map