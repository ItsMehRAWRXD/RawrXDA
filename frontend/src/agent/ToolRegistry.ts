import { LowRiskToolAdapter } from './LowRiskToolAdapter';
import { governanceEnforcer } from '../telemetry/GovernanceEnforcer';

export type RiskLevel = 'LOW' | 'MEDIUM' | 'HIGH';

export interface ToolExecutionResult {
  status: 'PENDING_APPROVAL' | 'APPROVED' | 'DENIED' | 'EXECUTED' | 'FAILED';
  message: string;
  output?: unknown;
}

export interface ToolDefinition {
  name: string;
  description: string;
  riskLevel: RiskLevel;
  execute: (params: Record<string, unknown>) => Promise<ToolExecutionResult>;
}

interface ToolManifestEntry {
  name: string;
  description: string;
  riskLevel: RiskLevel;
}

const TOOL_MANIFEST: ToolManifestEntry[] = [
  { name: 'read_file', description: 'Read file content', riskLevel: 'LOW' },
  { name: 'list_dir', description: 'List directory entries', riskLevel: 'LOW' },
  { name: 'search_code', description: 'Search code and symbols', riskLevel: 'LOW' },
  { name: 'get_symbol_refs', description: 'Find symbol references', riskLevel: 'LOW' },
  { name: 'write_file', description: 'Write file content', riskLevel: 'MEDIUM' },
  { name: 'edit_file', description: 'Edit existing file', riskLevel: 'MEDIUM' },
  { name: 'create_file', description: 'Create new file', riskLevel: 'MEDIUM' },
  { name: 'delete_file', description: 'Delete file', riskLevel: 'HIGH' },
  { name: 'rename_file', description: 'Rename file', riskLevel: 'MEDIUM' },
  { name: 'move_file', description: 'Move file', riskLevel: 'MEDIUM' },
  { name: 'open_terminal', description: 'Open terminal session', riskLevel: 'MEDIUM' },
  { name: 'execute_shell', description: 'Execute shell command', riskLevel: 'HIGH' },
  { name: 'install_dependency', description: 'Install package dependency', riskLevel: 'HIGH' },
  { name: 'run_tests', description: 'Run test suite', riskLevel: 'LOW' },
  { name: 'run_lint', description: 'Run linter', riskLevel: 'LOW' },
  { name: 'run_format', description: 'Run formatter', riskLevel: 'LOW' },
  { name: 'git_status', description: 'Read git status', riskLevel: 'LOW' },
  { name: 'git_diff', description: 'Read git diff', riskLevel: 'LOW' },
  { name: 'git_add', description: 'Stage git changes', riskLevel: 'MEDIUM' },
  { name: 'git_commit', description: 'Create git commit', riskLevel: 'HIGH' },
  { name: 'git_branch', description: 'Create/manage branches', riskLevel: 'MEDIUM' },
  { name: 'git_checkout', description: 'Switch branch', riskLevel: 'MEDIUM' },
  { name: 'git_merge', description: 'Merge branches', riskLevel: 'HIGH' },
  { name: 'git_rebase', description: 'Rebase branches', riskLevel: 'HIGH' },
  { name: 'create_pr', description: 'Open pull request', riskLevel: 'MEDIUM' },
  { name: 'fetch_http', description: 'Fetch HTTP endpoint', riskLevel: 'LOW' },
  { name: 'post_http', description: 'POST to HTTP endpoint', riskLevel: 'MEDIUM' },
  { name: 'parse_json', description: 'Parse JSON content', riskLevel: 'LOW' },
  { name: 'parse_yaml', description: 'Parse YAML content', riskLevel: 'LOW' },
  { name: 'parse_markdown', description: 'Parse Markdown content', riskLevel: 'LOW' },
  { name: 'run_sql_query', description: 'Run SQL query', riskLevel: 'HIGH' },
  { name: 'migrate_db', description: 'Run DB migrations', riskLevel: 'HIGH' },
  { name: 'seed_db', description: 'Seed database', riskLevel: 'HIGH' },
  { name: 'start_dev_server', description: 'Start dev server', riskLevel: 'MEDIUM' },
  { name: 'stop_dev_server', description: 'Stop dev server', riskLevel: 'MEDIUM' },
  { name: 'run_benchmark', description: 'Run benchmark process', riskLevel: 'MEDIUM' },
  { name: 'collect_telemetry', description: 'Collect telemetry', riskLevel: 'MEDIUM' },
  { name: 'inspect_logs', description: 'Inspect service logs', riskLevel: 'LOW' },
  { name: 'restart_engine', description: 'Restart engine process', riskLevel: 'HIGH' },
  { name: 'deploy_staging', description: 'Deploy to staging', riskLevel: 'HIGH' },
  { name: 'deploy_production', description: 'Deploy to production', riskLevel: 'HIGH' },
  { name: 'rollback_release', description: 'Rollback release', riskLevel: 'HIGH' },
  { name: 'manage_secrets', description: 'Manage secret material', riskLevel: 'HIGH' },
  { name: 'update_ci_workflow', description: 'Modify CI workflow', riskLevel: 'HIGH' },
];

const createDeniedTool = (entry: ToolManifestEntry): ToolDefinition => ({
  ...entry,
  execute: async (_params: Record<string, unknown>) => ({
    status: 'DENIED',
    message: `Tool ${entry.name} is disabled in propose-only mode.`,
  }),
});

const createToolDefinition = (entry: ToolManifestEntry): ToolDefinition => {
  if (entry.name === 'read_file') {
    return {
      ...entry,
      execute: LowRiskToolAdapter.readFile,
    };
  }

  if (entry.name === 'list_dir') {
    return {
      ...entry,
      execute: LowRiskToolAdapter.listDir,
    };
  }

  if (entry.name === 'search_code') {
    return {
      ...entry,
      execute: LowRiskToolAdapter.searchCode,
    };
  }

  if (entry.name === 'write_file') {
    return {
      ...entry,
      execute: LowRiskToolAdapter.writeFile,
    };
  }

  return createDeniedTool(entry);
};

export const DEFAULT_TOOL_REGISTRY: ToolDefinition[] = TOOL_MANIFEST.map(createToolDefinition);

export const TOOL_REGISTRY_BY_NAME: Record<string, ToolDefinition> = DEFAULT_TOOL_REGISTRY
  .reduce<Record<string, ToolDefinition>>((acc, tool) => {
    acc[tool.name] = tool;
    return acc;
  }, {});

/**
 * StatefulRegistry
 * Day 18: Dynamic Override Layer — wraps the static registry with runtime
 * permission ratcheting driven by the GovernanceEnforcer.
 *
 * All consumers should call getEffectiveRiskLevel() instead of reading
 * tool.riskLevel directly, so that demotions are respected.
 */
export class StatefulRegistry {
  private baseRegistry: Record<string, ToolDefinition>;

  constructor(baseRegistry: Record<string, ToolDefinition> = TOOL_REGISTRY_BY_NAME) {
    this.baseRegistry = { ...baseRegistry };
  }

  public getTool(name: string): ToolDefinition | undefined {
    return this.baseRegistry[name];
  }

  public getToolNames(): string[] {
    return Object.keys(this.baseRegistry).sort();
  }

  public getEffectiveRiskLevel(toolName: string): RiskLevel {
    const tool = this.baseRegistry[toolName];
    if (!tool) {
      return 'HIGH';
    }
    return governanceEnforcer.getEffectiveRiskLevel(toolName, tool.riskLevel);
  }

  public getEffectiveTool(toolName: string): ToolDefinition | undefined {
    const tool = this.baseRegistry[toolName];
    if (!tool) {
      return undefined;
    }
    const effectiveRisk = this.getEffectiveRiskLevel(toolName);
    if (effectiveRisk === tool.riskLevel) {
      return tool;
    }
    return {
      ...tool,
      riskLevel: effectiveRisk,
    };
  }

  public listTools(): ToolDefinition[] {
    return Object.values(this.baseRegistry).map((tool) => {
      const effectiveRisk = this.getEffectiveRiskLevel(tool.name);
      if (effectiveRisk === tool.riskLevel) {
        return tool;
      }
      return {
        ...tool,
        riskLevel: effectiveRisk,
      };
    });
  }
}

export const statefulRegistry = new StatefulRegistry();
