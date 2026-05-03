-- Migration 0007 — add schedule_hours to cats.
--
-- The firmware's CatRoster carries a 4-slot MealSchedule per cat
-- (Breakfast / Lunch / Dinner / Treat hour-of-day). Up until now
-- those values lived only in NVS — the backend had no column for
-- them. Sync needs them server-side so the LWW merge can round-trip
-- the firmware's local edits. Per Q2 in the sync handoff we keep
-- schedule embedded in the cat row rather than promote it to its
-- own table; the cat's updated_at bumps when any slot changes.
--
-- Stored as a JSON-serialised array (e.g. "[7,12,18,21]") to keep
-- the schema flat and avoid a 4-column UPDATE on every nudge of
-- a single hour. Default matches the firmware's MealSchedule
-- factory defaults so existing rows render sensibly until the
-- device's first sync overwrites them with NVS truth.

ALTER TABLE cats ADD COLUMN schedule_hours TEXT NOT NULL DEFAULT '[7,12,18,21]';
