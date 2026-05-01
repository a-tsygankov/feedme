// Worker bindings + secrets shared across modules. Defined here (not in
// index.ts) so cats.ts / users.ts / auth.ts can import the type without
// circular dependencies on the router.

export interface Env {
  // D1 database binding from wrangler.toml.
  DB: D1Database;
  // HMAC secret for session-token signing. Set via:
  //   wrangler secret put AUTH_SECRET
  // For local dev (`wrangler dev`), drop a value into .dev.vars:
  //   AUTH_SECRET=dev-only-do-not-ship-replace-me
  // The auth module falls back to a hard-coded dev string with a
  // loud console warning when this is absent so we don't silently
  // sign with empty bytes in production.
  AUTH_SECRET?: string;
}
