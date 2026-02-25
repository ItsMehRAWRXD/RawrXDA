import React, { createContext, useContext, useState, useCallback } from 'react';

const ProjectContext = createContext(null);

function getLanguage(name) {
  const ext = (name || '').split('.').pop()?.toLowerCase() || '';
  const map = {
    js: 'javascript', jsx: 'javascript', ts: 'typescript', tsx: 'typescript',
    py: 'python', cpp: 'cpp', cc: 'cpp', cxx: 'cpp', h: 'cpp', hpp: 'cpp', c: 'c',
    json: 'json', md: 'markdown', html: 'html', css: 'css', rs: 'rust', go: 'go',
    asm: 'asm', masm: 'asm'
  };
  return map[ext] || 'plaintext';
}

function buildTree(entries, basePath) {
  if (!entries || !entries.length) return [];
  const nodes = [];
  const byName = {};
  entries.forEach((e) => {
    const node = {
      name: e.name,
      path: e.path || `${basePath}/${e.name}`.replace(/\/+/g, '/'),
      isDirectory: e.isDirectory,
      children: e.isDirectory ? [] : undefined
    };
    byName[e.name] = node;
    nodes.push(node);
  });
  return nodes;
}

export function ProjectProvider({ children }) {
  const [projectRoot, setProjectRoot] = useState(null);
  const [files, setFiles] = useState([]);
  const [fileTree, setFileTree] = useState([]);
  const [activeFile, setActiveFileState] = useState(null);
  const [fileContentCache, setFileContentCache] = useState({});

  const openProject = useCallback(async (pathFromDialog) => {
    const api = window.electronAPI;
    if (!api?.openProject && !pathFromDialog) return;
    const result = pathFromDialog
      ? { success: true, data: pathFromDialog }
      : await api.openProject();
    if (!result?.success) return;
    const root = result.data;
    if (!root) return;
    setProjectRoot(root);
    setFiles([]);
    setFileTree([]);
    setActiveFileState(null);
    setFileContentCache({});
    if (api.readDir) {
      const dirResult = await api.readDir(root);
      if (dirResult?.success && dirResult.data?.length) {
        const tree = buildTree(dirResult.data, root);
        setFileTree(tree);
        const flatFiles = (dirResult.data || [])
          .filter((e) => !e.isDirectory)
          .map((e) => ({
            path: e.path,
            name: e.name,
            language: getLanguage(e.name)
          }));
        setFiles(flatFiles);
      }
    }
  }, []);

  const openFile = useCallback(
    async (fileNode) => {
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
    },
    []
  );

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
    files,
    fileTree,
    activeFile,
    fileContentCache,
    openProject,
    openFile,
    updateFile,
    setActiveFile
  };

  return <ProjectContext.Provider value={value}>{children}</ProjectContext.Provider>;
}

export function useProject() {
  const ctx = useContext(ProjectContext);
  if (!ctx) throw new Error('useProject must be used within ProjectProvider');
  return ctx;
}

export default ProjectContext;
