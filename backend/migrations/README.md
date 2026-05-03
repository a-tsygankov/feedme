# Migrations

Numbered SQL files that run in order. Each migration is **idempotent
where possible** (uses `IF NOT EXISTS`) but a few statements
(`ALTER TABLE ADD COLUMN`) are not — those will error harmlessly on
databases where they've already been applied.

Run them in order against `--local` or `--remote`:

```sh
# Apply ALL migrations (safe to re-run; failed ALTERs on already-
# applied steps are visible in the output but don't corrupt state):
for f in migrations/*.sql; do
  npx wrangler d1 execute feedme --remote --file="$f"
done

# Or apply one by one:
npm run db:apply:remote -- --file=./migrations/0001_legacy_event_cols.sql
npm run db:apply:remote -- --file=./migrations/0002_webapp_tables.sql
```

## Files

- **`0001_legacy_event_cols.sql`** — adds `cat` and `event_id`
  columns to `events`, plus the unique index on `event_id`.
  No-op for fresh databases (the canonical `schema.sql` already
  includes these). Errors on databases where it was already
  applied (the ALTER TABLE statements). Safe to ignore those
  errors — the columns are already there.

- **`0002_webapp_tables.sql`** — adds `households`, `cats`, and
  `users` for the web-app PIN auth + management surface
  (PR #19). All `CREATE TABLE IF NOT EXISTS` so safely re-runnable.

- **`0003_household_name.sql`** — adds `households.name` (friendly
  display label like "Smith Family"). `ALTER TABLE ADD COLUMN`,
  errors with "duplicate column name" if already applied — safe to
  ignore, the deploy script catches and continues. **Note:** after
  migration 0004 `households.hid` IS the home name; the `name`
  column is unused by current code (kept around so we don't have to
  drop a column).

- **`0004_devices.sql`** — splits device identity (firmware's
  `feedme-{mac6}` hid) from home identity (user-chosen name in
  `households.hid`). Adds a `devices(device_id PK, home_hid)` map
  and seeds it from existing households so legacy single-device homes
  keep working without any code changes on the firmware side. All
  `CREATE TABLE IF NOT EXISTS` + `INSERT OR IGNORE`, fully re-runnable.

- **`0005_entity_timestamps.sql`** — universal LWW triplet
  (`created_at`, `updated_at`, `is_deleted`) on households, cats,
  users, devices. `ALTER TABLE ADD COLUMN`, errors with
  "duplicate column name" if already applied — safe to ignore.
  See `docs/sync-implementation-handoff.md` §5.2 for merge rules.

- **`0006_sync_tables.sql`** — three new tables for the multi-device
  sync flow: `pending_pairings` (3-min TTL handshake state),
  `pairings` (persistent device↔home relationship), `sync_logs`
  (audit log; capped to 100 rows / home in code). All
  `CREATE TABLE IF NOT EXISTS`, fully re-runnable.

## What about `schema.sql`?

`schema.sql` is the **canonical full schema** — what a brand-new
database should look like. Use it for `--local` provisioning or for
docs / inspection. **Don't** apply it to an old remote database with
missing columns — the unique index creation references columns added
later, so it'll fail mid-way.

For a fresh database:
```sh
npm run db:apply:remote   # uses schema.sql, full creation
```

For an existing database that needs catch-up:
```sh
# Apply each migration that hasn't been run yet (in order).
# Errors on "duplicate column" are expected if a step was already done.
```
