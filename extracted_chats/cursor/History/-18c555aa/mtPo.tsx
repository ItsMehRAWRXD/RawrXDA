// BigDaddyGEngine/ui/editor/SyntaxHighlighting.tsx
// CodeMirror-based Syntax Highlighting for Code Editor

import { useEffect, useRef } from 'react';

export interface SyntaxHighlighterProps {
  content: string;
  language: string;
  onChange: (content: string) => void;
  className?: string;
}

// Lightweight syntax highlighting without external dependencies
export function SyntaxHighlighter({ 
  content, 
  language, 
  onChange,
  className = ''
}: SyntaxHighlighterProps) {
  const editorRef = useRef<HTMLTextAreaElement>(null);
  const highlightedRef = useRef<HTMLPreElement>(null);
  const langRef = useRef<string>(language);

  useEffect(() => {
    langRef.current = language;
    if (highlightedRef.current) {
      highlightCode(content, language);
    }
  }, [content, language]);

  const highlightCode = (text: string, lang: string) => {
    if (!highlightedRef.current) return;

    const keywords = getKeywords(lang);
    let highlighted = escapeHtml(text);

    // Highlight keywords
    keywords.forEach(keyword => {
      const regex = new RegExp(`\\b${keyword}\\b`, 'g');
      highlighted = highlighted.replace(regex, `<span class="keyword">${keyword}</span>`);
    });

    // Highlight strings
    highlighted = highlighted.replace(/"([^"]*)"/g, '<span class="string">"$1"</span>');
    highlighted = highlighted.replace(/'([^']*)'/g, "<span class='string'>'$1'</span>");
    highlighted = highlighted.replace(/`([^`]*)`/g, '<span class="string">`$1`</span>');

    // Highlight numbers
    highlighted = highlighted.replace(/\b(\d+\.?\d*)\b/g, '<span class="number">$1</span>');

    // Highlight comments
    highlighted = highlighted.replace(/\/\/.*$/gm, '<span class="comment">$&</span>');
    highlighted = highlighted.replace(/\/\*[\s\S]*?\*\//g, '<span class="comment">$&</span>');

    highlightedRef.current.innerHTML = highlighted;
  };

  const handleScroll = () => {
    if (editorRef.current && highlightedRef.current) {
      highlightedRef.current.scrollTop = editorRef.current.scrollTop;
      highlightedRef.current.scrollLeft = editorRef.current.scrollLeft;
    }
  };

  const escapeHtml = (text: string) => {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
  };

  const getKeywords = (lang: string): string[] => {
    const keywordMap: Record<string, string[]> = {
      'typescript': [
        'const', 'let', 'var', 'function', 'return', 'if', 'else', 'for', 'while',
        'class', 'interface', 'type', 'async', 'await', 'import', 'export',
        'try', 'catch', 'finally', 'throw', 'new', 'this', 'super', 'extends'
      ],
      'javascript': [
        'const', 'let', 'var', 'function', 'return', 'if', 'else', 'for', 'while',
        'class', 'async', 'await', 'import', 'export', 'try', 'catch', 'finally'
      ],
      'python': [
        'def', 'class', 'if', 'elif', 'else', 'for', 'while', 'try', 'except',
        'import', 'from', 'return', 'yield', 'async', 'await', 'with'
      ],
      'java': [
        'public', 'private', 'protected', 'class', 'interface', 'extends', 'implements',
        'if', 'else', 'for', 'while', 'return', 'import', 'package', 'new'
      ],
      'html': ['html', 'head', 'body', 'div', 'span', 'script', 'style'],
      'css': ['@media', '@keyframes', 'font-family', 'color', 'background', 'margin', 'padding']
    };
    return keywordMap[lang] || [];
  };

  return (
    <div className={`syntax-highlighter ${className}`}>
      <pre 
        ref={highlightedRef} 
        className="highlighted-code"
        aria-hidden="true"
      />
      <textarea
        ref={editorRef}
        value={content}
        onChange={(e) => onChange(e.target.value)}
        onScroll={handleScroll}
        spellCheck={false}
        className="code-input"
        style={{
          position: 'absolute',
          top: 0,
          left: 0,
          right: 0,
          bottom: 0,
          background: 'transparent',
          color: 'transparent',
          caretColor: '#fff',
          resize: 'none',
          fontFamily: 'monospace',
          fontSize: '14px',
          padding: '10px',
          border: 'none',
          outline: 'none',
          tabSize: 2
        }}
      />
    </div>
  );
}

// Add styles for syntax highlighting
export const syntaxHighlightStyles = `
.syntax-highlighter {
  position: relative;
}

.highlighted-code {
  margin: 0;
  padding: 10px;
  font-family: monospace;
  font-size: 14px;
  white-space: pre;
  overflow: auto;
  border: none;
  background: #1e1e1e;
  color: #d4d4d4;
  pointer-events: none;
  tab-size: 2;
}

.code-input {
  text-shadow: 0 0 0 transparent;
}

.keyword {
  color: #569cd6;
  font-weight: 600;
}

.string {
  color: #ce9178;
}

.number {
  color: #b5cea8;
}

.comment {
  color: #6a9955;
  font-style: italic;
}
`;
