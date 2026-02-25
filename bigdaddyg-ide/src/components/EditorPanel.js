import React, { useRef, useState } from 'react';
import Editor from '@monaco-editor/react';

const EditorPanel = ({ activeFile, onUpdateFile }) => {
  const editorRef = useRef(null);

  const handleEditorDidMount = (editor) => {
    editorRef.current = editor;
    if (editor) {
      editor.updateOptions({
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
      });
    }
  };

  const handleChange = (value) => {
    if (activeFile && value !== undefined && onUpdateFile) {
      onUpdateFile(activeFile.path, value);
    }
  };

  if (!activeFile) {
    return (
      <div className="flex-1 flex items-center justify-center bg-ide-bg">
        <div className="text-center text-gray-400">
          <h2 className="text-xl font-semibold mb-2 text-white">No File Open</h2>
          <p>Open a project or select a file from the sidebar to start editing</p>
        </div>
      </div>
    );
  }

  const language = activeFile.language || 'plaintext';

  return (
    <div className="flex-1 flex flex-col min-h-0">
      <div className="flex items-center justify-between px-4 py-2 bg-ide-toolbar border-b border-gray-700">
        <div className="flex items-center gap-2">
          <span className="text-sm font-medium text-white">{activeFile.name}</span>
          <span className="text-xs text-gray-400 bg-ide-sidebar px-2 py-1 rounded">
            {language}
          </span>
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
          loading={<div className="flex items-center justify-center h-full text-white">Loading editor...</div>}
          options={{
            automaticLayout: true,
            fontLigatures: true,
            formatOnPaste: true,
            formatOnType: true,
            smoothScrolling: true,
            cursorSmoothCaretAnimation: 'on'
          }}
        />
      </div>
    </div>
  );
};

export default EditorPanel;
