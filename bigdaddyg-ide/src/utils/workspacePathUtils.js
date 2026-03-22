/** Repo-root-relative path to the M01–M07 polish checklist (for tooltips / footers). */
export const MINIMALISTIC_DOC = 'docs/MINIMALISTIC_7_ENHANCEMENTS.md';

/**
 * Workspace-relative path for display/tooltips (prefer showing basename or path under project root).
 */
export function workspaceRelativePath(absolutePath, projectRoot) {
  if (absolutePath == null || absolutePath === '') return '';
  const abs = String(absolutePath).replace(/\\/g, '/');
  const root = projectRoot ? String(projectRoot).replace(/\\/g, '/').replace(/\/$/, '') : '';
  const normAbs = abs.replace(/\/$/, '');
  if (root && (normAbs === root || normAbs.startsWith(`${root}/`))) {
    const rel = normAbs.slice(root.length).replace(/^\//, '');
    return rel || '.';
  }
  const parts = normAbs.split('/').filter(Boolean);
  return parts.length ? parts[parts.length - 1] : normAbs;
}

export function workspaceFolderBasename(projectRoot) {
  if (!projectRoot) return '';
  const s = String(projectRoot).replace(/\\/g, '/').replace(/\/$/, '');
  const i = s.lastIndexOf('/');
  return i >= 0 ? s.slice(i + 1) : s;
}
