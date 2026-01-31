// AiResponseRenderer.tsx - Dynamic router for rich AI-generated UI components
import React from 'react';
import ReactMarkdown from 'react-markdown';
import { Prism as SyntaxHighlighter } from 'react-syntax-highlighter';
import { dracula } from 'react-syntax-highlighter/dist/esm/styles/prism';
import DiffViewer from './DiffViewer';
import FileBrowser from './FileBrowser';
import ProgressBar from './ProgressBar';
import ActionButtons from './ActionButtons';
import CodeBlock from './CodeBlock';
import TableView from './TableView';

// Mapping of component names to React components
const componentMap = {
    'diff-viewer': DiffViewer,
    'file-browser': FileBrowser,
    'progress-bar': ProgressBar,
    'action-buttons': ActionButtons,
    'code-block': CodeBlock,
    'table-view': TableView,
};

interface AiResponse {
    type?: 'rich_component' | 'markdown' | 'text';
    name?: string;
    props?: any;
    text?: string;
    components?: AiResponse[];
}

const AiResponseRenderer: React.FC<{ response: AiResponse }> = ({ response }) => {
    // Handle rich component rendering
    if (response.type === 'rich_component' && response.name && componentMap[response.name]) {
        const Component = componentMap[response.name];
        return (
            <div className="ai-rich-component">
                <Component {...response.props} />
            </div>
        );
    }

    // Handle multiple components in sequence
    if (response.components && response.components.length > 0) {
        return (
            <div className="ai-multi-component">
                {response.components.map((component, index) => (
                    <div key={index} className="component-item">
                        <AiResponseRenderer response={component} />
                    </div>
                ))}
            </div>
        );
    }

    // Handle markdown with enhanced components
    if (response.text) {
        return (
            <div className="ai-markdown-content">
                <ReactMarkdown
                    children={response.text}
                    components={{
                        code({ node, inline, className, children, ...props }) {
                            const match = /language-(\w+)/.exec(className || '');
                            const language = match ? match[1] : '';
                            
                            if (!inline && language) {
                                return (
                                    <CodeBlock 
                                        language={language}
                                        code={String(children).replace(/\n$/, '')}
                                    />
                                );
                            } else {
                                return (
                                    <code className={className} {...props}>
                                        {children}
                                    </code>
                                );
                            }
                        },
                        
                        table({ children }) {
                            return (
                                <TableView>
                                    {children}
                                </TableView>
                            );
                        },
                        
                        blockquote({ children }) {
                            return (
                                <blockquote className="ai-blockquote">
                                    {children}
                                </blockquote>
                            );
                        },
                        
                        ul({ children }) {
                            return (
                                <ul className="ai-list">
                                    {children}
                                </ul>
                            );
                        },
                        
                        ol({ children }) {
                            return (
                                <ol className="ai-ordered-list">
                                    {children}
                                </ol>
                            );
                        },
                        
                        li({ children }) {
                            return (
                                <li className="ai-list-item">
                                    {children}
                                </li>
                            );
                        },
                        
                        a({ href, children }) {
                            return (
                                <a 
                                    href={href} 
                                    target="_blank" 
                                    rel="noopener noreferrer"
                                    className="ai-link"
                                    onClick={(e) => {
                                        // Handle internal links differently
                                        if (href && href.startsWith('vscode://')) {
                                            e.preventDefault();
                                            window.vscode.postMessage({
                                                command: 'openFile',
                                                path: href.replace('vscode://', '')
                                            });
                                        }
                                    }}
                                >
                                    {children}
                                </a>
                            );
                        },
                        
                        h1({ children }) {
                            return <h1 className="ai-heading ai-heading-1">{children}</h1>;
                        },
                        
                        h2({ children }) {
                            return <h2 className="ai-heading ai-heading-2">{children}</h2>;
                        },
                        
                        h3({ children }) {
                            return <h3 className="ai-heading ai-heading-3">{children}</h3>;
                        },
                        
                        p({ children }) {
                            return <p className="ai-paragraph">{children}</p>;
                        },
                        
                        strong({ children }) {
                            return <strong className="ai-strong">{children}</strong>;
                        },
                        
                        em({ children }) {
                            return <em className="ai-emphasis">{children}</em>;
                        }
                    }}
                />
            </div>
        );
    }

    // Fallback for unknown response types
    return (
        <div className="ai-unknown-response">
            <pre>{JSON.stringify(response, null, 2)}</pre>
        </div>
    );
};

export default AiResponseRenderer;
