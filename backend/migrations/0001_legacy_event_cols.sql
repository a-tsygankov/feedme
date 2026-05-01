-- Migration 0001 — add `cat` and `event_id` columns to events.
--
-- Pre-Phase-2 databases were created when events had only id, hid,
-- ts, type, by, note. The web app's schema.sql now includes these
-- columns in the CREATE statement, but old DBs miss them — and
-- schema.sql's `CREATE UNIQUE INDEX idx_event_id ON events(event_id)`
-- chokes on them ("no such column: event_id").
--
-- This file brings such databases up to current. Already-applied
-- steps will error with "duplicate column name" — that's fine,
-- skip and continue.

ALTER TABLE events ADD COLUMN cat      TEXT NOT NULL DEFAULT 'primary';
ALTER TABLE events ADD COLUMN event_id TEXT;

CREATE INDEX        IF NOT EXISTS idx_hid_cat_ts ON events(hid, cat, ts DESC);
CREATE UNIQUE INDEX IF NOT EXISTS idx_event_id   ON events(event_id);
