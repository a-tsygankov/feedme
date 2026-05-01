-- Migration 0002 — household auth + cats + users tables for the
-- web/phone app (PR #19). All CREATE TABLE IF NOT EXISTS, so safely
-- re-runnable. Independent of the events table — applying this
-- migration on a database that hasn't run 0001 is fine; the firmware
-- still works against the legacy events shape.

CREATE TABLE IF NOT EXISTS households (
  hid        TEXT PRIMARY KEY,
  pin_salt   TEXT NOT NULL,
  pin_hash   TEXT NOT NULL,
  created_at INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS cats (
  hid                  TEXT NOT NULL,
  slot_id              INTEGER NOT NULL,
  name                 TEXT NOT NULL,
  color                INTEGER NOT NULL DEFAULT 0,
  slug                 TEXT NOT NULL DEFAULT 'C2',
  default_portion_g    INTEGER NOT NULL DEFAULT 40,
  hungry_threshold_sec INTEGER NOT NULL DEFAULT 18000,
  deleted_at           INTEGER,
  PRIMARY KEY (hid, slot_id)
);

CREATE TABLE IF NOT EXISTS users (
  hid        TEXT NOT NULL,
  slot_id    INTEGER NOT NULL,
  name       TEXT NOT NULL,
  color      INTEGER NOT NULL DEFAULT 0,
  deleted_at INTEGER,
  PRIMARY KEY (hid, slot_id)
);
