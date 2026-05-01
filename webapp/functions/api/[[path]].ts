// Cloudflare Pages Function: transparent proxy for /api/* → the Worker.
//
// Why: the SPA build is hosted on Pages, but the API lives on a
// separate Worker (feedme.atsyg-feedme.workers.dev). Without this
// shim the browser tries to POST /api/auth/exists at the Pages
// origin and gets 405 (Pages refuses POST on static assets). With
// this shim Pages forwards the request server-side, keeping the
// whole webapp single-origin (no CORS preflight noise either).
//
// Override the upstream by setting WORKER_ORIGIN as a Pages env var
// (Production/Preview) — handy for staging Workers or `wrangler dev`.

const DEFAULT_WORKER_ORIGIN = "https://feedme.atsyg-feedme.workers.dev";

interface Env {
  WORKER_ORIGIN?: string;
}

export const onRequest: PagesFunction<Env> = async ({ request, env }) => {
  const origin = (env.WORKER_ORIGIN ?? DEFAULT_WORKER_ORIGIN).replace(/\/+$/, "");
  const incoming = new URL(request.url);
  const upstream = origin + incoming.pathname + incoming.search;

  // Forward the request body / method / headers verbatim. The Worker
  // already sets CORS headers, so we don't need to touch them.
  const init: RequestInit = {
    method: request.method,
    headers: request.headers,
    body: ["GET", "HEAD"].includes(request.method) ? undefined : request.body,
    // Required when streaming a request body in workerd-compatible runtimes.
    // @ts-expect-error duplex is a valid fetch init key in workerd.
    duplex: "half",
    redirect: "manual",
  };

  return fetch(upstream, init);
};
