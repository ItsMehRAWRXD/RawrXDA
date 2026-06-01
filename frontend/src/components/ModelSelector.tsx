/**
 * ModelSelector.tsx
 * Day 6: Governance-Locked Model Selector
 *
 * The UI is a Passive Requestor. It sends POST /model/select and then
 * watches the polling loop for the engine's authoritative LOADING -> READY
 * confirmation. The selector is strictly disabled while the engine is in
 * LOADING (state 1) or FAULT (state 3), eliminating all race conditions.
 */

import React, { useEffect, useRef, useState } from 'react';
import { engineService, EngineStateCode } from '../engine/EngineService';

export const ModelSelector: React.FC = () => {
  const [models, setModels] = useState<string[]>([]);
  const [stateCode, setStateCode] = useState<EngineStateCode>(() => engineService.getStateCode());
  const [switchError, setSwitchError] = useState<string | null>(null);
  const mountedRef = useRef(true);

  // Fetch model list once on mount.
  useEffect(() => {
    void engineService.fetchAvailableModels().then((list) => {
      if (mountedRef.current) setModels(list);
    });

    return () => {
      mountedRef.current = false;
    };
  }, []);

  // Track engine state reactively from the existing 1Hz polling loop.
  useEffect(() => {
    const unsubscribe = engineService.onStatusChange((status) => {
      if (!mountedRef.current) return;
      const stateMap = ['IDLE', 'LOADING', 'READY', 'FAULT'] as const;
      const code = stateMap[status.loader_context.state] ?? 'UNKNOWN';
      setStateCode(code);
    });
    return unsubscribe;
  }, []);

  // Governance Rule: selector is strictly read-only while engine is LOADING or FAULT.
  const isLocked = stateCode === 'LOADING' || stateCode === 'FAULT';

  const handleSelect = async (model: string) => {
    if (isLocked) return;
    setSwitchError(null);

    try {
      await engineService.requestModelLoad(model);
      // Do NOT set active model locally — wait for engine authority via polling loop.
    } catch (err) {
      if (mountedRef.current) {
        setSwitchError(err instanceof Error ? err.message : 'Model switch rejected');
      }
    }
  };

  const currentModel = engineService.getRecommendedModel();

  return (
    <div className="model-selector">
      <div className="model-selector-header">
        <label className="model-selector-label">Model:</label>
        {stateCode === 'LOADING' && (
          <span className="model-selector-switching">Loading…</span>
        )}
      </div>

      <select
        className="model-selector-dropdown"
        disabled={isLocked || models.length === 0}
        value={currentModel}
        onChange={(e) => void handleSelect(e.target.value)}
        aria-label="Select inference model"
      >
        {/* Ensure current model always appears even if list not yet fetched */}
        {currentModel && !models.includes(currentModel) && (
          <option value={currentModel}>{currentModel}</option>
        )}
        {models.map((m) => (
          <option key={m} value={m}>{m}</option>
        ))}
      </select>

      {switchError && <div className="model-selector-error">{switchError}</div>}

      {isLocked && stateCode === 'FAULT' && (
        <div className="model-selector-blocked">Locked — engine in fault</div>
      )}
    </div>
  );
};

