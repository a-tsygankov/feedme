# feedme web app

Phone-friendly web UI for FeedMe. Talks to the same Cloudflare Worker
the firmware uses; auth is PIN-based per household. PWA-ready
scaffolding (manifest + theme-color) with React + Vite + TypeScript.

## Stack

- **Vite 7** + **React 19** + **TypeScript**
- **react-router-dom 7** for in-app navigation
- No state library — `useState` + `localStorage` for the bearer token
- Native `fetch` via a small `lib/api.ts` wrapper

## Running locally

```sh
npm install
npm run dev          # vite dev server on :5173
```

The dev server proxies `/api/*` to the deployed Worker so the
browser doesn't fight CORS during development. Override the target
by setting `VITE_API_BASE` in `.env.local` if you're running
`wrangler dev` against a local D1.

## Building for production

```sh
npm run build        # → dist/
```

Static output. Drop in any host (Cloudflare Pages, GitHub Pages,
Netlify) — the SPA falls back to `index.html` for unknown routes.

For Cloudflare Pages specifically:
```sh
npx wrangler pages deploy dist --project-name feedme
```

## Architecture

```
src/
├── App.tsx              top-level router + tab bar (auth-gated)
├── main.tsx             React mount + BrowserRouter
├── styles.css           theme primitives (mirrors firmware Aubergine)
├── lib/
│   ├── api.ts           fetch wrapper, ApiError, typed endpoints
│   └── palette.ts       cat / user colour palettes (mirror firmware)
└── views/
    ├── LoginPage.tsx    PIN-protected login + first-time setup
    ├── HomePage.tsx     dashboard (cats summary)
    ├── CatsPage.tsx     CRUD: rename, recolor, portion, add, remove
    ├── UsersPage.tsx    CRUD: rename, recolor, add, remove
    └── SettingsPage.tsx household ID, sign-out (more coming)
```

## Auth

- Token issued by the Worker (`/api/auth/login`, `/api/auth/setup`)
  is HMAC-SHA256 signed with `AUTH_SECRET`.
- Stored in `localStorage` under `feedme.token` and sent as
  `Authorization: Bearer <token>` on every authed request.
- 30-day TTL; `ApiError` with status 401 → router bounces to
  `/login`.

## Backend prerequisites

This app talks to the Worker in `../backend`. Before first deploy:

```sh
cd ../backend

# Apply schema. For a brand-new D1, schema.sql is enough:
npm run db:apply:remote

# For an existing D1 that pre-dates the web app, run migrations
# in order (errors on already-applied steps are expected and safe):
npm run db:migrate:0001:remote   # legacy event columns
npm run db:migrate:0002:remote   # web-app tables

# One-time: set the auth signing secret:
npx wrangler secret put AUTH_SECRET
# (paste a long random string)

npm run deploy
```

## Coming soon

- Statistics dashboard (per-cat / per-user / per-day breakdowns)
- Push notifications opt-in
- Change PIN
- Cat slug / pose picker
- Live "fed N min ago" + quick-feed buttons on home
