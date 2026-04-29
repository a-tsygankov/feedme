CREATE TABLE IF NOT EXISTS events (
  id    INTEGER PRIMARY KEY AUTOINCREMENT,
  hid   TEXT NOT NULL,
  ts    INTEGER NOT NULL,
  type  TEXT NOT NULL,
  by    TEXT NOT NULL,
  note  TEXT,
  -- Per-cat scope. 'primary' = the household's first cat (the silent
  -- default for single-cat households). When the firmware learns to
  -- pass per-cat IDs (currently single-cat across the wire), values
  -- will be the stable Cat::id from CatRoster.
  cat   TEXT NOT NULL DEFAULT 'primary'
);
CREATE INDEX IF NOT EXISTS idx_hid_ts     ON events(hid, ts DESC);
CREATE INDEX IF NOT EXISTS idx_hid_cat_ts ON events(hid, cat, ts DESC);

-- ── Migration for existing databases ──────────────────────────────
-- For a fresh `wrangler d1 execute feedme --remote --file=./schema.sql`,
-- the CREATE above already includes `cat`. For databases provisioned
-- before this column existed, run:
--
--   wrangler d1 execute feedme --remote \
--     --command="ALTER TABLE events ADD COLUMN cat TEXT NOT NULL DEFAULT 'primary'"
--
-- D1's ALTER TABLE supports ADD COLUMN with a DEFAULT.
