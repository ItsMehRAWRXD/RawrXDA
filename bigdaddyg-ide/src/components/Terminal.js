import React from 'react';

const Terminal = () => {
  return (
    <div className="h-32 flex flex-col bg-ide-bg border-t border-gray-700">
      <div className="px-3 py-1.5 bg-ide-toolbar border-b border-gray-700 text-xs font-medium text-gray-400">
        Terminal
      </div>
      <div className="flex-1 p-3 text-gray-500 text-sm overflow-auto font-mono">
        node-pty integration will be added in a future update. Run external commands from your system terminal for now.
      </div>
    </div>
  );
};

export default Terminal;
