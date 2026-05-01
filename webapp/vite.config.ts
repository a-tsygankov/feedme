import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

// Web/phone app for feedme. Talks to the same Cloudflare Worker that
// the firmware uses; auth is PIN-based (per-household). Build output
// goes to `dist/` and can be served by any static host or Cloudflare
// Pages — `wrangler pages deploy dist` after a build.
export default defineConfig({
  plugins: [react()],
  // Dev server proxies /api/* to the deployed Worker so the browser
  // doesn't need to chase CORS during local development. Override via
  // VITE_API_BASE in `.env.local` if pointing at a local `wrangler dev`.
  server: {
    port: 5173,
    proxy: {
      "/api": {
        target: "https://feedme.atsyg-feedme.workers.dev",
        changeOrigin: true,
        secure: true,
      },
    },
  },
});
