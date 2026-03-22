import React from 'react';

/**
 * Agentic inference strip: Chat = embedded WASM; toolbar provider = main `ai:invoke`;
 * Win32 RXIP via `native:status`; Ollama = optional probe (Settings URL).
 */
const LocalInferenceStatusStrip = ({ activeProviderId, settings }) => {
  const provider = activeProviderId || '—';
  const httpOnly = settings?.chatTransport === 'ollama-http';
  const fallbackOn = settings?.chatAgenticProviderFallback !== false;
  const chatLane = httpOnly
    ? 'Chat: HTTP provider transport (main ai invoke)'
    : `Chat: WASM first · fallback ${fallbackOn ? 'on' : 'off'} (Electron → toolbar provider)`;
  const baseUrl = String(settings?.ollamaProbeBaseUrl || 'http://127.0.0.1:11434').trim() || 'http://127.0.0.1:11434';
  const [probe, setProbe] = React.useState(null);
  const [native, setNative] = React.useState(null);

  React.useEffect(() => {
    const api = typeof window !== 'undefined' ? window.electronAPI : null;
    if (!api?.probeOllama) {
      setProbe({ skipped: true });
      return undefined;
    }
    let cancelled = false;
    (async () => {
      const r = await api.probeOllama(baseUrl);
      if (!cancelled) setProbe(r);
    })();
    return () => {
      cancelled = true;
    };
  }, [baseUrl]);

  React.useEffect(() => {
    const api = typeof window !== 'undefined' ? window.electronAPI : null;
    if (!api?.nativeStatus) {
      setNative({ skipped: true });
      return undefined;
    }
    let cancelled = false;
    const tick = async () => {
      try {
        const r = await api.nativeStatus();
        if (cancelled) return;
        if (r?.success && r.data) setNative({ ok: true, data: r.data });
        else setNative({ ok: false, error: r?.error || 'native:status failed' });
      } catch (e) {
        if (!cancelled) setNative({ ok: false, error: e?.message || String(e) });
      }
    };
    tick();
    const id = setInterval(tick, 8000);
    return () => {
      cancelled = true;
      clearInterval(id);
    };
  }, []);

  const ollamaLine = (() => {
    if (!probe) return 'Ollama: probing…';
    if (probe.skipped) return 'Ollama probe: n/a (web preview / no preload)';
    if (probe.success && probe.data?.ok) {
      return `Ollama: ${probe.data.modelCount} model(s) · ${probe.data.latencyMs}ms · ${probe.data.baseUrl || baseUrl}`;
    }
    const err = typeof probe.error === 'string' && probe.error ? probe.error : 'unreachable';
    return `Ollama: ${err} (${baseUrl})`;
  })();

  const rxipLine = (() => {
    if (!native) return 'Win32 bridge: …';
    if (native.skipped) return 'Win32 bridge: n/a (no nativeStatus)';
    if (native.ok && native.data) {
      const d = native.data;
      if (d.ready) {
        const pid = d.nativePid != null ? ` · pid ${d.nativePid}` : '';
        return `RXIP: connected${pid}`;
      }
      const err = d.error ? ` — ${d.error}` : '';
      return `RXIP: ${d.state || 'unknown'}${err}`;
    }
    return `Win32 bridge: ${native.error || 'unknown'}`;
  })();

  const aria = `${chatLane}. Agent provider ${provider}. ${rxipLine}. ${ollamaLine}.`;

  return (
    <div
      className="flex flex-wrap items-center gap-x-3 gap-y-1 text-[10px] text-gray-500 border-t border-gray-800/80 px-3 py-1.5 bg-black/25"
      role="region"
      aria-label={aria}
    >
      <span className="text-cyan-700/90 font-medium shrink-0" aria-hidden="true">
        Agentic AI
      </span>
      <span className="font-mono text-gray-400" title="Dock Chat uses rawrxdWasmLoader embedded bytes">
        {chatLane}
      </span>
      <span className="text-gray-600 hidden sm:inline" aria-hidden="true">
        |
      </span>
      <span className="font-mono text-gray-400">Agent IPC provider: {provider}</span>
      <span className="text-gray-600 hidden md:inline" aria-hidden="true">
        |
      </span>
      <span
        className="font-mono text-gray-500 max-w-[min(40vw,20rem)] truncate"
        title={`Main-process native:status → Win32 bridge (${rxipLine})`}
      >
        {rxipLine}
      </span>
      <span className="text-gray-600 hidden md:inline" aria-hidden="true">
        |
      </span>
      <span className="font-mono text-gray-500 max-w-[min(52vw,28rem)] truncate" title={ollamaLine}>
        {ollamaLine}
      </span>
      <span className="ml-auto text-[10px] text-gray-500 hidden lg:inline">
        Agent uses IPC · Chat per transport above
      </span>
    </div>
  );
};

export default LocalInferenceStatusStrip;
