import React, { useEffect, useRef } from 'react';
import Editor, { OnMount } from '@monaco-editor/react';
import * as monaco from 'monaco-editor';

type CodeEditorProps = {
  value: string;
  onChange: (value: string) => void;
  language?: string;
};

export const CodeEditor: React.FC<CodeEditorProps> = ({ value, onChange, language = 'cpp' }) => {
  const providerRef = useRef<monaco.IDisposable | null>(null);
  const abortControllerRef = useRef<AbortController | null>(null);

  const handleMount: OnMount = (_editor, monacoInstance) => {
    if (providerRef.current) {
      providerRef.current.dispose();
    }

    providerRef.current = monacoInstance.languages.registerInlineCompletionsProvider(language, {
      provideInlineCompletions: async (model: monaco.editor.ITextModel, position: monaco.Position, _context: monaco.languages.InlineCompletionContext, _token: monaco.CancellationToken) => {
        try {
          // Cancel any in-flight stream before starting a new one
          if (abortControllerRef.current) {
            abortControllerRef.current.abort();
          }
          abortControllerRef.current = new AbortController();

          const buffer = model.getValue();
          const offset = model.getOffsetAt(position);
          let fullCompletion = '';

          // Slice context window: last 2-4k chars before cursor
          const context_window = 4096;
          const context_start = offset > context_window ? offset - context_window : 0;
          const context = buffer.substring(context_start, offset);

          const response = await fetch('/complete/stream', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
              buffer: context,
              cursor_offset: context.length,
              language,
              mode: 'complete',
              max_tokens: 128,
              temperature: 0.2
            }),
            signal: abortControllerRef.current.signal
          });

          if (!response.ok || !response.body) {
            return { items: [], dispose: () => { } };
          }

          const reader = response.body.getReader();
          const decoder = new TextDecoder();
          let buffer_text = '';

          while (true) {
            // Check if stream was aborted
            if (abortControllerRef.current?.signal.aborted) {
              reader.cancel();
              break;
            }

            const { done, value } = await reader.read();
            if (done) break;

            buffer_text += decoder.decode(value, { stream: true });
            const lines = buffer_text.split('\n');
            buffer_text = lines.pop() || '';

            for (const line of lines) {
              if (line.startsWith('data: ')) {
                try {
                  const event = JSON.parse(line.substring(6));
                  if (event.token) {
                    fullCompletion += event.token;
                  }
                } catch (e) {
                  // Ignore parse errors
                }
              }
            }

            // Tokens accumulate in fullCompletion; final result returned below
          }

          if (!fullCompletion) {
            return { items: [], dispose: () => { } };
          }

          return {
            items: [
              {
                insertText: fullCompletion,
                range: new monacoInstance.Range(
                  position.lineNumber,
                  position.column,
                  position.lineNumber,
                  position.column
                )
              }
            ],
            dispose: () => { }
          };
        } catch (err) {
          if (err instanceof Error && err.name === 'AbortError') {
            // Stream was cancelled by new keystroke - this is expected
            return { items: [], dispose: () => { } };
          }
          console.error('Completion stream error:', err);
          return { items: [], dispose: () => { } };
        }
      },
      freeInlineCompletions: () => { }
    });
  };

  useEffect(() => {
    return () => {
      // Clean up completion provider on unmount
      if (providerRef.current) {
        providerRef.current.dispose();
        providerRef.current = null;
      }
      // Cancel any in-flight streaming completion request
      if (abortControllerRef.current) {
        abortControllerRef.current.abort();
        abortControllerRef.current = null;
      }
    };
  }, []);

  return (
    <Editor
      height="100%"
      defaultLanguage={language}
      value={value}
      onChange={(val) => onChange(val || '')}
      onMount={handleMount}
      theme="vs-dark"
      options={{
        minimap: { enabled: true },
        fontSize: 14,
        scrollBeyondLastLine: false,
        automaticLayout: true,
        inlineSuggest: { enabled: true }
      }}
    />
  );
};
