-- Migration 0011 — auth + pair audit log.
--
-- TEMPORARY (per user request): a per-home audit trail of every
-- pairing handshake, login attempt, login-QR exchange, and set-pin
-- call. Supplements sync_logs (which only tracks /api/sync calls).
-- Lets the user inspect WHY a pairing flow failed without opening a
-- terminal to grep Worker logs.
--
-- Same ring-buffer-of-100 retention pattern as sync_logs (the helper
-- in audit.ts prunes after each insert).
--
-- Idempotent: CREATE TABLE IF NOT EXISTS + CREATE INDEX IF NOT EXISTS.

CREATE TABLE IF NOT EXISTS auth_logs (
  id            INTEGER PRIMARY KEY AUTOINCREMENT,
  home_hid      TEXT,                 -- nullable: pre-auth events (login attempt for unknown home, etc) carry no hid
  ts            INTEGER NOT NULL,
  kind          TEXT NOT NULL,        -- "pair-start" / "pair-confirm" / "login" / "setup" / "quick-setup" / "login-qr" / "login-token-create" / "set-pin"
  identifier    TEXT,                 -- deviceId for pair flows, hid for login attempts, etc.
  result        TEXT NOT NULL,        -- "ok" | "error"
  error_message TEXT,                 -- non-null when result='error'; mirrors the response body's error
  duration_ms   INTEGER NOT NULL DEFAULT 0
);
CREATE INDEX IF NOT EXISTS idx_auth_logs_home_ts ON auth_logs(home_hid, ts DESC);
CREATE INDEX IF NOT EXISTS idx_auth_logs_ts      ON auth_logs(ts DESC);
