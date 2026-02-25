// BigDaddyGEngine/copilot/index.ts
// Main export file for Copilot Core

export { CopilotCore, getGlobalCopilot, initializeCopilot } from './CopilotCore';
export { AgentRegistry } from './AgentRegistry';
export { ContextTracker } from './ContextTracker';
export { Orchestrator } from './Orchestrator';
export { ReflectionEngine } from './ReflectionEngine';
export { MemoryFusion } from './MemoryFusion';
export { useVoiceInterface, createVoiceInterface } from './hooks/useVoiceInterface';
export { useStreamedInference, createStreamedInference } from './hooks/useStreamedInference';

// Re-export types
export type { AgentSpec } from './AgentRegistry';
export type { ContextEntry } from './ContextTracker';
export type { AgentRoute } from './Orchestrator';
export type { ReflectionTrace } from './ReflectionEngine';
export type { AgentMemory, MemoryEntry } from './MemoryFusion';
export type { VoiceConfig, UseVoiceInterfaceReturn } from './hooks/useVoiceInterface';
export type { InferenceConfig, UseStreamedInferenceReturn } from './hooks/useStreamedInference';
