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
# Run from the webapp/ root so Pages picks up the functions/ folder
# (the /api/* → Worker proxy) alongside dist/.
npx wrangler pages deploy dist --project-name feedme-webapp
```

**Why the proxy?** Pages serves static files; it doesn't know how to
forward POST `/api/auth/login` to your Worker. `functions/api/[[path]].ts`
is a tiny Pages Function that catches every `/api/*` request and
re-fetches it against the Worker (default
`https://feedme.atsyg-feedme.workers.dev`). Set `WORKER_ORIGIN` in the
Pages project's env vars to point at a staging Worker. Without this
file, the live site returns **405 Method Not Allowed** on every API
call — the dev server hides this because Vite proxies `/api/*` itself.

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

## Pairing flow

The device auto-generates its household identifier on first boot —
`feedme-{12-hex-mac}` (e.g. `feedme-a8b3c1d4e5f6`). It then displays
that ID + a QR code on a one-time pairing screen. The QR encodes:

```
https://feedme-webapp.pages.dev/setup?hid=feedme-a8b3c1d4e5f6
```

Scanning the QR with any phone lands on `/setup`, which probes
`/api/auth/exists` and either:

- `new` → shows a "Set a PIN" form. On submit creates the household
  and signs the user in — pairing complete.
- `exists` → offers "Continue to sign in" with the hid pre-filled
  (probably someone re-scanned the QR after the household was paired).

**Lost the PIN / starting over?** Long-press the QR screen on the
device. A confirm screen pops up; one tap rotates the hid (counter
appended: `feedme-a8b3c1d4e5f6-1`), wipes the NVS paired flag, and
reboots. The new QR points at a fresh, unclaimed household ID. The
old backend household record is orphaned — clean it up manually with
the "Forget this household" button under Settings (or via wrangler).

**Forget remotely?** Settings → Forget this household calls
`DELETE /api/auth/household`, which wipes the household + cats + users
rows for the signed-in hid. Feed history stays (orphaned, harmless).
After deletion the user is signed out and bounced to `/login`.

## Coming soon

- Statistics dashboard (per-cat / per-user / per-day breakdowns)
- Push notifications opt-in
- Change PIN
- Cat slug / pose picker
- Live "fed N min ago" + quick-feed buttons on home
