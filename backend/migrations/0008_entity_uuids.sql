-- Migration 0008 — entity UUIDs (Phase D of the sync rework).
--
-- Per Q1 in the sync handoff, cats and users get a per-row UUID
-- as a stable identity that's decoupled from `slot_id` (which is
-- a device-local rendering slot, capped at 4, and reused after
-- delete on the firmware side). Without UUIDs, two unpaired
-- devices each adding a cat at slot 0 would collide on first
-- sync — one cat would silently lose to the other under LWW.
--
-- Storage: 32-char lowercase hex (16 random bytes). Cheap to
-- compare, fits any URL, and 2^128 is well past collision risk
-- for the population we'll ever ship.
--
-- Migration shape:
--   1. ALTER TABLE ADD COLUMN uuid TEXT (no NOT NULL — backfill
--      runs separately so SQLite doesn't reject the ADD)
--   2. UPDATE rows where uuid IS NULL with hex(randomblob(16))
--   3. CREATE UNIQUE INDEX so subsequent writes are dedup'd
--      against the new identity
--
-- Idempotency: ADD COLUMN is NOT idempotent in SQLite — re-running
-- this migration on an already-applied DB errors with "duplicate
-- column name". The deploy script tolerates that. UPDATE is
-- idempotent (only touches rows where uuid IS NULL). CREATE INDEX
-- IF NOT EXISTS is idempotent.
--
-- Backwards compat:
--   - Pre-Phase-D devices keep sending entities WITHOUT uuid.
--     The sync merge engine falls back to (hid, slot_id) lookup
--     and the response includes the row's now-backfilled uuid;
--     the device persists it on the next sync.
--   - Pre-Phase-D webapp code reading /api/cats etc. keeps working
--     because the response gains a uuid field but doesn't drop
--     anything. UI code can ignore it until it wants to use it.

ALTER TABLE cats  ADD COLUMN uuid TEXT;
ALTER TABLE users ADD COLUMN uuid TEXT;

UPDATE cats  SET uuid = lower(hex(randomblob(16))) WHERE uuid IS NULL OR uuid = '';
UPDATE users SET uuid = lower(hex(randomblob(16))) WHERE uuid IS NULL OR uuid = '';

CREATE UNIQUE INDEX IF NOT EXISTS idx_cats_uuid  ON cats(uuid);
CREATE UNIQUE INDEX IF NOT EXISTS idx_users_uuid ON users(uuid);
