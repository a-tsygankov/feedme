-- Migration 0005 — universal entity timestamps + tombstone flag.
--
-- Adds the LWW-conflict-resolution triplet to every persisted
-- entity that participates in sync:
--   - created_at INTEGER  (immutable; first-write wall-clock)
--   - updated_at INTEGER  (bumped on every UPDATE; drives LWW)
--   - is_deleted INTEGER  (0/1; tombstone — webapp + firmware filter
--                          on `is_deleted = 0` for active sets)
--
-- Backfill strategy for existing rows:
--   - households.created_at already exists (Phase 2.4) → reuse as-is
--     for both created_at and updated_at; mark is_deleted = 0.
--   - cats / users have a `deleted_at` column from migration 0002.
--     Map: created_at = (today's epoch as a sentinel; we can't recover
--                       the real first-write time)
--          updated_at = COALESCE(deleted_at, sentinel)
--          is_deleted = (deleted_at IS NOT NULL)
--     deleted_at stays in the schema for now; new code reads is_deleted.
--   - devices.joined_at already exists (Phase B/dev-19) → reuse as
--     both created_at and updated_at; is_deleted = 0.
--
-- Idempotency: SQLite ALTER TABLE ADD COLUMN doesn't support
-- IF NOT EXISTS, so re-runs of this file error with "duplicate column
-- name". The deploy script catches the failure and continues.

-- ── households ────────────────────────────────────────────────────
ALTER TABLE households ADD COLUMN updated_at INTEGER NOT NULL DEFAULT 0;
ALTER TABLE households ADD COLUMN is_deleted INTEGER NOT NULL DEFAULT 0;
UPDATE households SET updated_at = created_at WHERE updated_at = 0;

-- ── cats ──────────────────────────────────────────────────────────
-- created_at is brand new. Set to a sentinel (1735689600 = 2025-01-01
-- UTC, well before any real shipped device) so any real updates win
-- LWW even if the device's clock is way off. updated_at follows
-- deleted_at if soft-deleted, otherwise the same sentinel.
ALTER TABLE cats ADD COLUMN created_at INTEGER NOT NULL DEFAULT 1735689600;
ALTER TABLE cats ADD COLUMN updated_at INTEGER NOT NULL DEFAULT 1735689600;
ALTER TABLE cats ADD COLUMN is_deleted INTEGER NOT NULL DEFAULT 0;
UPDATE cats SET updated_at = deleted_at, is_deleted = 1
 WHERE deleted_at IS NOT NULL;

-- ── users ─────────────────────────────────────────────────────────
ALTER TABLE users ADD COLUMN created_at INTEGER NOT NULL DEFAULT 1735689600;
ALTER TABLE users ADD COLUMN updated_at INTEGER NOT NULL DEFAULT 1735689600;
ALTER TABLE users ADD COLUMN is_deleted INTEGER NOT NULL DEFAULT 0;
UPDATE users SET updated_at = deleted_at, is_deleted = 1
 WHERE deleted_at IS NOT NULL;

-- ── devices ───────────────────────────────────────────────────────
ALTER TABLE devices ADD COLUMN created_at INTEGER NOT NULL DEFAULT 0;
ALTER TABLE devices ADD COLUMN updated_at INTEGER NOT NULL DEFAULT 0;
ALTER TABLE devices ADD COLUMN is_deleted INTEGER NOT NULL DEFAULT 0;
UPDATE devices SET created_at = joined_at, updated_at = joined_at
 WHERE created_at = 0;
