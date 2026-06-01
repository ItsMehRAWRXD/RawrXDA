import { engineService } from '../engine/EngineService';
import type { ToolExecutionResult } from './ToolRegistry';

const asString = (value: unknown, fallback: string): string => {
  return typeof value === 'string' && value.length > 0 ? value : fallback;
};

const asNumber = (value: unknown, fallback: number): number => {
  return typeof value === 'number' && Number.isFinite(value) ? value : fallback;
};

export class LowRiskToolAdapter {
  public static async readFile(params: Record<string, unknown>): Promise<ToolExecutionResult> {
    const path = asString(params.path, '.');
    const startLine = asNumber(params.startLine, 1);
    const endLine = asNumber(params.endLine, 200);

    try {
      const output = await engineService.toolReadFile(path, startLine, endLine);
      return {
        status: 'EXECUTED',
        message: `read_file executed for ${path}`,
        output,
      };
    } catch (error) {
      return {
        status: 'FAILED',
        message: error instanceof Error ? error.message : 'read_file failed',
      };
    }
  }

  public static async listDir(params: Record<string, unknown>): Promise<ToolExecutionResult> {
    const path = asString(params.path, '.');

    try {
      const output = await engineService.toolListDir(path);
      return {
        status: 'EXECUTED',
        message: `list_dir executed for ${path}`,
        output,
      };
    } catch (error) {
      return {
        status: 'FAILED',
        message: error instanceof Error ? error.message : 'list_dir failed',
      };
    }
  }

  public static async searchCode(params: Record<string, unknown>): Promise<ToolExecutionResult> {
    const query = asString(params.query, 'TODO');

    try {
      const output = await engineService.toolSearchCode(query);
      return {
        status: 'EXECUTED',
        message: `search_code executed for query '${query}'`,
        output,
      };
    } catch (error) {
      return {
        status: 'FAILED',
        message: error instanceof Error ? error.message : 'search_code failed',
      };
    }
  }

  public static async writeFile(params: Record<string, unknown>): Promise<ToolExecutionResult> {
    const path = asString(params.path, 'tmp/agent_write.txt');
    const content = typeof params.content === 'string' ? params.content : '';
    const append = params.append === true;
    const createDirs = params.createDirs !== false;

    try {
      const output = await engineService.toolWriteFile(path, content, append, createDirs);
      return {
        status: 'EXECUTED',
        message: `write_file executed for ${path}`,
        output,
      };
    } catch (error) {
      return {
        status: 'FAILED',
        message: error instanceof Error ? error.message : 'write_file failed',
      };
    }
  }
}
