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

  // ── Auto-pair E2E (post PR #34) ─────────────────────────────
  // Mirrors what the FIRMWARE does: device boots, calls /pair/start
  // immediately on entering the QR screen, then polls /pair/check.
  // Webapp's auth handlers (setup / login / quick-setup) MUST mark
  // the row confirmed inline so the device's next /pair/check returns
  // a DeviceToken. Regression check for the "device stuck on Pairing
  // screen forever" bug.
  console.log(`\n  ── auto-pair flows ───────────────────────────────`);
  await runAutoPairScenario("PIN setup",
    async (devId) => {
      const r = await call("POST", "/api/auth/setup",
        { hid: `auto-setup-${Date.now()}-${devId.slice(-6)}`, pin: "1111", deviceId: devId });
      return r;
    });
  await runAutoPairScenario("PIN login (existing home, new device)",
    async (devId) => {
      // Pre-create a home so login has a target.
      const homeHid = `auto-login-${Date.now()}-${devId.slice(-6)}`;
      const setup = await call("POST", "/api/auth/setup",
        { hid: homeHid, pin: "2222" });
      if (setup.status !== 200) {
        log("login scenario: precreate home", "fail", `status=${setup.status}`);
        return setup;
      }
      const r = await call("POST", "/api/auth/login",
        { hid: homeHid, pin: "2222", deviceId: devId });
      // Best-effort cleanup of the precreated home (uses the login token).
      const t = r.body?.token;
      if (t) await call("DELETE", "/api/auth/household", null, t);
      return r;
    });
  await runAutoPairScenario("Quick-Start (transparent home)",
    async (devId) => {
      const r = await call("POST", "/api/auth/quick-setup", { deviceId: devId });
      // Cleanup transparent home.
      const t = r.body?.token;
      if (t) await call("DELETE", "/api/auth/household", null, t);
      return r;
    });

  // Empty-list sync: an unpaired-then-just-paired device can send an
  // empty cats[] / users[] before its first local seeds land. Server
  // must accept this as a no-op, return its own state (also empty),
  // and not crash. Regression check after the user's "Check how sync
  // handles empty lists" report.
  console.log(`\n  ── empty-list sync ───────────────────────────────`);
  await runEmptyListSyncScenario();

  // pairError surface check: login WITH deviceId but WITHOUT a prior
  // /pair/start should still succeed (auth ok) but include pairError
  // in the response so the webapp can surface a recovery toast.
  console.log(`\n  ── pairError surface ────────────────────────────`);
  const noStartHome = `noStart-${Date.now()}`;
  const noStartSetup = await call("POST", "/api/auth/setup",
    { hid: noStartHome, pin: "3333" });
  const noStartToken = noStartSetup.body?.token;
  const noStartLogin = await call("POST", "/api/auth/login",
    { hid: noStartHome, pin: "3333", deviceId: `feedme-no-start-${Date.now()}` });
  if (noStartLogin.status === 200 && typeof noStartLogin.body?.pairError === "string") {
    log("login w/o /pair/start returns 200 + pairError", "ok",
        `"${noStartLogin.body.pairError.slice(0, 50)}…"`);
  } else {
    log("login w/o /pair/start returns 200 + pairError", "fail",
        `status=${noStartLogin.status} body=${JSON.stringify(noStartLogin.body).slice(0, 80)}`);
  }
  if (noStartToken) await call("DELETE", "/api/auth/household", null, noStartToken);

  console.log(`\n  ${failed === 0 ? "✓ all assertions passed" : `✗ ${failed} assertion(s) failed`}`);
  process.exit(failed === 0 ? 0 : 1);
}

// Empty-list sync: confirms the backend accepts cats=[] / users=[]
// without crashing. Mirrors the firmware's first-sync-after-pair
// state when its local rosters happen to have no entries (rare —
// firmware seeds locally on boot — but a possible edge if the user
// deleted everything via the webapp first). Also covers the
// auto-pair → empty-sync → expected-empty-response loop.
async function runEmptyListSyncScenario() {
  const devId = `feedme-empty-${Date.now().toString(16)}`;
  const homeHid = `empty-home-${Date.now()}`;

  await call("POST", "/api/pair/start", { deviceId: devId });
  const setup = await call("POST", "/api/auth/setup",
    { hid: homeHid, pin: "0000", deviceId: devId });
  if (setup.status !== 200) {
    log("[empty-list] precondition setup", "fail", `status=${setup.status}`);
    return;
  }
  const ut = setup.body.token;
  const check = await call("GET", `/api/pair/check?deviceId=${devId}`);
  const dt = check.body?.token;
  if (!dt) { log("[empty-list] got DeviceToken", "fail", `body=${JSON.stringify(check.body)}`); return; }

  // Empty rosters in the request body — both arrays present but
  // length 0. Server should: accept the body, run zero merge calls,
  // return whatever it has on file. PIN-setup homes have no
  // server-side defaults, so the response should also be empty.
  const emptyBody = {
    schemaVersion: 1, deviceId: devId, lastSyncAt: null,
    home: { name: homeHid, updatedAt: 0 },
    cats: [],
    users: [],
  };
  const sync = await call("POST", "/api/sync", emptyBody, dt);
  if (sync.status !== 200) {
    log("[empty-list] sync 200 for empty rosters", "fail",
        `status=${sync.status} body=${sync.text?.slice(0, 80)}`);
    await call("DELETE", "/api/auth/household", null, ut);
    return;
  }
  const cats  = Array.isArray(sync.body?.cats)  ? sync.body.cats  : null;
  const users = Array.isArray(sync.body?.users) ? sync.body.users : null;
  if (!cats || !users) {
    log("[empty-list] response has cats[] + users[]", "fail",
        `cats=${typeof sync.body?.cats} users=${typeof sync.body?.users}`);
  } else {
    log("[empty-list] empty rosters round-trip cleanly", "ok",
        `req cats=0 users=0 → resp cats=${cats.length} users=${users.length}`);
  }

  // Quick-Start variant: server SHOULD seed 1 cat + 1 user. Verify.
  const qsDev = `feedme-qs-empty-${Date.now().toString(16)}`;
  await call("POST", "/api/pair/start", { deviceId: qsDev });
  const qs = await call("POST", "/api/auth/quick-setup", { deviceId: qsDev });
  const qsToken = qs.body?.token;
  if (qs.status !== 200 || !qsToken) {
    log("[empty-list] quick-setup precondition", "fail", `status=${qs.status}`);
    await call("DELETE", "/api/auth/household", null, ut);
    return;
  }
  const qsCheck = await call("GET", `/api/pair/check?deviceId=${qsDev}`);
  const qsDt = qsCheck.body?.token;
  const qsSync = await call("POST", "/api/sync", {
    schemaVersion: 1, deviceId: qsDev, lastSyncAt: null,
    home: { name: qs.body.hid, updatedAt: 0 },
    cats: [], users: [],
  }, qsDt);
  const qsCats  = qsSync.body?.cats  ?? [];
  const qsUsers = qsSync.body?.users ?? [];
  if (qsCats.length === 1 && qsUsers.length === 1
      && qsCats[0]?.name === "Cat" && qsUsers[0]?.name === "User") {
    log("[empty-list] Quick-Start home seeded with 1 cat + 1 user", "ok",
        `cat='${qsCats[0].name}' user='${qsUsers[0].name}'`);
  } else {
    log("[empty-list] Quick-Start home seeded with 1 cat + 1 user", "fail",
        `cats=${qsCats.length} users=${qsUsers.length}`);
  }

  await call("DELETE", "/api/auth/household", null, ut);
  await call("DELETE", "/api/auth/household", null, qsToken);
}

// One full firmware-side simulation of the auto-pair handshake. Used
// by every scenario above (setup / login / quick-setup) so the
// assertion text reads identically across them — easy to scan when
// only one path is broken.
async function runAutoPairScenario(label, runAuth) {
  const devId = `feedme-auto-${Date.now().toString(16)}-${Math.random().toString(16).slice(2, 6)}`;

  // 1. Device opens its 3-min pair window.
  const start = await call("POST", "/api/pair/start", { deviceId: devId });
  if (start.status !== 200) {
    log(`[${label}] /pair/start`, "fail", `status=${start.status}`);
    return;
  }

  // 2. Webapp completes auth — should mark pending_pairings confirmed
  //    inline. Auth call is scenario-specific (passed in).
  const auth = await runAuth(devId);
  if (auth.status !== 200) {
    log(`[${label}] auth response 200`, "fail", `status=${auth.status}: ${auth.text?.slice(0, 80)}`);
    return;
  }
  if (auth.body?.pairError) {
    log(`[${label}] auth response has NO pairError`, "fail",
        `pairError="${auth.body.pairError}"`);
    return;
  }
  log(`[${label}] auth + inline pair OK`, "ok", `hid='${auth.body.hid}'`);

  // 3. Device polls /pair/check. With auto-pair this MUST return
  //    confirmed + a DeviceToken on the very first poll — no waiting,
  //    no separate /pair/confirm round-trip required.
  const check = await call("GET", `/api/pair/check?deviceId=${devId}`);
  if (check.status !== 200) {
    log(`[${label}] /pair/check status 200`, "fail", `got ${check.status}`);
    return;
  }
  if (check.body?.status !== "confirmed") {
    log(`[${label}] /pair/check returns confirmed`, "fail",
        `got status='${check.body?.status}' — pairing did NOT complete inline`);
    return;
  }
  if (typeof check.body?.token !== "string" || !check.body.token) {
    log(`[${label}] /pair/check carries DeviceToken`, "fail",
        `token=${JSON.stringify(check.body?.token)}`);
    return;
  }
  if (check.body?.hid !== auth.body.hid) {
    log(`[${label}] /pair/check hid matches auth`, "fail",
        `auth=${auth.body.hid} check=${check.body.hid}`);
    return;
  }
  log(`[${label}] device receives DeviceToken on first poll`, "ok",
      `hid='${check.body.hid}'`);
}

main().catch((e) => {
  console.error("smoke run threw:", e);
  process.exit(2);
});
