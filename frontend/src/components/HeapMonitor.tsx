import React, { useEffect, useState } from 'react';

export const HeapMonitor: React.FC = () => {
  const [heap, setHeap] = useState<number | null>(null);

  useEffect(() => {
    const interval = setInterval(() => {
      // @ts-expect-error Chromium exposes a non-standard performance.memory API.
      const memory = window.performance?.memory;
      if (memory && typeof memory.usedJSHeapSize === 'number') {
        setHeap(Math.round((memory.usedJSHeapSize / 1024 / 1024) * 100) / 100);
      }
    }, 2000);

    return () => clearInterval(interval);
  }, []);

  if (heap === null) {
    return null;
  }

  return <div className="heap-monitor">Heap: {heap.toFixed(2)} MB</div>;
};