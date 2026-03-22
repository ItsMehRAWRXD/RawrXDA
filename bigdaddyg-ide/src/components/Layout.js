import React from 'react';

/**
 * Two-column shell: project tree + main editor/dock stack.
 * Arranges panes only — does not persist project path, run agents, or execute terminal commands.
 */
const Layout = ({ sidebar, main }) => {
  return (
    <div
      className="flex flex-col h-full"
      title="Main workspace: project sidebar and editor/dock area. Layout only — does not stop running tasks or close buffers."
    >
      <div className="flex flex-1 overflow-hidden min-h-0">
        {sidebar}
        <div className="flex-1 flex flex-col min-w-0">
          {main}
        </div>
      </div>
    </div>
  );
};

export default Layout;
