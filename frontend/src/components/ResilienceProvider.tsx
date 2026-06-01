import React, { createContext, useEffect, useMemo, useRef, useState } from 'react';
import { engineService, EngineStatus, FaultSidecar } from '../engine/EngineService';
import { CrashRecoveryModal } from './CrashRecoveryModal';

interface ResilienceContextValue {
  fault: FaultSidecar | null;
  isEngineDown: boolean;
}

export const ResilienceContext = createContext<ResilienceContextValue>({
  fault: null,
  isEngineDown: false,
});

interface ResilienceProviderProps {
  children: React.ReactNode;
}

const FAULT_ENDPOINT = 'http://localhost:11435/fault';

export const ResilienceProvider: React.FC<ResilienceProviderProps> = ({ children }) => {
  const [fault, setFault] = useState<FaultSidecar | null>(null);
  const [isEngineDown, setIsEngineDown] = useState(false);
  const recoveryLatchedRef = useRef(false);

  const clearRecoveryState = () => {
    recoveryLatchedRef.current = false;
    setFault(null);
    setIsEngineDown(false);
  };

  useEffect(() => {
    let isMounted = true;

    const latchFault = (nextFault: FaultSidecar, engineReachable: boolean) => {
      if (!isMounted) {
        return;
      }

      recoveryLatchedRef.current = true;
      setFault(nextFault);
      setIsEngineDown(!engineReachable);
    };

    const buildEngineReportedFault = (status: EngineStatus): FaultSidecar => ({
      session_id: status.session_id,
      status_seq: status.status_seq,
      fault_class: status.loader_context.fault_class || status.last_error_tag || 'ENGINE_FAULT',
      suggested_action: status.loader_context.suggested_action || 'FOLLOW_ENGINE_POLICY',
      process_id: 0,
      active_model: status.recommended_model || engineService.getRecommendedModel(),
      source: 'ENGINE_REPORTED',
      timestamp: Date.now(), // Snapshotted at moment of observed state 3 — use for log correlation.
    });

    const fetchFaultPolicy = async (engineReachable: boolean) => {
      try {
        const response = await fetch(FAULT_ENDPOINT);
        if (!response.ok) {
          throw new Error(`Fault policy HTTP ${response.status}`);
        }

        const data: FaultSidecar = await response.json();
        latchFault(
          {
            ...data,
            source: engineReachable ? 'ENGINE_REPORTED' : 'DEAD_MAN_SWITCH',
          },
          engineReachable
        );
      } catch {
        if (isMounted) {
          latchFault(
            {
            session_id: engineService.getSessionId(),
            status_seq: engineService.getStatusSeq(),
            fault_class: engineReachable ? 'FAULT_POLICY_UNREACHABLE' : 'HEARTBEAT_LOSS',
            suggested_action: engineReachable ? 'VERIFY_ENGINE_AND_FAULT_PROXY' : 'RESTART_ENGINE_PROCESS',
            process_id: 0,
            active_model: engineService.getRecommendedModel(),
            source: engineReachable ? 'ENGINE_REPORTED' : 'DEAD_MAN_SWITCH',            timestamp: Date.now(),            },
            engineReachable
          );
        }
      }
    };

    engineService.startPolling();

    const unsubscribeStatus = engineService.onStatusChange((status) => {
      if (!isMounted) {
        return;
      }

      if (status.loader_context.state === 3) {
        latchFault(buildEngineReportedFault(status), true);
        return;
      }

      if (recoveryLatchedRef.current) {
        return;
      }

      setIsEngineDown(false);
      setFault(null);
    });

    const unsubscribeHeartbeatLoss = engineService.onHeartbeatLoss(() => {
      if (!recoveryLatchedRef.current) {
        void fetchFaultPolicy(false);
      }
    });

    return () => {
      isMounted = false;
      unsubscribeStatus();
      unsubscribeHeartbeatLoss();
      engineService.stopPolling();
    };
  }, []);

  const contextValue = useMemo(
    () => ({
      fault,
      isEngineDown,
    }),
    [fault, isEngineDown]
  );

  return (
    <ResilienceContext.Provider value={contextValue}>
      {fault && (
        <CrashRecoveryModal
          policy={fault}
          engineReachable={!isEngineDown}
          onRestarted={() => {
            clearRecoveryState();
            engineService.startPolling();
          }}
        />
      )}
      {children}
    </ResilienceContext.Provider>
  );
};