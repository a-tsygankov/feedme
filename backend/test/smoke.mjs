#!/usr/bin/env node
// End-to-end smoke against a deployed Worker. Replaces the ad-hoc
// curl scripts I'd been pasting into bash. Runs idempotency checks
// against every state-changing endpoint that we promised to
// idempotently retry.
//
// Usage:
//   node test/smoke.mjs                    # hits prod Worker
//   BASE=http://localhost:8787 node test/smoke.mjs   # hits local
//
// Self-contained: creates a temp home, exercises every flow, deletes
// the home at the end. Idempotency is the explicit focus — every
// fixture call is sent twice and the result asserted.
//
// Exit codes: 0 = all pass, 1 = at least one assertion failed.

const BASE = process.env.BASE ?? "https://feedme.atsyg-feedme.workers.dev";
const HID = `smoke-${Date.now()}`;
const PIN = "9999";
const DEVICE = `feedme-smoke-${Date.now()}`;
const TEST_EVENT_ID = crypto.randomUUID();

let userToken = null;
let deviceToken = null;
let failed = 0;

function log(label, status, detail = "") {
  const icon = status === "ok" ? "✓" : status === "skip" ? "—" : "✗";
  if (status === "fail") failed++;
  console.log(`${icon} ${label.padEnd(60)} ${detail}`);
}

async function call(method, path, body = null, token = null) {
  const headers = { "content-type": "application/json" };
  if (token) headers["authorization"] = `Bearer ${token}`;
  const res = await fetch(BASE + path, {
    method, headers,
    body: body ? JSON.stringify(body) : undefined,
  });
  const text = await res.text();
  let json = null;
  try { json = JSON.parse(text); } catch { /* keep null */ }
  return { status: res.status, body: json, text };
}

function assertStatus(label, got, want) {
  if (got.status === want) log(label, "ok", `HTTP ${got.status}`);
  else log(label, "fail", `expected ${want}, got ${got.status}: ${got.text.slice(0, 80)}`);
}
function assertEqual(label, got, want) {
  const same = JSON.stringify(got) === JSON.stringify(want);
  if (same) log(label, "ok");
  else log(label, "fail", `got ${JSON.stringify(got)}, want ${JSON.stringify(want)}`);
}

async function main() {
  console.log(`\n  Smoke target: ${BASE}\n  Test home:    ${HID}\n  Test device:  ${DEVICE}\n`);

  // ── auth ────────────────────────────────────────────────────
  const setup = await call("POST", "/api/auth/setup", { hid: HID, pin: PIN });
  assertStatus("auth/setup creates home", setup, 200);
  userToken = setup.body?.token;
  if (!userToken) { log("got user token", "fail", "no token in response"); return; }

  const setupAgain = await call("POST", "/api/auth/setup", { hid: HID, pin: PIN });
  assertStatus("auth/setup retry returns 409 (resource exists)", setupAgain, 409);

  // ── pair start: idempotent (INSERT OR REPLACE) ─────────────
  const start1 = await call("POST", "/api/pair/start", { deviceId: DEVICE });
  assertStatus("pair/start first call", start1, 200);
  const start2 = await call("POST", "/api/pair/start", { deviceId: DEVICE });
  assertStatus("pair/start retry — idempotent (window reset)", start2, 200);
  assertEqual("pair/start retry status pending", start2.body?.status, "pending");

  // ── pair confirm: idempotent (alreadyPaired branch) ────────
  const confirm1 = await call("POST", "/api/pair/confirm",
    { deviceId: DEVICE }, userToken);
  assertStatus("pair/confirm first call", confirm1, 200);

  const confirm2 = await call("POST", "/api/pair/confirm",
    { deviceId: DEVICE }, userToken);
  assertStatus("pair/confirm retry — same home OK", confirm2, 200);
  assertEqual("pair/confirm retry alreadyPaired flag", confirm2.body?.alreadyPaired, true);

  // ── pair check: get the device token ────────────────────────
  const check = await call("GET", `/api/pair/check?deviceId=${DEVICE}`);
  assertStatus("pair/check returns confirmed", check, 200);
  deviceToken = check.body?.token;
  assertEqual("pair/check status confirmed", check.body?.status, "confirmed");
  if (!deviceToken) { log("got device token", "fail", "no token"); return; }

  // ── /api/sync: same payload twice, server state must be stable
  const syncBody = {
    schemaVersion: 1, deviceId: DEVICE, lastSyncAt: null,
    home: { name: HID, updatedAt: 1700000000 },
    cats: [{
      slotId: 0, name: "Mochi", color: 0, slug: "C2",
      defaultPortionG: 40, hungryThresholdSec: 18000,
      scheduleHours: [7, 12, 18, 21],
      createdAt: 1700000000, updatedAt: 1700000000, isDeleted: false,
    }],
    users: [],
  };
  const sync1 = await call("POST", "/api/sync", syncBody, deviceToken);
  assertStatus("sync first call", sync1, 200);
  const sync2 = await call("POST", "/api/sync", syncBody, deviceToken);
  assertStatus("sync retry — same end state", sync2, 200);
  assertEqual("sync retry — same cat name returned",
    sync2.body?.cats?.[0]?.name, sync1.body?.cats?.[0]?.name);
  assertEqual("sync retry — cat updatedAt unchanged",
    sync2.body?.cats?.[0]?.updatedAt, sync1.body?.cats?.[0]?.updatedAt);

  // ── Phase D: server must mint + return a uuid for the cat we
  //    INSERTed without one, and the SAME uuid must come back on
  //    every subsequent response (proving identity is stable, not
  //    re-rolled on each sync).
  const uuid1 = sync1.body?.cats?.[0]?.uuid;
  const uuid2 = sync2.body?.cats?.[0]?.uuid;
  if (typeof uuid1 === "string" && /^[0-9a-f]{32}$/.test(uuid1)) {
    log("sync response includes 32-hex uuid", "ok", uuid1.slice(0, 8) + "…");
  } else {
    log("sync response includes 32-hex uuid", "fail", `got ${JSON.stringify(uuid1)}`);
  }
  assertEqual("uuid stable across syncs", uuid2, uuid1);

  // ── Phase D: send the cat back WITH the uuid. Server should match
  //    on uuid (not slot_id) and return the same row. Sanity check
  //    that the uuid round-trip closes the loop.
  const sync3 = await call("POST", "/api/sync", {
    ...syncBody,
    cats: [{ ...syncBody.cats[0], uuid: uuid1 }],
  }, deviceToken);
  assertEqual("sync with uuid returns same uuid", sync3.body?.cats?.[0]?.uuid, uuid1);

  // ── /api/dashboard/feed: same eventId twice → only one row
  const feed1 = await call("POST", "/api/dashboard/feed",
    { catSlotId: 0, by: "Andrey", eventId: TEST_EVENT_ID }, userToken);
  assertStatus("dashboard/feed first call", feed1, 200);
  const feed2 = await call("POST", "/api/dashboard/feed",
    { catSlotId: 0, by: "Andrey", eventId: TEST_EVENT_ID }, userToken);
  assertStatus("dashboard/feed retry same eventId", feed2, 200);

  // The history endpoint shows whether dedup actually happened.
  const hist = await call("GET", "/api/dashboard/history?cat=0&n=10", null, userToken);
  const eventCount = (hist.body?.events ?? []).length;
  if (eventCount === 1) log("dashboard/feed retry deduped via eventId", "ok", `${eventCount} event in DB`);
  else log("dashboard/feed retry deduped via eventId", "fail",
    `${eventCount} events in DB (expected 1)`);

  // ── DELETE /api/pair/<id>: idempotent retry ─────────────────
  const del1 = await call("DELETE", `/api/pair/${DEVICE}`, null, userToken);
  assertStatus("pair DELETE first call", del1, 200);
  const del2 = await call("DELETE", `/api/pair/${DEVICE}`, null, userToken);
  assertStatus("pair DELETE retry — idempotent 200", del2, 200);
  assertEqual("pair DELETE retry alreadyForgotten flag",
    del2.body?.alreadyForgotten, true);

  // ── DELETE truly unknown device → 404 ───────────────────────
  const delMissing = await call("DELETE",
    "/api/pair/feedme-never-existed-xyz", null, userToken);
  assertStatus("pair DELETE unknown device 404", delMissing, 404);

  // ── cleanup ────────────────────────────────────────────────
  const purge = await call("DELETE", "/api/auth/household", null, userToken);
  assertStatus("auth/household DELETE cleanup", purge, 200);

  console.log(`\n  ${failed === 0 ? "✓ all assertions passed" : `✗ ${failed} assertion(s) failed`}`);
  process.exit(failed === 0 ? 0 : 1);
}

main().catch((e) => {
  console.error("smoke run threw:", e);
  process.exit(2);
});
