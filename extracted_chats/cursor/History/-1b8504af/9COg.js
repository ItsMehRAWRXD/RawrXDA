/* autostart-shutdown.js
 * Graceful-shooter for every long-lived module.
 * Import this once in index.html BEFORE any worker/daemon scripts.
 */
(function () {
  const shutdownMap = new Map();           // name → async cleanup fn
  const SHUTDOWN_TIMEOUT  = 5000;          // max ms to wait per module
  let shuttingDown = false;

  window.registerShutdown = function (name, cleanupFn) {
    shutdownMap.set(name, cleanupFn);
  };

  async function runShutdown() {
    if (shuttingDown) return;
    shuttingDown = true;
    console.log('[shutdown] starting…');
    const results = [];
    for (const [name, fn] of shutdownMap) {
      try {
        const t0 = performance.now();
        await Promise.race([
          fn(),
          new Promise((_, rej) =>
            setTimeout(() => rej(new Error(`${name} timeout`)), SHUTDOWN_TIMEOUT)
          )
        ]);
        results.push({ name, ok: true, ms: performance.now() - t0 });
      } catch (e) {
        results.push({ name, ok: false, error: e.message });
      }
    }
    console.log('[shutdown] finished', results);
  }

  /* universal triggers */
  window.addEventListener('beforeunload', runShutdown);
  window.addEventListener('pagehide',  runShutdown);
  document.addEventListener('visibilitychange', () => {
    if (document.visibilityState === 'hidden') runShutdown();
  });

  /* manual trigger for dev console */
  window.emergencyShutdown = runShutdown;
})();


