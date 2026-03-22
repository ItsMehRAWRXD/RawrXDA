import React from 'react';
import { useIdeFeatures } from '../contexts/IdeFeaturesContext';
import { useProject } from '../contexts/ProjectContext';
import {
  focusVisibleRing,
  CopySupportLineButton,
  MinimalSurfaceM814Footer,
  workspaceFolderBasename,
  workspaceRelativePath,
  MINIMALISTIC_DOC
} from '../utils/minimalisticM08M14';

/** M02 — remember expanded folders per project root */
const EXPANDED_STORAGE_KEY = 'rawrxd.ide.sidebarExpanded.v1';

function loadExpandedSet(projectRoot) {
  if (!projectRoot) return new Set();
  try {
    const raw = localStorage.getItem(EXPANDED_STORAGE_KEY);
    const all = raw ? JSON.parse(raw) : {};
    const arr = all[projectRoot];
    return Array.isArray(arr) ? new Set(arr) : new Set();
  } catch {
    return new Set();
  }
}

function saveExpandedSet(projectRoot, set) {
  if (!projectRoot) return;
  try {
    const raw = localStorage.getItem(EXPANDED_STORAGE_KEY);
    const all = raw ? JSON.parse(raw) : {};
    all[projectRoot] = Array.from(set);
    localStorage.setItem(EXPANDED_STORAGE_KEY, JSON.stringify(all));
  } catch {
    /* ignore */
  }
}

function splitPathRoot(root) {
  if (!root) return [];
  return root.split(/[/\\]+/).filter(Boolean);
}

/**
 * Full project file browser: lazy-loaded directories, expand/collapse, sorted folders-first.
 */
const Sidebar = ({
  isOpen,
  onToggle,
  projectRoot,
  rootListingError,
  fileTree,
  activeFile,
  onOpenFolder,
  onOpenFile,
  loadDirectoryChildren
}) => {
  const width = isOpen ? 280 : 48;
  const { pushToast, setStatusLine, noisyLog } = useIdeFeatures();
  const { reopenLastProject, getLastProjectRoot } = useProject();
  const [expandedPaths, setExpandedPaths] = React.useState(() => new Set());
  const [loadingPath, setLoadingPath] = React.useState(null);

  React.useEffect(() => {
    setLoadingPath(null);
    setExpandedPaths(loadExpandedSet(projectRoot));
  }, [projectRoot]);

  React.useEffect(() => {
    if (projectRoot) {
      saveExpandedSet(projectRoot, expandedPaths);
    }
  }, [expandedPaths, projectRoot]);

  const toggleDirectory = React.useCallback(
    async (item) => {
      const p = item.path;
          if (expandedPaths.has(p)) {
        noisyLog('[sidebar] collapse folder', p);
        setStatusLine('[sidebar] folder collapsed');
        setExpandedPaths((prev) => {
          const next = new Set(prev);
          next.delete(p);
          return next;
        });
        return;
      }
      if (item.children === null && typeof loadDirectoryChildren === 'function') {
        setLoadingPath(p);
        setStatusLine('[sidebar] loading folder…');
        noisyLog('[sidebar]', 'load children', p);
        try {
          const r = await loadDirectoryChildren(p);
          if (!r.success) {
            pushToast({
              title: 'Folder',
              message: `${r.error || 'Could not read directory'} — try Refresh or pick another folder (paths must stay under the project).`,
              variant: 'error',
              durationMs: 5000
            });
            setStatusLine('[sidebar] read folder failed');
            noisyLog('[sidebar] load children failed', p, r.error);
            return;
          }
        } finally {
          setLoadingPath(null);
        }
      }
      setExpandedPaths((prev) => new Set(prev).add(p));
      setStatusLine('[sidebar] folder expanded');
      noisyLog('[sidebar] expand folder', p);
    },
    [expandedPaths, loadDirectoryChildren, pushToast, setStatusLine, noisyLog]
  );

  const resetSidebarTree = React.useCallback(() => {
    setExpandedPaths(new Set());
    if (projectRoot) {
      saveExpandedSet(projectRoot, new Set());
    }
    setStatusLine('[sidebar] tree reset (expanded folders cleared)');
    noisyLog('[sidebar] reset expanded paths');
  }, [projectRoot, setStatusLine, noisyLog]);

  const lastRoot = getLastProjectRoot();

  const renderTree = (items, depth = 0) => {
    if (!items || !items.length) return null;
    return (
      <ul className="list-none m-0 p-0" role="tree" aria-label="Project files">
        {items.map((item) => {
          const isExpanded = expandedPaths.has(item.path);
          const isLoading = loadingPath === item.path;
          const pad = 6 + depth * 14;

          if (item.isDirectory) {
            const hasLoaded = item.children !== null;
            const isEmptyLoaded = hasLoaded && item.children.length === 0;
            return (
              <li key={item.path} className="select-none" role="treeitem" aria-expanded={isExpanded}>
                <button
                  type="button"
                  onClick={() => toggleDirectory(item)}
                  title="Expand or collapse this folder in the tree. Does not delete or rename files on disk."
                  className={`flex w-full items-center gap-1 rounded py-1 pr-1 text-left text-xs text-gray-300 hover:bg-gray-700/80 hover:text-white min-w-0 ${focusVisibleRing}`}
                  style={{ paddingLeft: pad }}
                >
                  <span className="w-3 shrink-0 text-[10px] text-gray-500" aria-hidden>
                    {isLoading ? '…' : isExpanded ? '\u25BC' : '\u25B6'}
                  </span>
                  <span className="truncate font-medium text-gray-400">{item.name}</span>
                  <span className="shrink-0 text-gray-600">/</span>
                </button>
                {isExpanded && hasLoaded && !isEmptyLoaded ? (
                  <div className="border-l border-gray-700/50 ml-3">{renderTree(item.children, depth + 1)}</div>
                ) : null}
                {isExpanded && isEmptyLoaded ? (
                  <div
                    className="py-1 text-[10px] text-gray-600 italic"
                    style={{ paddingLeft: pad + 16 }}
                  >
                    Empty folder
                  </div>
                ) : null}
              </li>
            );
          }

          return (
            <li key={item.path} role="none">
              <button
                type="button"
                onClick={() => {
                  if (onOpenFile) {
                    setStatusLine(`[sidebar] opening ${item.name}`);
                    noisyLog('[sidebar] open file', item.path);
                    onOpenFile(item);
                  }
                }}
                title={`Open in editor: ${workspaceRelativePath(item.path, projectRoot)} — read-only from disk; does not run agent.`}
                className={`block w-full truncate rounded py-1 pr-2 text-left text-xs min-w-0 ${focusVisibleRing} ${
                  activeFile?.path === item.path
                    ? 'bg-ide-accent text-white'
                    : 'text-gray-300 hover:bg-gray-700'
                }`}
                style={{ paddingLeft: pad }}
                role="treeitem"
              >
                {item.name}
              </button>
            </li>
          );
        })}
      </ul>
    );
  };

  const breadcrumbParts = splitPathRoot(projectRoot);
  const breadcrumbTail =
    breadcrumbParts.length > 4
      ? ['\u2026', ...breadcrumbParts.slice(-3)]
      : breadcrumbParts;

  return (
    <div
      className="flex flex-col bg-ide-sidebar border-r border-gray-700 flex-shrink-0"
      style={{ width }}
    >
      <div className="flex items-center justify-between gap-1 p-2 border-b border-gray-700 min-h-[40px]">
        {isOpen && (
          <>
            <button
              type="button"
              onClick={onOpenFolder}
              className={`text-xs text-ide-accent hover:underline shrink-0 rounded px-0.5 ${focusVisibleRing}`}
              title="Pick a project folder. Tree is scoped to that root; ignored dirs match workspace policy (node_modules, .git, …)."
            >
              Open folder…
            </button>
            {projectRoot ? (
              <button
                type="button"
                onClick={resetSidebarTree}
                className={`text-[10px] text-gray-500 hover:text-gray-300 shrink-0 ml-1 rounded px-0.5 ${focusVisibleRing}`}
                title="M02 — Clear remembered expanded folders for this project only"
              >
                Reset tree
              </button>
            ) : null}
          </>
        )}
        <button
          type="button"
          onClick={onToggle}
          className={`p-1 rounded hover:bg-gray-700 text-gray-400 ml-auto ${focusVisibleRing}`}
          aria-label={isOpen ? 'Collapse sidebar' : 'Expand sidebar'}
          title={isOpen ? 'Hide file tree (does not close files)' : 'Show project file tree'}
        >
          {isOpen ? '\u2039' : '\u203A'}
        </button>
      </div>
      {isOpen && (
        <div className="flex-1 min-h-0 overflow-auto py-2 flex flex-col">
          {projectRoot ? (
            <>
              <div
                className="text-[10px] text-gray-500 px-2 pb-2 border-b border-gray-700/60 mb-1"
                title={`Project folder: ${workspaceFolderBasename(projectRoot)} — M14: full absolute path not in tooltip; use Copy support line if needed.`}
              >
                <div className="text-[9px] uppercase tracking-wider text-gray-600 mb-0.5">Project</div>
                <div className="flex flex-wrap gap-x-1 gap-y-0.5 break-all">
                  {breadcrumbTail.map((part, i) => (
                    <span key={`${part}-${i}`} className="text-gray-400">
                      {i > 0 ? <span className="text-gray-600 mx-0.5">\u203A</span> : null}
                      <span className={part === '\u2026' ? 'text-gray-600' : 'text-cyan-200/90'}>{part}</span>
                    </span>
                  ))}
                </div>
              </div>
              {rootListingError ? (
                <div
                  className="text-rose-200/95 text-xs px-2 py-2 rounded border border-rose-600/40 bg-rose-950/40 space-y-1"
                  role="alert"
                >
                  <div className="font-semibold text-rose-100">Could not list project root</div>
                  <div className="text-[11px] font-mono break-words text-rose-200/90">{rootListingError}</div>
                  <p className="text-[10px] text-rose-300/80">
                    This message comes from Electron <code className="text-rose-100/90">readDir</code> / main-process
                    sandboxing — not a generic placeholder. Try Open folder… again or check the main log.
                  </p>
                </div>
              ) : fileTree && fileTree.length > 0 ? (
                renderTree(fileTree)
              ) : (
                <p className="text-gray-500 text-xs px-2 py-2">
                  No files shown — the project folder is empty or every entry is filtered (ignored names like{' '}
                  <span className="text-gray-400">node_modules</span>, <span className="text-gray-400">.git</span>). Add
                  files or pick another folder.
                </p>
              )}
            </>
          ) : (
            <div className="text-gray-500 text-xs px-2 py-2 space-y-2">
              <p>
                M03 — No project open. Next: click <span className="text-gray-400">Open folder…</span> or use the
                palette <kbd className="bg-gray-800 px-1 rounded text-[10px]">Ctrl+O</kbd>.
              </p>
              {lastRoot ? (
                <button
                  type="button"
                  onClick={() => reopenLastProject()}
                  className={`block w-full text-left text-[11px] text-cyan-300 hover:text-cyan-200 border border-gray-700 rounded px-2 py-1.5 ${focusVisibleRing}`}
                  title="M02 — Reopens the last folder you opened in this shell (stored locally only)."
                >
                  Open last project: {workspaceFolderBasename(lastRoot) || '…'}
                </button>
              ) : null}
              <p className="text-[10px] text-gray-600">
                M06 — Files load through Electron IPC; only paths under the chosen root are readable. Does not bypass
                workspace sandbox.
              </p>
            </div>
          )}
          <MinimalSurfaceM814Footer
            surfaceId="sidebar"
            offlineHint="Tree works offline once a folder is open (local FS via IPC)."
            docPath={MINIMALISTIC_DOC}
            m13Hint="Dev: noisyConsole logs [sidebar] expand/open/refresh."
          >
            <div className="flex flex-wrap gap-1 items-center px-2 pb-1">
              <CopySupportLineButton
                getText={() =>
                  `[sidebar] active_rel=${activeFile?.path ? workspaceRelativePath(activeFile.path, projectRoot) : 'none'} root=${workspaceFolderBasename(projectRoot) || 'none'}`
                }
                label="Copy support line"
                className={`text-[9px] px-1.5 py-0.5 rounded border border-gray-700 text-gray-400 hover:text-gray-200 ${focusVisibleRing}`}
                onCopied={() => {
                  pushToast({ title: '[sidebar]', message: 'Support line copied.', variant: 'success', durationMs: 2000 });
                  setStatusLine('[sidebar] support line copied');
                }}
                onFailed={() =>
                  pushToast({ title: '[sidebar]', message: 'Clipboard unavailable.', variant: 'warn', durationMs: 2600 })
                }
              />
            </div>
          </MinimalSurfaceM814Footer>
        </div>
      )}
    </div>
  );
};

export default Sidebar;
