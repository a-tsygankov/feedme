-- Migration 0006 — sync support tables.
--
-- Three new tables for the multi-device sync rework. All
-- CREATE TABLE IF NOT EXISTS so this migration is idempotent and
-- safe to re-run. See docs/sync-implementation-handoff.md §4.2 for
-- the full design rationale.

-- ── pending_pairings ──────────────────────────────────────────────
-- Short-lived (3-min TTL) handshake state. The device creates a row
-- via POST /api/pair/start when it shows the QR + "Pairing..." screen.
-- The user creates the home OR signs in to an existing one in the
-- webapp, then clicks "Confirm pairing" — that POST /api/pair/confirm
-- writes home_hid + confirmed_at + a freshly-issued device_token to
-- this row. The device's polling GET /api/pair/check picks up the
-- token on its next 15s poll and stores it in NVS.
--
-- After collection (or after expires_at passes), the row may be GC'd
-- by a cron purge — but harmless if we don't, the rows are tiny.
CREATE TABLE IF NOT EXISTS pending_pairings (
  device_id     TEXT PRIMARY KEY,
  requested_at  INTEGER NOT NULL,
  expires_at    INTEGER NOT NULL,         -- requested_at + 180
  home_hid      TEXT,                     -- NULL until confirmed
  confirmed_at  INTEGER,                  -- NULL until confirmed
  device_token  TEXT,                     -- NULL until confirmed
  cancelled_at  INTEGER                   -- NULL unless user/device cancelled
);

-- ── pairings ──────────────────────────────────────────────────────
-- The persistent device↔home relationship. A device has at most one
-- *active* pairing (is_deleted = 0); a home has many (one row per
-- paired device). Compound uniqueness is enforced in code rather
-- than at the DB level — partial-unique-index on SQLite works but
-- adds churn we don't need.
--
-- created_at = first time this device joined this home.
-- updated_at = bumped if the same device re-pairs after a soft-delete
--              (Reset → re-pair without rotating the device id).
-- is_deleted = 1 when the device-side Reset or webapp-side Forget
--              tears the relationship down. We never hard-delete so
--              the sync_logs that reference this pairing stay valid.
CREATE TABLE IF NOT EXISTS pairings (
  id            INTEGER PRIMARY KEY AUTOINCREMENT,
  device_id     TEXT NOT NULL,
  home_hid      TEXT NOT NULL,
  created_at    INTEGER NOT NULL,
  updated_at    INTEGER NOT NULL,
  is_deleted    INTEGER NOT NULL DEFAULT 0
);
CREATE INDEX IF NOT EXISTS idx_pairings_device  ON pairings(device_id);
CREATE INDEX IF NOT EXISTS idx_pairings_home    ON pairings(home_hid);

-- ── sync_logs ─────────────────────────────────────────────────────
-- Audit log: one row per sync attempt (success or failure), per
-- home + per device. Capped to last 100 per home by an after-insert
-- prune (done in the Worker code, not via a trigger — D1 triggers
-- have caveats and the prune is a single DELETE).
--
-- Counts:
--   entities_in   how many rows the device uploaded
--   entities_out  how many rows the server returned (after merge)
--   conflicts     # rows where server's updated_at > client's by ≥5s
--                 (heuristic for "another device wrote between syncs")
CREATE TABLE IF NOT EXISTS sync_logs (
  id              INTEGER PRIMARY KEY AUTOINCREMENT,
  home_hid        TEXT NOT NULL,
  device_id       TEXT NOT NULL,
  ts              INTEGER NOT NULL,
  result          TEXT NOT NULL,            -- 'ok' | 'error' | 'cancelled'
  error_message   TEXT,
  entities_in     INTEGER NOT NULL DEFAULT 0,
  entities_out    INTEGER NOT NULL DEFAULT 0,
  conflicts       INTEGER NOT NULL DEFAULT 0,
  duration_ms     INTEGER NOT NULL DEFAULT 0
);
CREATE INDEX IF NOT EXISTS idx_sync_logs_home_ts ON sync_logs(home_hid, ts DESC);
