import React from 'react';
import { Terminal as XTerm } from 'xterm';
import { FitAddon } from 'xterm-addon-fit';
import '../styles/xterm-terminal.css';
import { useIdeFeatures } from '../contexts/IdeFeaturesContext';
import {
  CopySupportLineButton,
  MinimalSurfaceM814Footer,
  focusVisibleRing,
  workspaceFolderBasename,
  MINIMALISTIC_DOC
} from '../utils/minimalisticM08M14';

const SHELL_STORAGE = 'rawrxd.terminal.shell';

/**
 * Embedded PTY when preload/main expose terminal IPC; otherwise shows a clear disabled state.
 * Elevated shells may still open in separate windows.
 */
const Terminal = React.forwardRef(function Terminal({ projectRoot }, ref) {
  const { pushToast, noisyLog, playUiSound, setStatusLine } = useIdeFeatures();
  const containerRef = React.useRef(null);
  const xtermRef = React.useRef(null);
  const fitRef = React.useRef(null);
  const sessionIdRef = React.useRef(null);
  const unsubsRef = React.useRef([]);

  const [shell, setShell] = React.useState(() => {
    try {
      return localStorage.getItem(SHELL_STORAGE) || (window.electronAPI?.platform === 'win32' ? 'powershell' : 'bash');
    } catch {
      return 'powershell';
    }
  });
  const [ptyError, setPtyError] = React.useState('');
  const [statusNote, setStatusNote] = React.useState('');

  React.useEffect(() => {
    try {
      localStorage.setItem(SHELL_STORAGE, shell);
    } catch {
      /* ignore */
    }
  }, [shell]);

  const teardownSession = React.useCallback(async () => {
    unsubsRef.current.forEach((u) => {
      try {
        u();
      } catch {
        /* ignore */
      }
    });
    unsubsRef.current = [];
    const id = sessionIdRef.current;
    sessionIdRef.current = null;
    if (id != null && window.electronAPI?.terminalKill) {
      try {
        await window.electronAPI.terminalKill(id);
      } catch {
        /* ignore */
      }
    }
    if (xtermRef.current) {
      try {
        xtermRef.current.dispose();
      } catch {
        /* ignore */
      }
      xtermRef.current = null;
    }
    fitRef.current = null;
  }, []);

  const bootSession = React.useCallback(async () => {
    const api = window.electronAPI;
    if (!api?.terminalCreate || !containerRef.current) {
      setPtyError('Embedded PTY is disabled in this build. Use external console windows or agent run_terminal steps.');
      setStatusNote('');
      setStatusLine('[terminal] PTY disabled in this build');
      return;
    }

    await teardownSession();
    setPtyError('');
    setStatusNote('Starting shell…');

    const create = await api.terminalCreate({ shell, cols: 80, rows: 24 });
    if (!create.success) {
      setPtyError(create.error || 'terminal session init failed');
      setStatusNote('');
      noisyLog('[terminal] create failed', create.error);
      setStatusLine('[terminal] PTY create failed — npm run rebuild:pty (node-pty vs Electron ABI)');
      return;
    }

    sessionIdRef.current = create.id;
    noisyLog('[terminal] session', create.id, create.shell, create.cwd);
    setStatusNote(`${create.shell} @ ${create.cwd}`);

    const term = new XTerm({
      cursorBlink: true,
      fontSize: 13,
      fontFamily: 'Consolas, "Cascadia Mono", monospace',
      theme: {
        background: '#0f172a',
        foreground: '#e2e8f0',
        cursor: '#38bdf8',
        selectionBackground: '#33415580'
      }
    });
    const fit = new FitAddon();
    term.loadAddon(fit);
    term.open(containerRef.current);
    fit.fit();
    xtermRef.current = term;
    fitRef.current = fit;

    await api.terminalResize(create.id, term.cols, term.rows);

    const offData = api.onTerminalData((sid, data) => {
      if (sid === create.id && xtermRef.current) {
        xtermRef.current.write(data);
      }
    });
    const offExit = api.onTerminalExit((sid, code) => {
      if (sid === create.id) {
        noisyLog('[terminal] exit', code);
        if (xtermRef.current) {
          xtermRef.current.writeln(`\r\n\x1b[33m[Process exited with code ${code}]\x1b[0m`);
        }
        sessionIdRef.current = null;
      }
    });
    unsubsRef.current = [offData, offExit];

    term.onData((d) => {
      const sid = sessionIdRef.current;
      if (sid != null) api.terminalWrite(sid, d);
    });

    const ro = new ResizeObserver(() => {
      const t = xtermRef.current;
      const f = fitRef.current;
      const sid = sessionIdRef.current;
      if (!t || !f || sid == null) return;
      try {
        f.fit();
        api.terminalResize(sid, t.cols, t.rows);
      } catch {
        /* ignore */
      }
    });
    ro.observe(containerRef.current);
    unsubsRef.current.push(() => ro.disconnect());

    term.focus();
    setStatusLine('[terminal] PTY ready');
  }, [shell, teardownSession, noisyLog, setStatusLine]);

  React.useEffect(() => {
    void bootSession();
    return () => {
      void teardownSession();
    };
  }, [bootSession, teardownSession, projectRoot]);

  const copyProjectRoot = () => {
    if (!projectRoot) {
      playUiSound('warn');
      setStatusLine('[terminal] no projectRoot — Open project');
      noisyLog('[terminal] copy cwd skipped — no project');
      pushToast({
        title: 'Workspace',
        message: 'Open a project first.',
        variant: 'warn',
        durationMs: 2400
      });
      return;
    }
    const p = String(projectRoot);
    if (navigator.clipboard?.writeText) {
      navigator.clipboard.writeText(p).then(
        () => {
          noisyLog('[terminal] copied cwd path');
          setStatusLine('[terminal] copied workspace path to clipboard');
          pushToast({ title: 'Copied', message: p, variant: 'success', durationMs: 1800 });
        },
        () => {
          noisyLog('[terminal] clipboard denied');
          setStatusLine('[terminal] copy failed — permission denied');
          pushToast({ title: 'Clipboard', message: 'Denied', variant: 'error', durationMs: 2000 });
        }
      );
    } else {
      setStatusLine('[terminal] clipboard API unavailable — copy manually');
      pushToast({ title: 'Clipboard', message: p, variant: 'info', durationMs: 5000 });
    }
  };

  const openElevated = async (kind) => {
    const api = window.electronAPI;
    if (!api?.terminalOpenElevated) return;
    const r = await api.terminalOpenElevated({ shell: kind });
    if (r.success) {
      playUiSound('tick');
      pushToast({ title: 'Elevation', message: r.message || 'Check for UAC / new window.', variant: 'info', durationMs: 5000 });
      noisyLog('[terminal] elevated launch requested', kind);
    } else {
      pushToast({ title: 'Elevation', message: r.error || 'Failed', variant: 'error', durationMs: 4000 });
    }
  };

  const win = typeof window !== 'undefined' && window.electronAPI?.platform === 'win32';

  return (
    <div
      ref={ref}
      role="region"
      aria-label="Integrated terminal"
      className="flex flex-col bg-ide-bg border-t border-gray-700 shrink-0 min-h-[220px] max-h-[45vh] h-[min(40vh,420px)]"
    >
      <div className="px-2 py-1.5 bg-ide-toolbar border-b border-gray-700 flex flex-wrap items-center gap-2 text-xs">
        <span className="font-medium text-gray-200">Terminal</span>
        <select
          value={shell}
          onChange={(e) => setShell(e.target.value)}
          className={`bg-gray-900 border border-gray-600 rounded px-1 py-0.5 text-gray-200 ${focusVisibleRing}`}
          aria-label="Shell for integrated terminal"
          title="Shell for the embedded PTY (cwd = open project or home)."
        >
          {win ? (
            <>
              <option value="powershell">PowerShell</option>
              <option value="cmd">Command Prompt</option>
              <option value="pwsh">PowerShell 7 (pwsh)</option>
            </>
          ) : (
            <>
              <option value="bash">Bash (login)</option>
              <option value="zsh">zsh (login)</option>
            </>
          )}
        </select>
        <button
          type="button"
          onClick={() => bootSession()}
          className={`px-2 py-0.5 rounded border border-gray-600 text-gray-300 hover:bg-gray-700 ${focusVisibleRing}`}
          title="Kill current PTY and start a new one (e.g. after shell change)."
        >
          New session
        </button>
        <button
          type="button"
          onClick={copyProjectRoot}
          className={`px-2 py-0.5 rounded border border-gray-600 text-gray-300 hover:bg-gray-700 ${focusVisibleRing}`}
          title="Clipboard: full projectRoot path."
        >
          Copy cwd path
        </button>
        {win ? (
          <>
            <button
              type="button"
              onClick={() => openElevated('powershell')}
              className={`px-2 py-0.5 rounded border border-amber-700/60 text-amber-200 hover:bg-amber-950/40 ${focusVisibleRing}`}
              title="Opens a separate elevated PowerShell via UAC — not inside this panel."
            >
              Elevated PowerShell
            </button>
            <button
              type="button"
              onClick={() => openElevated('cmd')}
              className={`px-2 py-0.5 rounded border border-amber-700/60 text-amber-200 hover:bg-amber-950/40 ${focusVisibleRing}`}
              title="Opens a separate elevated cmd via UAC — not inside this panel."
            >
              Elevated cmd
            </button>
          </>
        ) : null}
        <span className="text-[10px] text-gray-500 ml-auto truncate max-w-[40%]" title={statusNote}>
          {ptyError ? <span className="text-rose-400">{ptyError}</span> : statusNote}
        </span>
      </div>
      {ptyError ? (
        <div className="p-3 text-xs text-rose-200 space-y-2 overflow-auto">
          <p>Embedded PTY unavailable (node-pty build must match this Electron version).</p>
          <pre className="bg-black/40 p-2 rounded text-[10px] whitespace-pre-wrap">{ptyError}</pre>
          <p className="text-gray-400">
            From <code className="text-cyan-300">bigdaddyg-ide</code>: run{' '}
            <code className="text-cyan-300">npm install</code> then <code className="text-cyan-300">npm run rebuild:pty</code>
          </p>
          {win ? (
            <p className="text-gray-500">
              Elevated buttons still work: they spawn an external Windows console with UAC.
            </p>
          ) : null}
        </div>
      ) : (
        <div ref={containerRef} className="flex-1 min-h-0 p-1 overflow-hidden" />
      )}
      <p className="text-[9px] text-gray-600 px-2 py-0.5 border-t border-gray-800">
        Embedded shell runs as your user. Elevated opens a separate window (UAC). Do not paste untrusted commands.
      </p>
      <MinimalSurfaceM814Footer
        surfaceId="terminal"
        offlineHint="PTY needs Electron + native rebuild; elevated buttons spawn external Windows consoles."
        docPath={MINIMALISTIC_DOC}
        m13Hint="Dev: noisyLog('[terminal]', …) on session lifecycle."
        className="px-2 pb-1"
      >
        <div className="flex flex-wrap gap-1 px-1 pb-0.5">
          <CopySupportLineButton
            getText={() =>
              `[terminal] shell=${shell} err=${ptyError ? '1' : '0'} workspace=${projectRoot ? workspaceFolderBasename(projectRoot) : 'none'}`
            }
            label="Copy support line"
            className={`text-[9px] px-1.5 py-0.5 rounded border border-gray-700 text-gray-400 hover:text-gray-200 ${focusVisibleRing}`}
            onCopied={() => {
              pushToast({ title: '[terminal]', message: 'Support line copied.', variant: 'success', durationMs: 2000 });
              setStatusLine('[terminal] support line copied');
            }}
            onFailed={() =>
              pushToast({ title: '[terminal]', message: 'Clipboard unavailable.', variant: 'warn', durationMs: 2600 })
            }
          />
        </div>
      </MinimalSurfaceM814Footer>
    </div>
  );
});

export default Terminal;
