import React from 'react';

const Layout = ({ sidebar, main }) => {
  return (
    <div className="flex flex-col h-full">
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
