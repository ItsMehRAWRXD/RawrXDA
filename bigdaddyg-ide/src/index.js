/**
 * Renderer entry: mounts the IDE shell only.
 * Does not load model weights, run inference, or read providers.json (main process owns that).
 *
 * Manual verify: launch window → toolbar visible → Ctrl+Shift+P opens command palette → run a listed command.
 */
import React from 'react';
import ReactDOM from 'react-dom/client';
import './styles/tailwind.css';
import App from './App';

const root = ReactDOM.createRoot(document.getElementById('root'));
root.render(
  <React.StrictMode>
    <App />
  </React.StrictMode>
);
