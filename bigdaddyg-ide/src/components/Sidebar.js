import React from 'react';

const Sidebar = ({
  isOpen,
  onToggle,
  projectRoot,
  fileTree,
  activeFile,
  onOpenFolder,
  onOpenFile
}) => {
  const width = isOpen ? 260 : 48;

  const renderTree = (items, depth = 0) => {
    if (!items || !items.length) return null;
    return (
      <ul className="list-none pl-2" style={{ paddingLeft: depth * 12 }}>
        {items.map((item) => (
          <li key={item.path || item.name}>
            {item.isDirectory ? (
              <span className="text-gray-400 block py-0.5 text-xs">{item.name}/</span>
            ) : (
              <button
                type="button"
                onClick={() => onOpenFile && onOpenFile(item)}
                className={`block w-full text-left py-1 px-1 rounded text-xs truncate ${
                  activeFile?.path === item.path
                    ? 'bg-ide-accent text-white'
                    : 'text-gray-300 hover:bg-gray-700'
                }`}
              >
                {item.name}
              </button>
            )}
            {item.children?.length ? renderTree(item.children, depth + 1) : null}
          </li>
        ))}
      </ul>
    );
  };

  return (
    <div
      className="flex flex-col bg-ide-sidebar border-r border-gray-700 flex-shrink-0"
      style={{ width }}
    >
      <div className="flex items-center justify-between p-2 border-b border-gray-700 min-h-[40px]">
        {isOpen && (
          <button
            type="button"
            onClick={onOpenFolder}
            className="text-xs text-ide-accent hover:underline"
          >
            Open folder
          </button>
        )}
        <button
          type="button"
          onClick={onToggle}
          className="p-1 rounded hover:bg-gray-700 text-gray-400"
          aria-label={isOpen ? 'Collapse sidebar' : 'Expand sidebar'}
        >
          {isOpen ? '\u2039' : '\u203A'}
        </button>
      </div>
      {isOpen && (
        <div className="flex-1 overflow-auto py-2">
          {projectRoot ? (
            <>
              <div className="text-xs text-gray-500 px-2 truncate" title={projectRoot}>
                {projectRoot.replace(/^.*[\\/]/, '') || projectRoot}
              </div>
              {fileTree && fileTree.length > 0 ? (
                renderTree(fileTree)
              ) : (
                <p className="text-gray-500 text-xs px-2 py-2">No files listed</p>
              )}
            </>
          ) : (
            <p className="text-gray-500 text-xs px-2 py-2">Open a folder to browse files</p>
          )}
        </div>
      )}
    </div>
  );
};

export default Sidebar;
