/**
 * StatusMapper.ts
 * Maps engine integer states (0-3) to UI labels and colors.
 * Pure function; no side effects.
 */

export interface UIStateStyle {
  label: string;
  color: string;
  icon: string;
  description: string;
}

export const EngineStateMap: Record<number, UIStateStyle> = {
  0: {
    label: 'Idle',
    color: '#9CA3AF',
    icon: '⚪',
    description: 'Engine is idle and ready to load a model.',
  },
  1: {
    label: 'Loading',
    color: '#3B82F6',
    icon: '⏳',
    description: 'Engine is loading a model. Please wait...',
  },
  2: {
    label: 'Ready',
    color: '#10B981',
    icon: '🟢',
    description: 'Engine is ready for inference.',
  },
  3: {
    label: 'Fault',
    color: '#EF4444',
    icon: '⚠️',
    description: 'Engine encountered an error. Check the fault panel for recovery options.',
  },
};

export const getStatusUI = (stateCode: number): UIStateStyle => {
  return EngineStateMap[stateCode] || { label: 'Unknown', color: '#000000', icon: '❓', description: 'Unknown state' };
};
