import { useState, useEffect } from 'react';

// App component fetches /api/hello and /api/status on mount
export default function App() {
    const [hello,  setHello]  = useState(null);
    const [status, setStatus] = useState(null);
    const [error,  setError]  = useState(null);

    useEffect(() => {
        Promise.all([
            fetch('/api/hello').then(r => r.json()),
            fetch('/api/status').then(r => r.json()),
        ])
        .then(([h, s]) => { setHello(h); setStatus(s); })
        .catch(e => setError(e.message));
    }, []);

    if (error)  return <div className="card err">Error: {error}</div>;
    if (!hello) return <div className="card loading">Loading…</div>;

    return (
        <div className="card">
            <h1>RawrXD Agentic Core</h1>
            <span className="badge">LIVE</span>
            <p className="msg">"{hello.message}"</p>
            <div className="meta">
                <div>engine: {status?.engine}</div>
                <div>version: {status?.version}</div>
                <div>uptime: {((status?.uptime_ms ?? 0) / 1000).toFixed(1)}s</div>
            </div>
        </div>
    );
}
