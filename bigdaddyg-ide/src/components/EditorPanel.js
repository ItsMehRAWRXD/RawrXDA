import React, { useRef, useEffect, useCallback, useMemo } from 'react';
import Editor from '@monaco-editor/react';
import { useIdeFeatures } from '../contexts/IdeFeaturesContext';
import { useProject } from '../contexts/ProjectContext';
import {
  CopySupportLineButton,
  MinimalSurfaceM814Footer,
  focusVisibleRing,
  MINIMALISTIC_DOC,
  workspaceRelativePath
} from '../utils/minimalisticM08M14';
import { extractSymbolsFromText } from '../utils/textSymbolIndex';

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
  const { saveActiveFile } = useProject();
  const { settings, modules, playUiSound, pushToast, noisyLog, noisyLogVerbose, setStatusLine } = useIdeFeatures();

  const runSaveRef = useRef(async () => ({ success: false }));

  const runSave = useCallback(async () => {
    const live = editorRef.current != null ? editorRef.current.getValue() : undefined;
    const r = await saveActiveFile(live);
    if (r.success) {
      playUiSound('tick');
      pushToast({
        title: 'Saved',
        message: activeFile?.name || 'File',
        variant: 'success',
        durationMs: 2200
      });
      setStatusLine(`[editor] saved ${activeFile?.name || ''}`);
      noisyLog('[editor] save ok', activeFile?.path);
    } else {
      playUiSound('warn');
      pushToast({
        title: 'Save',
        message: r.error || 'Save failed',
        variant: 'error',
        durationMs: 5200
      });
      setStatusLine('[editor] save failed');
      noisyLog('[editor] save failed', r.error);
    }
    return r;
  }, [saveActiveFile, activeFile?.name, activeFile?.path, playUiSound, pushToast, setStatusLine, noisyLog]);

  useEffect(() => {
    runSaveRef.current = runSave;
  }, [runSave]);

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
        noisyLog('[editor]', 'inline completions registered', INLINE_LANGS.length);
        if (settings.noiseIntensity === 'maximum') {
          playUiSound('tick');
          pushToast({
            title: 'Editor',
            message: 'Buffer-local inline hints enabled (regex on open file).',
            variant: 'success',
            durationMs: 2200
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
    try {
      editor.addCommand(monaco.KeyMod.CtrlCmd | monaco.KeyCode.KeyS, () => {
        void runSaveRef.current();
      });
    } catch {
      /* ignore */
    }
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
        title="Open a project file from the sidebar. Ctrl+S saves through Electron."
      >
        <div className="text-center text-gray-400 max-w-md">
          <h2 className="text-xl font-semibold mb-2 text-white">No file open</h2>
          <p className="text-sm mb-2">Open a project folder, then pick a file in the sidebar.</p>
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
            offlineHint="Works offline. Project files load after you open a folder."
            docPath={MINIMALISTIC_DOC}
            m13Hint="Dev: noisyLogVerbose('editor', …)"
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
          <div className="flex items-center gap-2 min-w-0 flex-wrap">
            <span
              className="text-sm font-medium text-white truncate"
              title={`${displayPath || activeFile.path} — Save writes this buffer (Ctrl+S).`}
            >
              {activeFile.name}
            </span>
            <span className="text-xs text-gray-400 bg-ide-sidebar px-2 py-1 rounded shrink-0">{language}</span>
            <button
              type="button"
              onClick={() => void runSave()}
              className={`text-xs px-2 py-1 rounded border border-cyan-700/50 text-cyan-200 hover:bg-cyan-950/40 shrink-0 ${focusVisibleRing}`}
              title="Save to disk (Ctrl+S)"
            >
              Save
            </button>
            {settings.copilotInlineHints && modules.inlineCompletion && (
              <span className="text-[10px] uppercase tracking-wider text-cyan-400/90 animate-pulse shrink-0">
                inline on
              </span>
            )}
          </div>
          {displayPath && displayPath !== activeFile.name ? (
            <span className="text-[10px] text-gray-500 truncate font-mono" title="Relative to project root">
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
            <div className="flex flex-col items-center justify-center h-full text-white gap-2 px-4 text-center">
              <span className="animate-pulse">Loading editor…</span>
              <span className="text-xs text-gray-500">Monaco is loading.</span>
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
        offlineHint="Save persists changes. Inline hints: enable in Settings › Copilot and Modules › Inline completion."
        docPath={MINIMALISTIC_DOC}
        m13Hint="Dev: noisyLogVerbose('editor', …)"
      />
    </div>
  );
};

export default EditorPanel;
