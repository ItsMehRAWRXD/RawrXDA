import React, { useState, useRef, useEffect, useMemo } from 'react';
// @ts-ignore TS1261: workspace opened with mixed root casing (d:/RawrXD vs d:/rawrxd)
import { useIdeFeatures } from '../contexts/IdeFeaturesContext';
import {
  focusVisibleRing,
  MinimalM08M14Footnote,
  copyTextToClipboard,
  MINIMALISTIC_DOC
} from '../utils/minimalisticM08M14';
import { invokeRawrxdWasmChat, clearRawrxdWasmInferenceCache } from '../utils/rawrxdWasmInference';
import { workspaceRelativePath } from '../utils/workspacePathUtils';

/**
 * Cursor / Copilot–style chat: WASM-first lane; optional agentic delegation to main `ai:invoke` (Electron).
 */
const CHAT_DRAFT_KEY = 'rawrxd.ide.chatDraft.v1';

const defaultThread = () => [
  {
    role: 'assistant',
    content:
      'Dock chat tries embedded WASM first. Bootstrap WASM echoes only — with “Autonomous provider fallback” on (Settings › AI), Electron delegates the same prompt to the toolbar provider for a real answer. Use the Agent dock for multi-step autonomous work.'
  }
];

const ChatPanel = ({ activeFile, projectRoot, settings }) => {
  const {
    playUiSound,
    pushToast,
    setStatusLine,
    noisyLog,
    announceA11y,
    settings: shellSettings,
    accessibilityReducedMotionEffective,
    toolbarActiveProviderId
  } = useIdeFeatures();
  const [messages, setMessages] = useState(() => defaultThread());
  const [input, setInput] = useState(() => {
    try {
      return localStorage.getItem(CHAT_DRAFT_KEY) || '';
    } catch {
      return '';
    }
  });
  const [busy, setBusy] = useState(false);
  const bottomRef = useRef(null);

  useEffect(() => {
    const t = window.setTimeout(() => {
      try {
        localStorage.setItem(CHAT_DRAFT_KEY, input);
      } catch {
        /* ignore */
      }
    }, 200);
    return () => window.clearTimeout(t);
  }, [input]);

  useEffect(() => {
    bottomRef.current?.scrollIntoView({
      behavior: accessibilityReducedMotionEffective ? 'auto' : 'smooth'
    });
  }, [messages, accessibilityReducedMotionEffective]);

  useEffect(() => {
    clearRawrxdWasmInferenceCache();
  }, [settings.wasmChatUrl]);

  useEffect(() => {
    noisyLog('[chat]', 'panel ready');
    setStatusLine('chat: ready');
  }, [noisyLog, setStatusLine]);

  const invokeToolbarChat = async (fullPrompt) => {
    const api = typeof window !== 'undefined' ? window.electronAPI : null;
    if (!api?.invokeAI) {
      return { ok: false, error: 'Toolbar AI route needs Electron (preload invokeAI).' };
    }
    const context = {
      temperature: settings.temperature,
      maxTokens: settings.maxTokens,
      preferLocalInferenceFirst: settings.preferLocalInferenceFirst !== false,
      projectRoot: projectRoot || undefined
    };
    const r = await api.invokeAI(toolbarActiveProviderId || null, fullPrompt, context);
    if (r?.success && r.data && typeof r.data.content === 'string' && r.data.content.trim()) {
      return { ok: true, content: r.data.content.trim(), lane: r.data.lane || 'ipc' };
    }
    return { ok: false, error: r?.error || 'Empty or failed IPC response' };
  };

  const personaPrefix = {
    copilot: 'You are GitHub Copilot. Answer concisely with code when relevant.\n\n',
    cursor: 'You are a Cursor-style coding agent. Prefer actionable steps and file-aware answers.\n\n',
    raw: 'You are the RawrXD IDE assistant.\n\n'
  }[settings.chatPersona || 'raw'];

  const send = async () => {
    const text = input.trim();
    if (!text || busy) return;

    let ctx = '';
    if (settings.attachActiveFileToChat && activeFile?.path) {
      const displayPath =
        projectRoot && activeFile.path
          ? workspaceRelativePath(activeFile.path, projectRoot) || activeFile.path
          : activeFile.path;
      ctx += `Active file: ${displayPath}\n`;
      if (activeFile.content) {
        const snippet = activeFile.content.slice(0, 12000);
        ctx += `\`\`\`\n${snippet}\n\`\`\`\n`;
      }
    }

    setMessages((m) => [...m, { role: 'user', content: text }]);
    setInput('');
    setBusy(true);
    playUiSound('chatSend');
    setStatusLine('chat: waiting for model…');
    const httpOnly = settings.chatTransport === 'ollama-http';
    announceA11y(httpOnly ? 'Chat sending via main-process provider.' : 'Chat sending. WASM lane first.');
    noisyLog('[chat]', 'send', { temperature: settings.temperature, maxTokens: settings.maxTokens });

    const prompt = `${personaPrefix}${ctx ? ctx + '\n' : ''}User: ${text}`;
    const fallbackOn = settings.chatAgenticProviderFallback !== false;
    const canDelegate = Boolean(typeof window !== 'undefined' && window.electronAPI?.invokeAI);

    try {
      let reply = '';

      if (httpOnly) {
        const ipc = await invokeToolbarChat(prompt);
        if (ipc.ok) {
          reply = ipc.content;
          playUiSound('success');
          if (shellSettings.noisyToasts) {
            pushToast({ title: 'Chat', message: `Provider reply (${ipc.lane})`, variant: 'success', durationMs: 2200 });
          }
          setStatusLine(`chat: ${ipc.lane}`);
          announceA11y('Chat reply ready from main-process provider.');
          noisyLog('[chat]', 'http-only ipc ok', ipc.lane);
        } else {
          reply = `Error: ${ipc.error}`;
          playUiSound('error');
          if (shellSettings.noisyToasts) {
            pushToast({ title: 'Chat', message: reply, variant: 'error', durationMs: 6000 });
          }
          setStatusLine('chat: provider error');
          announceA11y(`Chat provider error. ${ipc.error}`, { assertive: true });
          noisyLog('[chat]', 'http-only ipc failed', ipc.error);
        }
        setMessages((m) => [...m, { role: 'assistant', content: reply }]);
      } else {
        const wasmResult = await invokeRawrxdWasmChat(prompt, {
          maxTokens: settings.maxTokens,
          wasmUrl: settings.wasmChatUrl
        });
        const stub =
          wasmResult.ok &&
          wasmResult.lane === 'wasm-echo-stub' &&
          (!wasmResult.content || !String(wasmResult.content).trim());
        const shouldDelegate = fallbackOn && canDelegate && (stub || !wasmResult.ok);

        if (wasmResult.ok && !shouldDelegate) {
          reply = wasmResult.content;
          playUiSound('success');
          if (shellSettings.noisyToasts) {
            pushToast({ title: 'Chat', message: `WASM lane (${wasmResult.lane})`, variant: 'success', durationMs: 2000 });
          }
          setStatusLine(`chat: ${wasmResult.lane}`);
          noisyLog('[chat]', 'wasm reply ok', wasmResult.lane);
          announceA11y('Chat reply ready from WASM lane.');
        } else if (shouldDelegate) {
          const reason = stub ? 'bootstrap echo' : `WASM error: ${wasmResult.error}`;
          announceA11y(`Delegating to toolbar provider. ${reason}`, { assertive: !stub });
          setStatusLine(stub ? 'chat: delegating to provider…' : 'chat: WASM failed — delegating…');
          noisyLog('[chat]', 'delegate to ipc', { stub, toolbarActiveProviderId });
          const ipc = await invokeToolbarChat(prompt);
          if (ipc.ok) {
            reply = ipc.content;
            playUiSound('success');
            if (shellSettings.noisyToasts) {
              pushToast({
                title: 'Chat',
                message: stub ? 'Provider reply (after WASM bootstrap)' : 'Provider reply (WASM fallback)',
                variant: 'success',
                durationMs: 2600
              });
            }
            setStatusLine(`chat: ${ipc.lane} (delegated)`);
            announceA11y('Chat reply ready from delegated provider.');
          } else {
            reply = stub
              ? `Error: ${ipc.error}`
              : `Error: WASM: ${wasmResult.error}. Fallback: ${ipc.error}`;
            playUiSound('error');
            if (shellSettings.noisyToasts) {
              pushToast({ title: 'Chat', message: reply.slice(0, 200), variant: 'error', durationMs: 7000 });
            }
            setStatusLine('chat: delegation failed');
            announceA11y(`Chat delegation failed. ${ipc.error}`, { assertive: true });
          }
        } else if (stub) {
          reply =
            'Local WASM is bootstrap-only (echo). Turn on “Autonomous provider fallback” in Settings › AI and run in Electron with a configured toolbar provider, or switch Chat transport to HTTP provider.';
          playUiSound('warn');
          setStatusLine('chat: wasm bootstrap only');
          if (shellSettings.noisyToasts) {
            pushToast({
              title: 'Chat',
              message: 'WASM echo only — enable fallback or HTTP transport',
              variant: 'warn',
              durationMs: 5000
            });
          }
          announceA11y('WASM bootstrap only. Enable provider fallback in Settings.', { assertive: true });
          noisyLog('[chat]', 'stub no delegate', { fallbackOn, canDelegate });
        } else {
          reply = `Error: ${wasmResult.error}`;
          playUiSound('error');
          if (shellSettings.noisyToasts) {
            pushToast({
              title: 'Chat',
              message: `${reply} — Check WASM path in Settings › AI.`,
              variant: 'error',
              durationMs: 6000
            });
          }
          setStatusLine('chat: wasm error');
          noisyLog('[chat]', 'wasm failed', wasmResult.error);
          announceA11y(`Chat WASM error. ${wasmResult.error}`, { assertive: true });
        }

        setMessages((m) => [...m, { role: 'assistant', content: reply }]);
      }
    } catch (e) {
      playUiSound('error');
      announceA11y(`Chat exception. ${String(e?.message || e)}`, { assertive: true });
      if (shellSettings.noisyToasts) {
        pushToast({ title: 'Chat', message: e?.message || String(e), variant: 'error', durationMs: 5000 });
      }
      setStatusLine('chat: exception');
      noisyLog('[chat]', 'exception', e?.message);
      setMessages((m) => [...m, { role: 'assistant', content: `Error: ${e?.message || String(e)}` }]);
    } finally {
      setBusy(false);
    }
  };

  const lastAssistantText = useMemo(() => {
    for (let i = messages.length - 1; i >= 0; i--) {
      if (messages[i]?.role === 'assistant' && messages[i]?.content) return messages[i].content;
    }
    return '';
  }, [messages]);

  const copyLastReply = () => {
    const line = lastAssistantText.trim();
    if (!line) {
      pushToast({ title: 'Chat', message: 'Nothing to copy yet — send a message first.', variant: 'warn', durationMs: 2400 });
      return;
    }
    copyTextToClipboard(line, {
      onDone: () => {
        playUiSound('success');
        setStatusLine('chat: copied last assistant reply');
      },
      onFail: () =>
        pushToast({ title: 'Chat', message: 'Clipboard failed — copy from the thread manually.', variant: 'error', durationMs: 3000 })
    });
  };

  const copyLastDiagnostic = () => {
    const line = lastAssistantText.startsWith('Error:') ? lastAssistantText : '';
    if (!line) {
      pushToast({
        title: 'Chat',
        message: 'Latest reply is not an error line. Use “Copy reply” or select text in the thread.',
        variant: 'warn',
        durationMs: 3600
      });
      return;
    }
    copyTextToClipboard(line, {
      onDone: () => {
        playUiSound('success');
        setStatusLine('chat: copied last Error: diagnostic');
      },
      onFail: () =>
        pushToast({ title: 'Chat', message: 'Clipboard failed — copy from the thread manually.', variant: 'error', durationMs: 3000 })
    });
  };

  const clearThread = () => {
    setMessages(defaultThread());
    try {
      localStorage.removeItem(CHAT_DRAFT_KEY);
    } catch {
      /* ignore */
    }
    setInput('');
    setStatusLine('chat: thread cleared');
    noisyLog('[chat]', 'clear thread');
    playUiSound('tick');
  };

  return (
    <div
      className="flex flex-col h-full bg-ide-sidebar text-white min-w-0"
      role="region"
      aria-label="AI chat: WASM first, optional main-process provider delegation"
    >
      <div className="p-3 border-b border-gray-700 flex items-start justify-between gap-2 flex-wrap">
        <div className="min-w-0 flex-1">
          <h3 className="font-semibold text-sm">Chat</h3>
          <p className="text-[10px] text-gray-500 mt-0.5">
            WASM first (Settings › AI); optional delegation to the toolbar provider in Electron when fallback is on, or HTTP-only
            transport.
          </p>
        </div>
        <div className="flex flex-wrap gap-1 shrink-0 justify-end">
          <button
            type="button"
            onClick={copyLastReply}
            className={`text-[10px] px-2 py-1 rounded border border-gray-600 text-gray-400 hover:text-white ${focusVisibleRing}`}
            title="Copies the latest assistant bubble (reply or Error: line)."
          >
            Copy reply
          </button>
          <button
            type="button"
            onClick={copyLastDiagnostic}
            className={`text-[10px] px-2 py-1 rounded border border-gray-600 text-gray-400 hover:text-white ${focusVisibleRing}`}
            title="Copies the latest assistant text only when it starts with “Error:”."
          >
            Copy Error: line
          </button>
          <button
            type="button"
            onClick={clearThread}
            className={`text-[10px] px-2 py-1 rounded border border-gray-600 text-gray-400 hover:text-white ${focusVisibleRing}`}
            title="Clear messages and draft (local only; does not change files)"
          >
            Clear thread
          </button>
        </div>
      </div>
      <div
        className="flex-1 overflow-y-auto p-3 space-y-3 text-sm"
        role="log"
        aria-live="polite"
        aria-relevant="additions"
        aria-busy={busy}
      >
        {messages.map((msg, i) => (
          <div
            key={msg.id ?? `${msg.role}-${i}-${msg.content}`}
            className={`rounded-lg px-3 py-2 ${
              msg.role === 'user' ? 'bg-ide-accent/20 ml-4' : 'bg-gray-800/80 mr-4'
            }`}
          >
            <div className="text-[10px] uppercase text-gray-500 mb-1">{msg.role}</div>
            <pre className="whitespace-pre-wrap break-words font-sans text-gray-100">{msg.content}</pre>
          </div>
        ))}
        {busy && (
          <div
            className={`text-gray-500 text-xs ${accessibilityReducedMotionEffective ? '' : 'animate-pulse'}`}
            aria-live="polite"
          >
            Thinking…
          </div>
        )}
        <div ref={bottomRef} />
      </div>
      <div className="p-2 border-t border-gray-700 flex gap-2">
        <input
          type="text"
          value={input}
          onChange={(e) => setInput(e.target.value)}
          onKeyDown={(e) => e.key === 'Enter' && !e.shiftKey && (e.preventDefault(), send())}
          placeholder="Message…"
          disabled={busy}
          aria-label="Chat message"
          title="Draft saved locally. Routing: Settings › AI (transport + fallback + WASM path)."
          className={`flex-1 bg-ide-bg border border-gray-600 rounded px-2 py-2 text-sm min-w-0 ${focusVisibleRing}`}
        />
        <button
          type="button"
          onClick={send}
          disabled={busy || !input.trim()}
          title="Send: WASM lane first, or HTTP provider only, per Settings › AI."
          aria-label="Send chat message"
          className={`px-3 py-2 rounded bg-ide-accent hover:bg-blue-600 disabled:opacity-40 text-sm shrink-0 ${focusVisibleRing}`}
        >
          Send
        </button>
      </div>
      <div className="px-3 pb-2 border-t border-gray-800/80">
        <MinimalM08M14Footnote
          surfaceId="chat"
          offlineHint="WASM loads offline; real answers need HTTP transport or Electron provider fallback."
          docPath="docs/INFERENCE_PATH_MATRIX.md"
          m13Hint={`Also ${MINIMALISTIC_DOC} · Settings › Noise for verboseDevLogs.`}
        />
      </div>
    </div>
  );
};

export default ChatPanel;
