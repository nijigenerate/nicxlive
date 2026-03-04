const CACHE_NAME = "nicxlive-wasm-static-v2";
const PRECACHE_URLS = [
  "/wasm/",
  "/wasm/index.html",
  "/wasm/manifest.webmanifest",
  "/wasm/sw.js",
  "/wasm/icons/icon-192.png",
  "/wasm/icons/icon-512.png",
  "/webgl_backend/webgl_backend.js",
  "/build-wasm-check/nicxlive.js",
  "/build-wasm-check/nicxlive.wasm",
];

self.addEventListener("install", (event) => {
  event.waitUntil((async () => {
    const cache = await caches.open(CACHE_NAME);
    const jobs = PRECACHE_URLS.map(async (url) => {
      try {
        await cache.add(new Request(url, { cache: "reload" }));
      } catch (_) {}
    });
    await Promise.all(jobs);
    await self.skipWaiting();
  })());
});

self.addEventListener("activate", (event) => {
  event.waitUntil((async () => {
    const keys = await caches.keys();
    await Promise.all(keys.map((key) => (key === CACHE_NAME ? Promise.resolve() : caches.delete(key))));
    await self.clients.claim();
  })());
});

function isStaticAssetPath(pathname) {
  if (pathname === "/wasm/" || pathname === "/wasm/index.html") return true;
  if (pathname === "/webgl_backend/webgl_backend.js") return true;
  if (pathname === "/build-wasm-check/nicxlive.js" || pathname === "/build-wasm-check/nicxlive.wasm") return true;
  if (pathname.startsWith("/wasm/icons/")) return true;
  if (pathname.endsWith(".js") || pathname.endsWith(".wasm") || pathname.endsWith(".css")) return true;
  if (pathname.endsWith(".webmanifest")) return true;
  return false;
}

self.addEventListener("fetch", (event) => {
  const req = event.request;
  if (req.method !== "GET") return;
  const url = new URL(req.url);
  if (url.origin !== self.location.origin) return;

  if (req.mode === "navigate") {
    event.respondWith((async () => {
      try {
        const net = await fetch(req);
        const cache = await caches.open(CACHE_NAME);
        cache.put("/wasm/index.html", net.clone()).catch(() => {});
        return net;
      } catch (_) {
        const cached = await caches.match("/wasm/index.html", { ignoreSearch: true });
        if (cached) return cached;
        throw _;
      }
    })());
    return;
  }

  if (!isStaticAssetPath(url.pathname)) return;

  event.respondWith((async () => {
    const cached = await caches.match(req, { ignoreSearch: true });
    if (cached) return cached;
    try {
      const net = await fetch(req);
      const cache = await caches.open(CACHE_NAME);
      cache.put(req, net.clone()).catch(() => {});
      return net;
    } catch (_) {
      const fallback = await caches.match(url.pathname, { ignoreSearch: true });
      if (fallback) return fallback;
      throw _;
    }
  })());
});
