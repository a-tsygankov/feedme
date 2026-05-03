-- Migration 0009 — per-home sync interval setting (Phase E).
--
-- Default 14400 sec (4h) matches the value the sync engine has been
-- hardcoded to since Phase B. Range enforcement (1..24h) is in the
-- backend handler — this column accepts whatever int is provided
-- and the read path doesn't clamp.
--
-- Why a column on households rather than a separate home_settings
-- table: there's exactly one int field worth of state right now and
-- denormalising into the home row keeps the read path single-query.
-- If/when home-level settings multiply (notification prefs, PIN
-- policy, etc.) extracting them is a future migration.
--
-- Idempotency: ALTER TABLE ADD COLUMN errors with "duplicate column
-- name" on re-run — safe to ignore (deploy script tolerates it).

ALTER TABLE households ADD COLUMN sync_interval_sec INTEGER NOT NULL DEFAULT 14400;
