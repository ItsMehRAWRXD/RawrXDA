self.addEventListener('install', (event) => {
  event.waitUntil(self.skipWaiting());
});

self.addEventListener('activate', (event) => {
  event.waitUntil(self.clients.claim());
});

// Network-first passthrough (no caching) to avoid stale UI.
self.addEventListener('fetch', (event) => {
  event.respondWith(fetch(event.request));
});

