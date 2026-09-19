// Keeps the game on the device between visits. Without it every start of the
// home-screen app fetches 13 MB again, and a start with no network lands on a
// browser error page inside a window that has no address bar to recover from.
// The cache name carries a build stamp, so a new deploy installs beside the old
// one and then replaces it.
const CACHE = 'snapszer-__CACHE_VERSION__';
const ASSETS = [
    './',
    'index.html',
    'snapszer.js',
    'snapszer.wasm',
    'qtloader.js',
    'manifest.webmanifest',
    'icon-180.png',
    'icon-192.png',
    'icon-512.png',
    'LICENSE-GPL-3.0.txt',
    'LICENSE-MIT.txt',
];

self.addEventListener('install', event => {
    event.waitUntil(
        caches.open(CACHE)
            .then(cache => cache.addAll(ASSETS))
            .then(() => self.skipWaiting()));
});

self.addEventListener('activate', event => {
    event.waitUntil(
        caches.keys()
            .then(names => Promise.all(names.filter(n => n !== CACHE).map(n => caches.delete(n))))
            .then(() => self.clients.claim()));
});

self.addEventListener('fetch', event => {
    if (event.request.method !== 'GET')
        return;
    if (new URL(event.request.url).origin !== self.location.origin)
        return;
    event.respondWith(
        caches.match(event.request, { ignoreSearch: true })
            .then(hit => hit || fetch(event.request).then(response => {
                if (response.ok) {
                    const copy = response.clone();
                    caches.open(CACHE).then(cache => cache.put(event.request, copy));
                }
                return response;
            })));
});
