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
 * Cursor / Copilot–style chat: multi-turn, local WASM lane only.
 */
const CHAT_DRAFT_KEY = 'rawrxd.ide.chatDraft.v1';

const defaultThread = () => [
  {
    role: 'assistant',
    content:
      'Local WASM chat uses only module exports in this renderer and loads embedded bytes only (no HTTP, no host fallback). Pick "Ollama-compatible HTTP" in Settings > AI for main-process inference. The Agent dock handles multi-step autonomous work separately.'
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
    accessibilityReducedMotionEffective
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
    announceA11y('Chat sending. Local WASM lane.');
    noisyLog('[chat]', 'send', { temperature: settings.temperature, maxTokens: settings.maxTokens });

    const prompt = `${personaPrefix}${ctx ? ctx + '\n' : ''}User: ${text}`;

    try {
      let reply;
      const wasmResult = await invokeRawrxdWasmChat(prompt, {
        maxTokens: settings.maxTokens,
        wasmUrl: settings.wasmChatUrl
      });
      if (wasmResult.ok) {
        reply = wasmResult.content;
        playUiSound('success');
        if (shellSettings.noisyToasts) {
          pushToast({ title: 'Chat', message: 'WASM lane reply ready', variant: 'success', durationMs: 2000 });
        }
        setStatusLine(`chat: ${wasmResult.lane}`);
        noisyLog('[chat]', 'wasm reply ok', wasmResult.lane);
        announceA11y('Chat reply ready from WASM lane.');
      } else {
        reply = `Error: ${wasmResult.error}`;
        playUiSound('error');
        if (shellSettings.noisyToasts) {
          pushToast({
            title: 'Chat',
            message: `${reply} — Verify the WASM URL in Settings › AI.`,
            variant: 'error',
            durationMs: 6000
          });
        }
        setStatusLine('chat: wasm error');
        noisyLog('[chat]', 'wasm failed', wasmResult.error);
        announceA11y(`Chat WASM error. ${wasmResult.error}`, { assertive: true });
      }

      setMessages((m) => [...m, { role: 'assistant', content: reply }]);
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
      pushToast({ title: 'Chat', message: 'M03 — No assistant message to copy yet.', variant: 'warn', durationMs: 2400 });
      return;
    }
    copyTextToClipboard(line, {
      onDone: () => {
        playUiSound('success');
        setStatusLine('chat: copied last assistant reply');
      },
      onFail: () =>
        pushToast({ title: 'Chat', message: 'M03 — Clipboard failed — select text manually.', variant: 'error', durationMs: 3000 })
    });
  };

  const copyLastDiagnostic = () => {
    const line = lastAssistantText.startsWith('Error:') ? lastAssistantText : '';
    if (!line) {
      pushToast({
        title: 'Chat',
        message:
          'M03 — Last assistant message does not start with “Error:”. Use “Copy reply” for any reply, or copy from the thread.',
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
        pushToast({ title: 'Chat', message: 'M03 — Clipboard failed — select text manually.', variant: 'error', durationMs: 3000 })
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
      aria-label="AI chat: local WASM lane"
    >
      <div className="p-3 border-b border-gray-700 flex items-start justify-between gap-2 flex-wrap">
        <div className="min-w-0 flex-1">
          <h3 className="font-semibold text-sm">Chat</h3>
          <p className="text-[10px] text-gray-500 mt-0.5">
            M01 — Transport: local WASM lane only. This panel does not route through external HTTP providers.
          </p>
        </div>
        <div className="flex flex-wrap gap-1 shrink-0 justify-end">
          <button
            type="button"
            onClick={copyLastReply}
            className={`text-[10px] px-2 py-1 rounded border border-gray-600 text-gray-400 hover:text-white ${focusVisibleRing}`}
            title="M10 — Copies the latest assistant message (WASM reply or error text)."
          >
            Copy reply
          </button>
          <button
            type="button"
            onClick={copyLastDiagnostic}
            className={`text-[10px] px-2 py-1 rounded border border-gray-600 text-gray-400 hover:text-white ${focusVisibleRing}`}
            title="M10 — Only if the latest assistant message starts with “Error:”. Otherwise use Copy reply."
          >
            Copy Error: line
          </button>
          <button
            type="button"
            onClick={clearThread}
            className={`text-[10px] px-2 py-1 rounded border border-gray-600 text-gray-400 hover:text-white ${focusVisibleRing}`}
            title="Clears on-screen thread and local draft (localStorage). Does not modify files on disk."
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
          title="M02 — Draft autosaves locally. M06 — Uses local WASM lane configured in Settings › AI."
          className={`flex-1 bg-ide-bg border border-gray-600 rounded px-2 py-2 text-sm min-w-0 ${focusVisibleRing}`}
        />
        <button
          type="button"
          onClick={send}
          disabled={busy || !input.trim()}
          title="Sends your message through local WASM inference."
          aria-label="Send chat message via local WASM inference"
          className={`px-3 py-2 rounded bg-ide-accent hover:bg-blue-600 disabled:opacity-40 text-sm shrink-0 ${focusVisibleRing}`}
        >
          Send
        </button>
      </div>
      <div className="px-3 pb-2 border-t border-gray-800/80">
        <MinimalM08M14Footnote
          surfaceId="chat"
          offlineHint="WASM lane works offline after wasm loads."
          docPath="docs/INFERENCE_PATH_MATRIX.md"
          m13Hint={`Also ${MINIMALISTIC_DOC} · Settings › Noise for verboseDevLogs.`}
        />
      </div>
    </div>
  );
};

export default ChatPanel;
