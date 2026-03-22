import React from 'react';
import { motion, AnimatePresence } from 'framer-motion';
// @ts-ignore TS1261: workspace opened with mixed root casing (d:/RawrXD vs d:/rawrxd)
import { useIdeFeatures } from '../contexts/IdeFeaturesContext';
import {
  CopySupportLineButton,
  focusVisibleRing,
  MinimalSurfaceM814Footer,
  MINIMALISTIC_DOC
} from '../utils/minimalisticM08M14';
import ChatPanel from './ChatPanel';
import AgentPanel from './AgentPanel';
import ModulesPanel from './ModulesPanel';
import ModelsPanel from './ModelsPanel';
import SymbolsPanel from './SymbolsPanel';

const TABS = [
  { id: 'chat', label: 'Chat', title: 'Chat panel (Ctrl+L)' },
  { id: 'agent', label: 'Agent', title: 'Agent panel (Ctrl+Shift+A)' },
  { id: 'modules', label: 'Modules', title: 'Module toggles (Ctrl+Shift+M)' },
  { id: 'symbols', label: 'Symbols', title: 'Text symbol index (Ctrl+Shift+Y)' },
  { id: 'models', label: 'Models', title: 'GGUF streamer (Ctrl+Shift+G)' }
];

/**
 * Secondary sidebar: Chat (Copilot) | Agent (autonomous) | Modules — Cursor-like layout.
 */
const RightSidebarDock = ({
  tab,
  onSelectTab,
  onClose,
  activeFile,
  projectRoot,
  settings,
  agentProps
}) => {
  const open = tab != null;
  const {
    settings: shellSettings,
    setStatusLine,
    noisyLog,
    pushToast,
    noisyLogVerbose,
    accessibilityReducedMotionEffective
  } = useIdeFeatures();
  const max = shellSettings.noiseIntensity === 'maximum';
  const reduceDockMotion = accessibilityReducedMotionEffective;

  React.useEffect(() => {
    if (!open || tab == null) return;
    setStatusLine(`[dock] panel=${tab}`);
    noisyLog('[dock]', 'active tab', tab);
    noisyLogVerbose('dock', 'tab', tab, Date.now());
  }, [open, tab, setStatusLine, noisyLog, noisyLogVerbose]);

  return (
    <AnimatePresence>
      {open && (
        <motion.div
          id="rawrxd-dock-region"
          tabIndex={-1}
          role="complementary"
          aria-label="AI dock: chat, autonomous agent, models, and modules"
          initial={
            reduceDockMotion ? false : { width: 0, opacity: 0, x: max ? 24 : 0 }
          }
          animate={{ width: 400, opacity: 1, x: 0 }}
          exit={
            reduceDockMotion
              ? { opacity: 0, transition: { duration: 0 } }
              : { width: 0, opacity: 0, x: max ? 16 : 0 }
          }
          transition={
            reduceDockMotion
              ? { duration: 0 }
              : max
                ? { type: 'spring', stiffness: 420, damping: 28 }
                : { type: 'tween', duration: 0.2 }
          }
          className="rawrxd-a11y-dock flex-shrink-0 border-l border-gray-700 flex flex-col min-w-0 bg-ide-sidebar overflow-hidden"
        >
          <div className="flex border-b border-gray-700 shrink-0" role="tablist" aria-label="Right dock panels">
            {TABS.map((t) => (
              <button
                key={t.id}
                type="button"
                role="tab"
                id={`rawrxd-dock-tab-${t.id}`}
                aria-selected={tab === t.id}
                aria-controls={`rawrxd-dock-panel-${t.id}`}
                onClick={() => onSelectTab(t.id)}
                title={t.title}
                aria-label={`${t.label} panel`}
                className={`flex-1 py-2 text-xs font-medium ${focusVisibleRing} ${
                  tab === t.id ? 'text-white border-b-2 border-ide-accent bg-gray-800/40' : 'text-gray-500 hover:text-gray-300'
                }`}
              >
                {t.label}
              </button>
            ))}
            <button
              type="button"
              onClick={onClose}
              className={`px-3 text-gray-500 hover:text-white text-xs ${focusVisibleRing}`}
              aria-label="Close side panel"
              title="Close dock (panels stay mounted; last tab is remembered)"
            >
              ✕
            </button>
          </div>
          {/* Keep every panel mounted so tab switches do not wipe state (GGUF manifest, chat history, agent UI). */}
          <div className="flex-1 min-h-0 overflow-hidden flex flex-col">
            <div
              id="rawrxd-dock-panel-chat"
              role="tabpanel"
              aria-labelledby="rawrxd-dock-tab-chat"
              className={
                tab === 'chat'
                  ? 'flex min-h-0 flex-1 flex-col overflow-hidden'
                  : 'hidden'
              }
              aria-hidden={tab !== 'chat'}
            >
              <ChatPanel activeFile={activeFile} projectRoot={projectRoot} settings={settings} />
            </div>
            <div
              id="rawrxd-dock-panel-agent"
              role="tabpanel"
              aria-labelledby="rawrxd-dock-tab-agent"
              className={
                tab === 'agent'
                  ? 'flex min-h-0 flex-1 flex-col overflow-hidden'
                  : 'hidden'
              }
              aria-hidden={tab !== 'agent'}
            >
              <AgentPanel
                onStartAgent={agentProps.onStartAgent}
                taskId={agentProps.taskId}
                taskStatus={agentProps.taskStatus}
                steps={agentProps.steps}
                onRefresh={agentProps.onRefresh}
                onRefreshTaskList={agentProps.onRefreshTaskList}
                taskSnapshots={agentProps.taskSnapshots}
                onSelectTask={agentProps.onSelectTask}
                pendingApproval={agentProps.pendingApproval}
                onApproveStep={agentProps.onApproveStep}
                onCancelTask={agentProps.onCancelTask}
                taskGoal={agentProps.taskGoal}
                taskLog={agentProps.taskLog}
                autoApproveWrites={agentProps.autoApproveWrites}
                onAutoApproveWritesChange={agentProps.onAutoApproveWritesChange}
                autoApproveReads={agentProps.autoApproveReads}
                onAutoApproveReadsChange={agentProps.onAutoApproveReadsChange}
                requireAskAiApproval={agentProps.requireAskAiApproval}
                onRequireAskAiApprovalChange={agentProps.onRequireAskAiApprovalChange}
                enableReflection={agentProps.enableReflection}
                onEnableReflectionChange={agentProps.onEnableReflectionChange}
                requirePlanApproval={agentProps.requirePlanApproval}
                onRequirePlanApprovalChange={agentProps.onRequirePlanApprovalChange}
                batchApproveMutations={agentProps.batchApproveMutations}
                onBatchApproveMutationsChange={agentProps.onBatchApproveMutationsChange}
                enableRollbackSnapshots={agentProps.enableRollbackSnapshots}
                onEnableRollbackSnapshotsChange={agentProps.onEnableRollbackSnapshotsChange}
                rollbackCount={agentProps.rollbackCount}
                onRollbackMutations={agentProps.onRollbackMutations}
              />
            </div>
            <div
              id="rawrxd-dock-panel-modules"
              role="tabpanel"
              aria-labelledby="rawrxd-dock-tab-modules"
              className={
                tab === 'modules'
                  ? 'flex min-h-0 flex-1 flex-col overflow-hidden'
                  : 'hidden'
              }
              aria-hidden={tab !== 'modules'}
            >
              <ModulesPanel modules={agentProps.modules} toggleModule={agentProps.toggleModule} />
            </div>
            <div
              id="rawrxd-dock-panel-symbols"
              role="tabpanel"
              aria-labelledby="rawrxd-dock-tab-symbols"
              className={
                tab === 'symbols'
                  ? 'flex min-h-0 flex-1 flex-col overflow-hidden'
                  : 'hidden'
              }
              aria-hidden={tab !== 'symbols'}
            >
              <SymbolsPanel activeFile={activeFile} />
            </div>
            <div
              id="rawrxd-dock-panel-models"
              role="tabpanel"
              aria-labelledby="rawrxd-dock-tab-models"
              className={
                tab === 'models'
                  ? 'flex min-h-0 flex-1 flex-col overflow-hidden'
                  : 'hidden'
              }
              aria-hidden={tab !== 'models'}
            >
              <ModelsPanel />
            </div>
          </div>
          <MinimalSurfaceM814Footer
            className="bg-ide-bg/80 px-2"
            surfaceId="right-dock"
            offlineHint="Tab strip works offline; AI tabs need provider/orchestrator when used."
            docPath={MINIMALISTIC_DOC}
            m13Hint="Dev: noisyLogVerbose('dock', …) on tab switch."
          >
            <div className="flex flex-wrap gap-1 items-center px-2 pb-1">
              <CopySupportLineButton
                getText={() => `[dock] active_tab=${tab || 'none'}`}
                label="Copy active tab name"
                className={`text-[9px] px-1.5 py-0.5 rounded border border-gray-700 text-gray-400 hover:text-gray-200 ${focusVisibleRing}`}
                onCopied={() => {
                  pushToast({ title: '[dock]', message: 'Copied tab id.', variant: 'success', durationMs: 2000 });
                  setStatusLine('[dock] support line copied');
                }}
                onFailed={() =>
                  pushToast({ title: '[dock]', message: 'Clipboard unavailable.', variant: 'warn', durationMs: 2600 })
                }
              />
            </div>
          </MinimalSurfaceM814Footer>
        </motion.div>
      )}
    </AnimatePresence>
  );
};

export default RightSidebarDock;
