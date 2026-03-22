import React, { useRef, useEffect, useCallback, useMemo } from 'react';
import Editor from '@monaco-editor/react';
import { useIdeFeatures } from '../contexts/IdeFeaturesContext';
import {
  CopySupportLineButton,
  MinimalSurfaceM814Footer,
  focusVisibleRing,
  MINIMALISTIC_DOC,
  workspaceRelativePath
} from '../utils/minimalisticM08M14';
import { extractSymbolsFromText } from '../utils/textSymbolIndex';

/** Pick a file symbol at or before the cursor line for inline hints (textSymbolIndex; not LSP). */
function pickInlineIdentifierHint(text, filePath, monacoLanguageId, lineNumber) {
  const syms = extractSymbolsFromText(text || '', filePath || '', monacoLanguageId || '');
  if (!syms.length) return null;
  const before = syms.filter((s) => s.line <= lineNumber);
  return before.length ? before[before.length - 1] : syms[0];
}

const INLINE_LANGS = [
  'javascript',
  'typescript',
  'javascriptreact',
  'typescriptreact',
  'python',
  'cpp',
  'c',
  'csharp',
  'go',
  'rust',
  'java',
  'plaintext',
  'json',
  'markdown',
  'yaml',
  'shell',
  'powershell'
];

const EditorPanel = ({ activeFile, projectRoot, onUpdateFile }) => {
  const editorRef = useRef(null);
  const monacoRef = useRef(null);
  const inlineDisposableRef = useRef(null);
  const { settings, modules, playUiSound, pushToast, noisyLog, noisyLogVerbose, setStatusLine } = useIdeFeatures();

  const displayPath = useMemo(
    () => (activeFile?.path ? workspaceRelativePath(activeFile.path, projectRoot) : ''),
    [activeFile?.path, projectRoot]
  );

  const registerInlineCompletions = useCallback(
    (monaco) => {
      inlineDisposableRef.current?.dispose();
      inlineDisposableRef.current = null;
      if (!monaco || !settings.copilotInlineHints || !modules.inlineCompletion) return;

      const provider = {
        provideInlineCompletions: (model, position) => {
          const word = model.getWordUntilPosition(position);
          const range = {
            startLineNumber: position.lineNumber,
            endLineNumber: position.lineNumber,
            startColumn: word.startColumn,
            endColumn: word.endColumn
          };
          const lineText = model.getLineContent(position.lineNumber) || '';
          const noisy = settings.noiseIntensity === 'maximum';
          const items = [];
          const uriPath = model.uri
            ? String(model.uri.path || model.uri.fsPath || model.uri.toString?.() || '')
            : '';
          const ident = pickInlineIdentifierHint(
            model.getValue(),
            uriPath,
            model.getLanguageId?.() || '',
            position.lineNumber
          );
          if (ident?.name) {
            items.push({
              insertText: `// ${ident.name} (${ident.kind}, L${ident.line})\n`,
              range,
              filterText: word.word
            });
          }

          if (lineText.trim().startsWith('//') || lineText.trim().startsWith('#')) {
            items.push({
              insertText: noisy ? ' RAWRXD-NOISY: refactor into small functions' : ' summarize behavior in one line',
              range,
              filterText: word.word
            });
          }
          items.push({
            insertText: '// Logger::info("trace");\n',
            range,
            filterText: word.word
          });
          items.push({
            insertText: '/* inline hint: extract helper */\n',
            range,
            filterText: word.word
          });
          if (noisy) {
            items.push({
              insertText: 'console.assert(true, "RAWRXD noisy mode");\n',
              range,
              filterText: word.word
            });
          }

          noisyLog('[editor]', 'inline completions offered', model.uri?.toString(), items.length);
          return { items };
        },
        disposeInlineCompletions: () => {}
      };

      try {
        inlineDisposableRef.current = monaco.languages.registerInlineCompletionsProvider(INLINE_LANGS, provider);
        noisyLog('[editor]', 'inline completions provider registered', INLINE_LANGS.length, 'langs');
        if (settings.noiseIntensity === 'maximum') {
          playUiSound('tick');
          pushToast({
            title: 'Editor',
            message:
              'Buffer-local inline hints on — regex/identifier picks from the open file, not a hosted Copilot service.',
            variant: 'success',
            durationMs: 2600
          });
        }
      } catch (e) {
        noisyLog('inline provider failed', e?.message);
      }
    },
    [settings.copilotInlineHints, settings.noiseIntensity, modules.inlineCompletion, pushToast, playUiSound, noisyLog]
  );

  useEffect(() => {
    return () => {
      inlineDisposableRef.current?.dispose();
      inlineDisposableRef.current = null;
    };
  }, []);

  useEffect(() => {
    if (activeFile?.path) {
      setStatusLine(`Editor: ${activeFile.name}`);
      noisyLog('[editor] active file', activeFile.path);
      noisyLogVerbose('editor', 'relative', displayPath);
    }
  }, [activeFile?.path, activeFile?.name, displayPath, setStatusLine, noisyLog, noisyLogVerbose]);

  useEffect(() => {
    if (monacoRef.current) {
      registerInlineCompletions(monacoRef.current);
    }
  }, [
    registerInlineCompletions,
    activeFile?.path,
    activeFile?.language,
    settings.copilotInlineHints,
    modules.inlineCompletion
  ]);

  const handleEditorDidMount = (editor, monaco) => {
    editorRef.current = editor;
    monacoRef.current = monaco;
    if (editor) {
      const opts = {
        scrollBeyondLastLine: false,
        minimap: { enabled: true },
        fontSize: 14,
        lineHeight: 1.4,
        renderLineHighlight: 'all',
        renderWhitespace: 'boundary',
        largeFileOptimizations: true,
        stopRenderingLineAfter: 10000,
        maxTokenizationLineLength: 20000,
        renderControlCharacters: false
      };
      if (settings.noiseIntensity === 'maximum') {
        opts.quickSuggestions = { other: true, comments: true, strings: true };
      }
      editor.updateOptions(opts);
    }
    registerInlineCompletions(monaco);
  };

  const handleChange = (value) => {
    if (activeFile && value !== undefined && onUpdateFile) {
      onUpdateFile(activeFile.path, value);
    }
  };

  if (!activeFile) {
    return (
      <div
        className="flex-1 flex items-center justify-center bg-ide-bg px-6"
        role="region"
        aria-label="Editor — no file open"
        title="M03 — Pick a file from the sidebar. Buffer stays in memory until Save IPC exists."
      >
        <div className="text-center text-gray-400 max-w-md">
          <h2 className="text-xl font-semibold mb-2 text-white">No file open</h2>
          <p className="text-sm mb-2">M03 — Open a project, then select a file in the sidebar.</p>
          <CopySupportLineButton
            getText={() => '[editor] no_file_open=true'}
            label="Copy support line"
            className={`mt-3 text-[10px] px-2 py-1 rounded border border-gray-600 text-gray-300 hover:bg-gray-800 ${focusVisibleRing}`}
            onCopied={() => {
              pushToast({ title: '[editor]', message: 'Support line copied.', variant: 'success', durationMs: 2000 });
              setStatusLine('[editor] support line copied');
            }}
            onFailed={() =>
              pushToast({ title: '[editor]', message: 'Clipboard unavailable.', variant: 'warn', durationMs: 2600 })
            }
          />
          <MinimalSurfaceM814Footer
            surfaceId="editor-empty"
            offlineHint="Empty state works offline; opening files needs Electron project IPC."
            docPath={MINIMALISTIC_DOC}
            m13Hint="Dev: noisyLogVerbose('editor', …) when verbose or MAX noise."
          />
        </div>
      </div>
    );
  }

  const language = activeFile.language || 'plaintext';

  return (
    <div className="flex-1 flex flex-col min-h-0">
      <div className="flex items-stretch px-4 py-2 bg-ide-toolbar border-b border-gray-700">
        <div className="flex flex-col gap-1 min-w-0 flex-1">
          <div className="flex items-center gap-2 min-w-0">
            <span
              className="text-sm font-medium text-white truncate"
              title={`M01 — ${displayPath || activeFile.path}\nDoes not auto-save to disk until Save is wired.`}
            >
              {activeFile.name}
            </span>
            <span className="text-xs text-gray-400 bg-ide-sidebar px-2 py-1 rounded shrink-0">{language}</span>
            {settings.copilotInlineHints && modules.inlineCompletion && (
              <span className="text-[10px] uppercase tracking-wider text-cyan-400/90 animate-pulse shrink-0">
                inline on
              </span>
            )}
          </div>
          {displayPath && displayPath !== activeFile.name ? (
            <span
              className="text-[10px] text-gray-500 truncate font-mono"
              title="M14 — workspace-relative path for display"
            >
              {displayPath}
            </span>
          ) : null}
        </div>
      </div>
      <div className="flex-1 min-h-0">
        <Editor
          height="100%"
          language={language}
          value={activeFile.content ?? ''}
          theme="vs-dark"
          onMount={handleEditorDidMount}
          onChange={handleChange}
          loading={
            <div
              className="flex flex-col items-center justify-center h-full text-white gap-2 px-4 text-center"
              title="M03 — If this hangs, reload the window; Monaco loads from the CDN bundle."
            >
              <span className="animate-pulse">Loading editor…</span>
              <span className="text-xs text-gray-500">Stuck? Reload the window (Ctrl+R).</span>
            </div>
          }
          options={{
            automaticLayout: true,
            fontLigatures: true,
            formatOnPaste: true,
            formatOnType: true,
            smoothScrolling: true,
            cursorSmoothCaretAnimation: 'on',
            inlineSuggest: { enabled: true }
          }}
        />
      </div>
      <MinimalSurfaceM814Footer
        surfaceId="editor"
        offlineHint="Buffer edits work offline; LSP/remote features degrade if backends are down."
        docPath={MINIMALISTIC_DOC}
        m13Hint="Dev: noisyLogVerbose('editor', …) on active file changes."
      />
    </div>
  );
};

export default EditorPanel;
