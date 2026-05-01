-- Migration 0004 — split device identity from home identity.
--
-- Before this migration the firmware's auto-generated `feedme-{mac6}`
-- identifier WAS the household's primary key. That conflated two
-- concepts:
--   - device identity   (the physical knob — feedme-mac6)
--   - home identity     (the family using it — "Smith Family")
--
-- This migration introduces `devices`, a many-to-one mapping from
-- device id (the firmware's hid) to home id (households.hid, which
-- now means the user-chosen home name). After this migration:
--   - households.hid     = user-chosen home name (e.g. "Smith Family")
--   - devices.device_id  = firmware's auto-generated id (feedme-mac6)
--   - devices.home_hid   = which household this device pairs into
--
-- Backwards-compat seed: every existing household assumed device_id ==
-- home_hid (the single-device-per-home model). The INSERT below
-- registers each existing household as its own device so the
-- firmware-facing endpoints (/api/feed, /api/state, /api/history)
-- continue to find their home via the same lookup path. New homes
-- created via the webapp will register their paired device explicitly
-- in /api/auth/setup.

CREATE TABLE IF NOT EXISTS devices (
  device_id TEXT PRIMARY KEY,        -- firmware's auto-generated hid
  home_hid  TEXT NOT NULL,           -- references households.hid
  joined_at INTEGER NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_devices_home_hid ON devices(home_hid);

-- Seed from existing households so legacy data Just Works.
-- INSERT OR IGNORE makes this idempotent (safe to re-run; rows that
-- already exist from a previous migration run are left untouched).
INSERT OR IGNORE INTO devices (device_id, home_hid, joined_at)
SELECT hid, hid, created_at FROM households;
