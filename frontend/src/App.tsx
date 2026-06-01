/**
 * App.tsx
 * Main application entry point.
 * Hosts the StatusDashboard and provides the IDE layout skeleton.
 */

import { StatusDashboard } from './components/StatusDashboard';
import { InferencePanel } from './components/InferencePanel';
import { ModelSelector } from './components/ModelSelector';
import { SafetyAuditPanel } from './components/SafetyAuditPanel';
import { AgentPanel } from './components/AgentPanel';
import { HitlAuditPanel } from './components/HitlAuditPanel';
import { ResilienceProvider } from './components/ResilienceProvider';
import './App.css';

function App() {
  return (
    <ResilienceProvider>
      <div className="app-layout">
        <header className="app-header">
          <h1>Sovereign Inference IDE v1.1-alpha</h1>
        </header>

        <div className="app-body">
          <aside className="app-sidebar">
            <nav className="sidebar-nav">
              <h3>Engine Status</h3>
              <StatusDashboard />
              <h3>Model</h3>
              <ModelSelector />
              <h3>Safety</h3>
              <SafetyAuditPanel />
              <h3>Agent</h3>
              <AgentPanel />
              <h3>Audit</h3>
              <HitlAuditPanel />
            </nav>
          </aside>

          <main className="app-main">
            <div className="placeholder">
              <h2>Control Plane + Data Plane Active</h2>
              <p>Control plane: status polling at 1Hz from the sovereign engine authority.</p>
              <p>Data plane: live token streaming from /inference/stream, gated by engine READY state.</p>
              <p>
                <strong>Engine Endpoints:</strong>{' '}
                <code>http://localhost:11435/status</code>, <code>http://localhost:11435/inference/stream</code>, and{' '}
                <code>http://localhost:11435/fault</code>
              </p>
            </div>
            <InferencePanel />
          </main>
        </div>
      </div>
    </ResilienceProvider>
  );
}

export default App;
