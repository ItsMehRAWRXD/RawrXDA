import React from 'react';

/**
 * Agentic inference strip: chat transport and toolbar provider (HTTP transport only uses the provider).
 */
const LocalInferenceStatusStrip = ({ activeProviderId, settings }) => {
  const provider = activeProviderId || '—';
  const http = settings?.chatTransport === 'ollama-http';
  const chatLane = http
    ? 'Chat: HTTP provider (toolbar / config/providers.json)'
    : 'Chat: WASM only — no host API delegation from this lane';
  const aria = `${chatLane}. Toolbar provider ${provider}.`;

  return (
    <div
      className="flex flex-wrap items-center gap-x-3 gap-y-1 text-[10px] text-gray-500 border-t border-gray-800/80 px-3 py-1.5 bg-black/25"
      role="region"
      aria-label={aria}
    >
      <span className="text-cyan-700/90 font-medium shrink-0" aria-hidden="true">
        Agentic AI
      </span>
      <span className="font-mono text-gray-400" title="Chat routing from Settings › AI">
        {chatLane}
      </span>
      <span className="text-gray-600 hidden sm:inline" aria-hidden="true">
        |
      </span>
      <span className="font-mono text-gray-400">toolbar provider: {provider}</span>
      {http ? (
        <span className="ml-auto text-[10px] text-gray-500 hidden lg:inline">
          Chat IPC uses active provider above
        </span>
      ) : (
        <span className="ml-auto text-[10px] text-gray-500 hidden lg:inline">
          Switch transport to Ollama HTTP to use the toolbar provider in Chat
        </span>
      )}
    </div>
  );
};

export default LocalInferenceStatusStrip;
