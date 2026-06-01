import React from 'react';
import { ideBridge } from '../bridge/ideBridge';
import { FaultSidecar } from '../engine/EngineService';

interface CrashRecoveryModalProps {
  policy: FaultSidecar;
  engineReachable: boolean;
  onRestarted: () => void;
}

export const CrashRecoveryModal: React.FC<CrashRecoveryModalProps> = ({
  policy,
  engineReachable,
  onRestarted,
}) => {
  const handleRestart = async () => {
    const success = await ideBridge.restartEngine();
    if (success) {
      onRestarted();
      return;
    }

    window.alert('System could not restart engine. Please restart manually.');
  };

  return (
    <div className="recovery-overlay" role="dialog" aria-modal="true" aria-labelledby="recovery-title">
      <div className="recovery-modal">
        <div className={`recovery-source recovery-source-${policy.source.toLowerCase()}`}>
          {policy.source === 'DEAD_MAN_SWITCH' ? 'DEAD MAN SWITCH' : 'ENGINE REPORTED'}
        </div>
        <h2 id="recovery-title">Engine Fault Detected</h2>
        <p className="recovery-subtitle">
          {engineReachable
            ? 'The engine has entered a governed recovery path.'
            : 'The engine heartbeat went dark. Recovery policy was loaded from the fault bridge.'}
        </p>

        <div className="recovery-details">
          <div>
            <strong>Fault Class:</strong> {policy.fault_class}
          </div>
          <div>
            <strong>Action:</strong> {policy.suggested_action}
          </div>
          <div>
            <strong>Session:</strong> {policy.session_id}
          </div>
          <div>
            <strong>Model:</strong> {policy.active_model}
          </div>
        </div>

        <button type="button" className="recovery-button" onClick={() => void handleRestart()}>
          Restart Engine Process
        </button>
      </div>
    </div>
  );
};