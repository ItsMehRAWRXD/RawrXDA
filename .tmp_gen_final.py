import json, subprocess
from pathlib import Path
p = subprocess.run(['python','scripts/audit_command_table.py','--json-only'], capture_output=True, text=True)
if not p.stdout.strip():
    raise SystemExit('no json output from audit')
j = json.loads(p.stdout)
missing = j['missing_from_table']

def handler_for(name,cat):
    n=name
    if n=='IDM_CONFIDENCE_SET_POLICY': return 'handleConfidenceSetPolicy','CMD_NONE'
    if n.startswith('IDM_ROUTER_'):
        m={'IDM_ROUTER_SHOW_DECISION':'handleRouterDecision','IDM_ROUTER_SET_POLICY':'handleRouterSetPolicy','IDM_ROUTER_SHOW_CAPABILITIES':'handleRouterCapabilities','IDM_ROUTER_SHOW_FALLBACKS':'handleRouterFallbacks','IDM_ROUTER_SAVE_CONFIG':'handleRouterSaveConfig','IDM_ROUTER_ROUTE_PROMPT':'handleRouterRoutePrompt','IDM_ROUTER_RESET_STATS':'handleRouterResetStats','IDM_ROUTER_SHOW_HEATMAP':'handleRouterShowHeatmap','IDM_ROUTER_ENSEMBLE_ENABLE':'handleRouterEnsembleEnable','IDM_ROUTER_ENSEMBLE_DISABLE':'handleRouterEnsembleDisable','IDM_ROUTER_ENSEMBLE_STATUS':'handleRouterEnsembleStatus','IDM_ROUTER_SIMULATE':'handleRouterSimulate','IDM_ROUTER_SHOW_COST_STATS':'handleRouterShowCostStats'}
        return m.get(n,'handleRouterStatus'),'CMD_NONE'
    if n.startswith('IDM_HYBRID_'):
        m={'IDM_HYBRID_EXPLAIN_SYMBOL':'handleHybridExplainSymbol','IDM_HYBRID_STREAM_ANALYZE':'handleHybridStreamAnalyze','IDM_HYBRID_SEMANTIC_PREFETCH':'handleHybridSemanticPrefetch','IDM_HYBRID_CORRECTION_LOOP':'handleHybridCorrectionLoop'}
        return m.get(n,'handleHybridStatus'),'CMD_NONE'
    if n.startswith('IDM_MULTI_RESP_'):
        m={'IDM_MULTI_RESP_SELECT_PREFERRED':'handleMultiRespSelectPreferred','IDM_MULTI_RESP_TOGGLE_TEMPLATE':'handleMultiRespToggleTemplate','IDM_MULTI_RESP_APPLY_PREFERRED':'handleMultiRespApplyPreferred'}
        return m.get(n,'handleMultiRespShowStatus'),'CMD_NONE'
    if n.startswith('IDM_SWARM_'): return 'handleSwarmStatus','CMD_NONE'
    if n.startswith('IDM_VOICE_AUTO_'): return 'handleVoiceMode','CMD_NONE'
    if n.startswith('IDM_VOICE_'):
        m={'IDM_VOICE_RECORD':'handleVoiceRecord','IDM_VOICE_PTT':'handleVoiceMode','IDM_VOICE_SPEAK':'handleVoiceSpeak','IDM_VOICE_JOIN_ROOM':'handleVoiceStatus','IDM_VOICE_SHOW_DEVICES':'handleVoiceDevices','IDM_VOICE_METRICS':'handleVoiceMetrics','IDM_VOICE_TOGGLE_PANEL':'handleVoiceStatus','IDM_VOICE_MODE_PTT':'handleVoiceMode','IDM_VOICE_MODE_CONTINUOUS':'handleVoiceMode','IDM_VOICE_MODE_DISABLED':'handleVoiceMode','IDM_VOICE_TOGGLE_LISTENING':'handleVoiceStatus','IDM_VOICE_TOGGLE_TTS':'handleVoiceSpeak','IDM_VOICE_SETTINGS':'handleVoiceMode','IDM_VOICE_TTS_TEST':'handleVoiceSpeak'}
        return m.get(n,'handleVoiceStatus'),'CMD_NONE'
    if n.startswith('IDM_HOTPATCH_'):
        m={'IDM_HOTPATCH_BYTE_SEARCH':'handleHotpatchByte','IDM_HOTPATCH_SERVER_REMOVE':'handleHotpatchServer','IDM_HOTPATCH_PROXY_REWRITE':'handleHotpatchServer','IDM_HOTPATCH_PROXY_TERMINATE':'handleHotpatchServer','IDM_HOTPATCH_PROXY_VALIDATE':'handleHotpatchServer','IDM_HOTPATCH_RESET_STATS':'handleHotpatchStatus','IDM_HOTPATCH_SHOW_PROXY_STATS':'handleHotpatchStatus','IDM_HOTPATCH_SET_TARGET_TPS':'handleHotpatchStatus'}
        return m.get(n,'handleHotpatchStatus'),'CMD_NONE'
    if n.startswith('IDM_BUILD_'): return 'handleToolsBuild','CMD_ASYNC'
    if n.startswith('IDM_EDITOR_'):
        if 'GOTO_LINE' in n: return 'handleEditGotoLine','CMD_REQUIRES_FILE'
        if 'GOTO_' in n or 'PEEK_' in n or 'INLAY' in n or 'CODE_ACTION' in n: return 'handleLspGotoDef','CMD_REQUIRES_FILE'
        if 'TOGGLE_COMMENT' in n: return 'handleEditToggleComment','CMD_REQUIRES_FILE'
        if 'DUPLICATE_LINE' in n: return 'handleEditDuplicateLine','CMD_REQUIRES_FILE'
        if 'DELETE_LINE' in n: return 'handleEditDeleteLine','CMD_REQUIRES_FILE'
        if 'MOVE_LINE' in n: return 'handleEditMoveLine','CMD_REQUIRES_FILE'
        return 'handleEditFind','CMD_REQUIRES_FILE'
    if n.startswith('IDM_AGENTIC_'): return 'handleAgentLoop','CMD_ASYNC'
    if n.startswith('IDM_AGENT_'): return 'handleAgentLoop','CMD_ASYNC'
    if n.startswith('IDM_FILE_'): return 'handleFileOpen','CMD_NONE'
    if n.startswith('IDM_VIEW_'): return 'handleViewToggleSidebar','CMD_NONE'
    if n.startswith('IDM_SECURITY_'): return 'handleSafetyStatus','CMD_NONE'
    if n.startswith('IDM_TELEMETRY_'): return 'handleAnalyze','CMD_NONE'
    if n.startswith('IDM_VSCEXT_'): return 'handleToolsExtensions','CMD_NONE'
    if n.startswith('IDM_LSP_'): return 'handleLspStatus','CMD_NONE'
    if n.startswith('IDM_EXEC_MODE_'): return 'handleAIModeSet','CMD_NONE'
    if n.startswith('IDM_COPILOT_'): return 'handleHistory','CMD_NONE'
    if n.startswith('IDM_OMEGA_'): return 'handleAnalyze','CMD_NONE'
    if n.startswith('IDM_ENT_'): return 'handleAnalyze','CMD_NONE'
    if n.startswith('IDM_AI_'): return 'handleAIMaxMode','CMD_NONE'
    if n.startswith('IDM_IMPACT_'): return 'handleAnalyze','CMD_NONE'
    return 'handleAnalyze','CMD_NONE'

def canon(name): return name.lower().replace('idm_','legacy.').replace('_','.')
def sym(name): return name.replace('IDM_','LEGACY_FINAL_')
def cli(name): return '!legacy_' + name.lower().replace('idm_','').replace('_',' ')

lines=['    /* Final catch-up aliases: remaining missing IDM routes */                                                   \\\\']
for m in missing:
    name,idv,cat=m['name'],m['value'],m['category']
    h,flags=handler_for(name,cat)
    lines.append(f'    X({idv}, {sym(name)}, "{canon(name)}", "{cli(name)}", GUI_ONLY, "{cat.title()}", {h}, {flags}) \\\\')
Path('tmp_final_catchup_entries.txt').write_text('\n'.join(lines), encoding='utf-8')
print('wrote',len(missing),'entries')
