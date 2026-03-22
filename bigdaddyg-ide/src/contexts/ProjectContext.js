/**
 * Project root, file tree, and active editor buffer (renderer).
 *
 * M01 — Does not: run agents, execute terminal commands, or enforce main-process path policy (Electron main still gates `read_file` / `write_file`).
 * M02 — `LAST_PROJECT_ROOT_KEY` remembers last root for “reopen”; clear via `localStorage.removeItem` for a clean slate.
 * M03 — IPC failures: prefer explicit error text + one recovery step where `{ success, error }` is returned.
 * M05 — Dev-only logs use `[project]` prefix when optional diagnostics run.
 */
import React, { createContext, useContext, useState, useCallback } from 'react';

const ProjectContext = createContext(null);

/** M02 — last opened project path for “Open last project” in sidebar empty state. Does not store file contents. */
const LAST_PROJECT_ROOT_KEY = 'rawrxd.ide.lastProjectRoot';

function devProjectWarn(...args) {
  try {
    if (process.env.NODE_ENV === 'development' || window.electronAPI?.isDev) {
      // eslint-disable-next-line no-console
      console.warn('[project]', ...args);
    }
  } catch {
    /* ignore */
  }
}

/** Hide heavy / VCS dirs in the sidebar (aligned with electron main workspace search). */
const SIDEBAR_IGNORE = new Set([
  'node_modules',
  '.git',
  'dist',
  'build',
  '.next',
  'out',
  '__pycache__',
  'target',
  '.cache',
  'coverage',
  'build_unified',
  'build_out'
]);

function getLanguage(name) {
  const ext = (name || '').split('.').pop()?.toLowerCase() || '';
  const map = {
    js: 'javascript',
    jsx: 'javascript',
    ts: 'typescript',
    tsx: 'typescript',
    py: 'python',
    cpp: 'cpp',
    cc: 'cpp',
    cxx: 'cpp',
    h: 'cpp',
    hpp: 'cpp',
    c: 'c',
    json: 'json',
    md: 'markdown',
    html: 'html',
    css: 'css',
    rs: 'rust',
    go: 'go',
    asm: 'asm',
    masm: 'asm'
  };
  return map[ext] || 'plaintext';
}

/**
 * @param {Array<{ name: string, path: string, isDirectory: boolean }>} entries
 */
function buildNodesFromEntries(entries) {
  const filtered = (entries || []).filter((e) => e && e.name && !SIDEBAR_IGNORE.has(e.name));
  filtered.sort((a, b) => {
    if (a.isDirectory !== b.isDirectory) return a.isDirectory ? -1 : 1;
    return a.name.localeCompare(b.name, undefined, { sensitivity: 'base' });
  });
  return filtered.map((e) => ({
    name: e.name,
    path: e.path,
    isDirectory: e.isDirectory,
    /** null = not loaded yet; array = loaded (maybe empty) */
    children: e.isDirectory ? null : undefined
  }));
}

function patchTreeChildren(nodes, targetPath, newChildren) {
  if (!nodes || !nodes.length) return nodes;
  return nodes.map((node) => {
    if (node.path === targetPath && node.isDirectory) {
      return { ...node, children: newChildren };
    }
    if (Array.isArray(node.children)) {
      return { ...node, children: patchTreeChildren(node.children, targetPath, newChildren) };
    }
    return node;
  });
}

/**
 * Provides `projectRoot`, sidebar tree, and `activeFile` buffer state.
 * Does not: write files to disk from here — Save flows call IPC from UI when wired.
 */
export function ProjectProvider({ children }) {
  const [projectRoot, setProjectRoot] = useState(null);
  /** Set when opening a project if readDir(project root) fails — real IPC/main error text for the sidebar. */
  const [rootListingError, setRootListingError] = useState(null);
  const [files, setFiles] = useState([]);
  const [fileTree, setFileTree] = useState([]);
  const [activeFile, setActiveFileState] = useState(null);
  const [fileContentCache, setFileContentCache] = useState({});

  const loadDirectoryChildren = useCallback(async (dirPath) => {
    const api = window.electronAPI;
    if (!api?.readDir) {
      const err = 'readDir unavailable — open the app in Electron or check preload exposes readDir.';
      devProjectWarn('loadDirectoryChildren:', err);
      return { success: false, error: err };
    }
    const dirResult = await api.readDir(dirPath);
    if (!dirResult?.success) {
      const base = dirResult.error || 'readDir failed';
      const err = `${base} — pick another folder or check main-process log.`;
      devProjectWarn('loadDirectoryChildren failed:', dirPath, base);
      return { success: false, error: err };
    }
    const nodes = buildNodesFromEntries(dirResult.data || []);
    setFileTree((prev) => patchTreeChildren(prev, dirPath, nodes));
    return { success: true };
  }, []);

  const openProject = useCallback(async (pathFromDialog) => {
    const api = window.electronAPI;
    if (!api?.openProject && !pathFromDialog) {
      devProjectWarn('openProject: no dialog API and no path — use Open project from the shell.');
      return;
    }
    const result = pathFromDialog
      ? { success: true, data: pathFromDialog }
      : await api.openProject();
    if (!result?.success) {
      devProjectWarn('openProject failed:', result?.error || 'unknown error');
      return;
    }
    const root = result.data;
    if (!root) return;
    try {
      localStorage.setItem(LAST_PROJECT_ROOT_KEY, root);
    } catch {
      /* ignore */
    }
    setProjectRoot(root);
    setFiles([]);
    setFileTree([]);
    setActiveFileState(null);
    setFileContentCache({});
    setRootListingError(null);
    if (api.readDir) {
      const dirResult = await api.readDir(root);
      if (dirResult && !dirResult.success) {
        const errText = dirResult.error || 'readDir failed';
        devProjectWarn('readDir(root) after open failed:', errText);
        setRootListingError(errText);
        setFileTree([]);
        setFiles([]);
      } else if (dirResult?.success && dirResult.data?.length) {
        setRootListingError(null);
        const tree = buildNodesFromEntries(dirResult.data);
        setFileTree(tree);
        const flatFiles = (dirResult.data || [])
          .filter((e) => !e.isDirectory && !SIDEBAR_IGNORE.has(e.name))
          .map((e) => ({
            path: e.path,
            name: e.name,
            language: getLanguage(e.name)
          }));
        setFiles(flatFiles);
      } else if (dirResult?.success) {
        setRootListingError(null);
        setFileTree([]);
        setFiles([]);
      }
    }
  }, []);

  const reopenLastProject = useCallback(async () => {
    let last = null;
    try {
      last = localStorage.getItem(LAST_PROJECT_ROOT_KEY);
    } catch {
      last = null;
    }
    if (last) {
      await openProject(last);
    }
  }, [openProject]);

  const getLastProjectRoot = useCallback(() => {
    try {
      return localStorage.getItem(LAST_PROJECT_ROOT_KEY);
    } catch {
      return null;
    }
  }, []);

  const openFile = useCallback(async (fileNode) => {
    if (!fileNode?.path) return;
    const api = window.electronAPI;
    if (!api?.readFile) return;
    const result = await api.readFile(fileNode.path);
    if (!result?.success) return;
    const content = result.data ?? '';
    const name = fileNode.name || fileNode.path.replace(/^.*[\\/]/, '');
    const language = fileNode.language || getLanguage(name);
    setFileContentCache((prev) => ({ ...prev, [fileNode.path]: content }));
    setActiveFileState({
      path: fileNode.path,
      name,
      content,
      language
    });
  }, []);

  const updateFile = useCallback((path, content) => {
    setActiveFileState((prev) => {
      if (!prev || prev.path !== path) return prev;
      return { ...prev, content };
    });
    setFileContentCache((prev) => ({ ...prev, [path]: content }));
  }, []);

  const setActiveFile = useCallback((file) => {
    setActiveFileState(file);
  }, []);

  React.useEffect(() => {
    const api = window.electronAPI;
    if (api?.onProjectOpened) {
      api.onProjectOpened((path) => openProject(path));
    }
  }, [openProject]);

  const value = {
    projectRoot,
    rootListingError,
    files,
    fileTree,
    activeFile,
    fileContentCache,
    openProject,
    reopenLastProject,
    getLastProjectRoot,
    openFile,
    updateFile,
    setActiveFile,
    loadDirectoryChildren
  };

  return <ProjectContext.Provider value={value}>{children}</ProjectContext.Provider>;
}

/**
 * Project tree + active file hook.
 * Does not: validate paths — relies on Electron main for sandboxing.
 */
export function useProject() {
  const ctx = useContext(ProjectContext);
  if (!ctx) throw new Error('useProject must be used within ProjectProvider');
  return ctx;
}

export default ProjectContext;
