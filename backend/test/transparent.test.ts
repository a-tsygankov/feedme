import { describe, it, expect, beforeEach } from "vitest";
import {
  postQuickSetup, postLoginQr, postLoginTokenCreate, postSetPin,
} from "../src/transparent";
import { issueDeviceToken, verifyToken } from "../src/auth";
import type { Env } from "../src/env";

// Phase F transparent-account handler tests. We don't have a real D1
// (or @cloudflare/vitest-pool-workers), so each test instantiates a
// tiny in-memory mock that captures every prepare().bind().run/first()
// call and returns scripted responses. Two reasons over a real
// SQLite-in-memory:
//
//   1. Faster setup — no migrations to apply per test.
//   2. Lets us assert on the EXACT SQL strings (e.g. confirm the
//      atomic UPDATE in postLoginQr includes "AND consumed_at IS NULL"
//      — the whole point of one of these fixes).
//
// SQL match is by substring on the prepare() string. Order matters:
// the first script whose pattern matches consumes that call. If a
// call comes in with no matching script, we throw — that's a louder
// failure than returning silent defaults, and surfaces "the handler
// changed its query order" regressions.

interface Script {
  // Substring (or substrings, all must appear) the SQL must contain.
  // Strings are case-sensitive — that's fine, our handler SQL is
  // hand-written and stable.
  match: string | string[];
  // What .first() returns (or undefined for run-only calls). For
  // success: the row object. For not-found: null.
  first?: unknown;
  // What .run() returns (defaults to { meta: { changes: 1 } }).
  run?: { meta: { changes: number } };
  // Capture the bound params for assertions in the test body.
  capture?: (params: unknown[]) => void;
}

function makeMockEnv(scripts: Script[]): {
  env: Env;
  // Index of the next script to consume; tests can poke this if
  // they want to assert "exactly N scripts were used".
  consumed: { i: number };
  recorded: Array<{ sql: string; params: unknown[] }>;
} {
  const consumed = { i: 0 };
  const recorded: Array<{ sql: string; params: unknown[] }> = [];

  const matches = (sql: string, m: string | string[]): boolean =>
    Array.isArray(m) ? m.every((s) => sql.includes(s)) : sql.includes(m);

  const env = {
    AUTH_SECRET: "test-secret",
    DB: {
      prepare(sql: string) {
        return {
          bind(...params: unknown[]) {
            recorded.push({ sql, params });
            const script = scripts[consumed.i];
            if (!script) {
              throw new Error(
                `mock D1: unscripted call #${consumed.i + 1}: ${sql.slice(0, 80)}…`,
              );
            }
            if (!matches(sql, script.match)) {
              throw new Error(
                `mock D1: script #${consumed.i + 1} expected ${JSON.stringify(script.match)} but got: ${sql.slice(0, 120)}…`,
              );
            }
            consumed.i++;
            script.capture?.(params);
            return {
              first: async () => script.first ?? null,
              run:   async () => script.run ?? { meta: { changes: 1 } },
              all:   async () => ({ results: [] }),
            };
          },
        };
      },
    },
  } as unknown as Env;

  return { env, consumed, recorded };
}

const SECRET = "test-secret";

// ── postQuickSetup ────────────────────────────────────────────────

describe("postQuickSetup", () => {
  it("rejects missing deviceId with 400", async () => {
    const { env } = makeMockEnv([]);
    const res = await postQuickSetup(env, {}, SECRET);
    expect(res.status).toBe(400);
  });

  it("rejects empty/whitespace deviceId", async () => {
    const { env } = makeMockEnv([]);
    const res = await postQuickSetup(env, { deviceId: "   " }, SECRET);
    expect(res.status).toBe(400);
  });

  it("returns alreadyPaired:true when device has an active pairing", async () => {
    const { env } = makeMockEnv([
      { match: ["SELECT home_hid FROM pairings", "is_deleted = 0"],
        first: { home_hid: "home-existing" } },
    ]);
    const res = await postQuickSetup(env, { deviceId: "feedme-abc" }, SECRET);
    expect(res.status).toBe(200);
    const body = await res.json() as { token: string; hid: string; alreadyPaired: boolean };
    expect(body.hid).toBe("home-existing");
    expect(body.alreadyPaired).toBe(true);
    // Cookie set on the response.
    expect(res.headers.get("set-cookie") ?? "").toContain("feedme.session=");
  });

  it("refuses with 409 when no pending_pairings row exists", async () => {
    // Order: existing-pairing check (none) → pending check (none) → 409.
    const { env, consumed } = makeMockEnv([
      { match: "SELECT home_hid FROM pairings", first: null },
      { match: "FROM pending_pairings",         first: null },
    ]);
    const res = await postQuickSetup(env, { deviceId: "feedme-fresh" }, SECRET);
    expect(res.status).toBe(409);
    expect(consumed.i).toBe(2);
    const body = await res.json() as { error: string };
    expect(body.error).toMatch(/tap Pair/i);
  });

  it("refuses with 410 when pending_pairings is cancelled", async () => {
    const now = Math.floor(Date.now() / 1000);
    const { env } = makeMockEnv([
      { match: "SELECT home_hid FROM pairings", first: null },
      { match: "FROM pending_pairings",
        first: { requested_at: now - 30, expires_at: now + 60, cancelled_at: now - 10 } },
    ]);
    const res = await postQuickSetup(env, { deviceId: "feedme-cncl" }, SECRET);
    expect(res.status).toBe(410);
    const body = await res.json() as { error: string };
    expect(body.error).toMatch(/cancelled/i);
  });

  it("refuses with 410 when pending_pairings has expired", async () => {
    const now = Math.floor(Date.now() / 1000);
    const { env } = makeMockEnv([
      { match: "SELECT home_hid FROM pairings", first: null },
      { match: "FROM pending_pairings",
        first: { requested_at: now - 200, expires_at: now - 20, cancelled_at: null } },
    ]);
    const res = await postQuickSetup(env, { deviceId: "feedme-old" }, SECRET);
    expect(res.status).toBe(410);
    const body = await res.json() as { error: string };
    expect(body.error).toMatch(/expired/i);
  });

  it("creates household + pairings + devices + updates pending_pairings, returns 200 + cookie", async () => {
    // SQL order (post-2026-05 default-cat-and-user seed refactor):
    //   1. existing-pairing check (none)
    //   2. pre-check pending_pairings (fresh, not cancelled)
    //   3. INSERT households
    //   4. INSERT cats (default seed: name='Cat')
    //   5. INSERT users (default seed: name='User')
    //   6. confirmPairingFor: re-read pending_pairings (fresh)
    //   7. confirmPairingFor: SELECT pairings (none — first time for this device)
    //   8. confirmPairingFor: INSERT pairings
    //   9. confirmPairingFor: INSERT devices
    //   10. confirmPairingFor: UPDATE pending_pairings { confirmed_at, device_token }
    const now = Math.floor(Date.now() / 1000);
    const householdParams: unknown[][] = [];
    const pendingUpdateParams: unknown[][] = [];
    const { env, consumed } = makeMockEnv([
      { match: "SELECT home_hid FROM pairings", first: null },
      { match: "FROM pending_pairings",
        first: { expires_at: now + 175, cancelled_at: null, confirmed_at: null } },
      { match: ["INSERT INTO households", "pin_salt"],
        capture: (p) => householdParams.push(p) },
      // 2026-05: default cat + user seeded server-side so the webapp
      // dashboard isn't blank between Quick-Start and the device's
      // first sync push.
      { match: ["INSERT INTO cats", "'Cat'"] },
      { match: ["INSERT INTO users", "'User'"] },
      // confirmPairingFor's full row read (extra columns).
      { match: ["FROM pending_pairings", "home_hid"],
        first: { expires_at: now + 175, home_hid: null,
                 confirmed_at: null, cancelled_at: null } },
      { match: ["SELECT id, is_deleted FROM pairings"], first: null },
      { match: "INSERT INTO pairings" },
      { match: ["INSERT INTO devices", "ON CONFLICT"] },
      { match: ["UPDATE pending_pairings", "device_token"],
        capture: (p) => pendingUpdateParams.push(p) },
    ]);
    const res = await postQuickSetup(env, { deviceId: "feedme-fresh" }, SECRET);
    expect(res.status).toBe(200);
    expect(consumed.i).toBe(10);   // see SQL-order list above (was 8 pre-seed)
    const body = await res.json() as { token: string; hid: string };
    expect(body.hid).toMatch(/^home-[0-9a-f]{16}$/);
    // Household INSERT was given empty pin_salt (transparent sentinel).
    expect(householdParams[0]?.[0]).toBe(body.hid);
    // pending_pairings UPDATE bound to (hid, now, device_token, deviceId).
    // The DeviceToken embeds (hid, deviceId) — verify the pair.ts
    // regression-from-2024 (args crossed) hasn't reappeared.
    const dt = pendingUpdateParams[0]?.[2] as string;
    expect(typeof dt).toBe("string");
    const payload = await verifyToken(dt, SECRET);
    expect(payload?.type).toBe("device");
    if (payload?.type === "device") {
      expect(payload.hid).toBe(body.hid);
      expect(payload.deviceId).toBe("feedme-fresh");
    }
    // UserToken in the response body is also valid.
    const userPayload = await verifyToken(body.token, SECRET);
    expect(userPayload?.type).toBe("user");
    expect(userPayload?.hid).toBe(body.hid);
    // Set-Cookie present.
    expect(res.headers.get("set-cookie") ?? "").toContain("feedme.session=");
  });
});

// ── postLoginTokenCreate ──────────────────────────────────────────

describe("postLoginTokenCreate", () => {
  it("refuses with 401 when device's pairing is soft-deleted", async () => {
    const { env, consumed } = makeMockEnv([
      { match: ["SELECT id FROM pairings", "is_deleted = 0"], first: null },
    ]);
    const res = await postLoginTokenCreate(env, {
      type: "device", hid: "home-x", deviceId: "feedme-revoked",
    });
    expect(res.status).toBe(401);
    expect(consumed.i).toBe(1);
    const body = await res.json() as { error: string };
    expect(body.error).toMatch(/revoked/i);
  });

  it("mints a 60-s token for an actively-paired device", async () => {
    const insertParams: unknown[][] = [];
    const { env } = makeMockEnv([
      { match: ["SELECT id FROM pairings"], first: { id: 42 } },
      { match: ["INSERT INTO login_qr_tokens"],
        capture: (p) => insertParams.push(p) },
    ]);
    const res = await postLoginTokenCreate(env, {
      type: "device", hid: "home-andrey", deviceId: "feedme-abc",
    });
    expect(res.status).toBe(200);
    const body = await res.json() as { token: string; expiresAt: number };
    // 32-hex token (16 bytes).
    expect(body.token).toMatch(/^[0-9a-f]{32}$/);
    // expiresAt ~now + 60 s.
    const now = Math.floor(Date.now() / 1000);
    expect(body.expiresAt).toBeGreaterThanOrEqual(now + 55);
    expect(body.expiresAt).toBeLessThanOrEqual(now + 65);
    // INSERT bound: token, deviceId, home_hid, created_at, expires_at.
    expect(insertParams[0]?.[0]).toBe(body.token);
    expect(insertParams[0]?.[1]).toBe("feedme-abc");
    expect(insertParams[0]?.[2]).toBe("home-andrey");
  });
});

// ── postLoginQr ───────────────────────────────────────────────────

describe("postLoginQr", () => {
  it("400 when deviceId or token missing", async () => {
    const { env } = makeMockEnv([]);
    const r1 = await postLoginQr(env, {}, SECRET);
    const r2 = await postLoginQr(env, { deviceId: "x" }, SECRET);
    const r3 = await postLoginQr(env, { token: "y" }, SECRET);
    expect(r1.status).toBe(400);
    expect(r2.status).toBe(400);
    expect(r3.status).toBe(400);
  });

  it("404 unknown token", async () => {
    const { env } = makeMockEnv([
      { match: "FROM login_qr_tokens", first: null },
    ]);
    const res = await postLoginQr(env, { deviceId: "x", token: "deadbeef" }, SECRET);
    expect(res.status).toBe(404);
  });

  it("403 deviceId mismatch", async () => {
    const now = Math.floor(Date.now() / 1000);
    const { env } = makeMockEnv([
      { match: "FROM login_qr_tokens",
        first: { home_hid: "home-x", expires_at: now + 30,
                 consumed_at: null, device_id: "feedme-real" } },
    ]);
    const res = await postLoginQr(env, { deviceId: "feedme-evil", token: "t" }, SECRET);
    expect(res.status).toBe(403);
  });

  it("410 expired", async () => {
    const now = Math.floor(Date.now() / 1000);
    const { env } = makeMockEnv([
      { match: "FROM login_qr_tokens",
        first: { home_hid: "home-x", expires_at: now - 5,
                 consumed_at: null, device_id: "feedme-real" } },
    ]);
    const res = await postLoginQr(env, { deviceId: "feedme-real", token: "t" }, SECRET);
    expect(res.status).toBe(410);
    const body = await res.json() as { error: string };
    expect(body.error).toMatch(/expired/i);
  });

  it("410 already consumed (pre-check)", async () => {
    const now = Math.floor(Date.now() / 1000);
    const { env } = makeMockEnv([
      { match: "FROM login_qr_tokens",
        first: { home_hid: "home-x", expires_at: now + 30,
                 consumed_at: now - 2, device_id: "feedme-real" } },
    ]);
    const res = await postLoginQr(env, { deviceId: "feedme-real", token: "t" }, SECRET);
    expect(res.status).toBe(410);
  });

  it("410 when concurrent consume wins the race (UPDATE returns 0 rows)", async () => {
    // Both callers pass the SELECT pre-check (consumed_at IS NULL),
    // but the second to reach UPDATE sees changes=0.
    const now = Math.floor(Date.now() / 1000);
    const { env, consumed } = makeMockEnv([
      { match: "FROM login_qr_tokens",
        first: { home_hid: "home-x", expires_at: now + 30,
                 consumed_at: null, device_id: "feedme-real" } },
      { match: ["UPDATE login_qr_tokens", "consumed_at IS NULL"],
        run: { meta: { changes: 0 } } },
    ]);
    const res = await postLoginQr(env, { deviceId: "feedme-real", token: "t" }, SECRET);
    expect(res.status).toBe(410);
    expect(consumed.i).toBe(2);
  });

  it("200 + cookie + UserToken on successful consumption", async () => {
    const now = Math.floor(Date.now() / 1000);
    const updateParams: unknown[][] = [];
    const { env } = makeMockEnv([
      { match: "FROM login_qr_tokens",
        first: { home_hid: "home-andrey", expires_at: now + 30,
                 consumed_at: null, device_id: "feedme-real" } },
      { match: ["UPDATE login_qr_tokens", "consumed_at IS NULL"],
        capture: (p) => updateParams.push(p),
        run: { meta: { changes: 1 } } },
    ]);
    const res = await postLoginQr(env, { deviceId: "feedme-real", token: "tok123" }, SECRET);
    expect(res.status).toBe(200);
    const body = await res.json() as { token: string; hid: string };
    expect(body.hid).toBe("home-andrey");
    const payload = await verifyToken(body.token, SECRET);
    expect(payload?.type).toBe("user");
    expect(payload?.hid).toBe("home-andrey");
    expect(res.headers.get("set-cookie") ?? "").toContain("feedme.session=");
    // UPDATE bound to (now, token).
    expect(updateParams[0]?.[1]).toBe("tok123");
  });
});

// ── postSetPin ────────────────────────────────────────────────────

describe("postSetPin", () => {
  beforeEach(() => {
    // Each test gets a fresh mock; nothing global to reset.
  });

  it("400 when pin missing or too short", async () => {
    const { env } = makeMockEnv([]);
    const r1 = await postSetPin(env, { type: "user", hid: "h" }, {});
    const r2 = await postSetPin(env, { type: "user", hid: "h" }, { pin: "12" });
    expect(r1.status).toBe(400);
    expect(r2.status).toBe(400);
  });

  it("404 when home doesn't exist", async () => {
    const { env } = makeMockEnv([
      { match: "FROM households", first: null },
    ]);
    const res = await postSetPin(env, { type: "user", hid: "ghost" }, { pin: "1234" });
    expect(res.status).toBe(404);
  });

  it("409 when home already has a PIN", async () => {
    const { env, consumed } = makeMockEnv([
      { match: "FROM households",
        first: { pin_salt: "abc123def456", pin_hash: "x".repeat(64) } },
    ]);
    const res = await postSetPin(env, { type: "user", hid: "h" }, { pin: "1234" });
    expect(res.status).toBe(409);
    expect(consumed.i).toBe(1);   // SELECT only — no UPDATE attempted
  });

  it("200 when promoting a transparent home (empty pin_salt + pin_hash)", async () => {
    const updateParams: unknown[][] = [];
    const { env } = makeMockEnv([
      { match: "FROM households", first: { pin_salt: "", pin_hash: "" } },
      { match: "UPDATE households", capture: (p) => updateParams.push(p) },
    ]);
    const res = await postSetPin(env, { type: "user", hid: "home-foo" }, { pin: "5555" });
    expect(res.status).toBe(200);
    // UPDATE bound to (salt, hash, now, hid). Salt + hash should be
    // hex strings (PBKDF2 result).
    expect(typeof updateParams[0]?.[0]).toBe("string");
    expect((updateParams[0]?.[0] as string).length).toBe(32);  // 16 bytes hex
    expect((updateParams[0]?.[1] as string).length).toBe(64);  // 32 bytes hex
    expect(updateParams[0]?.[3]).toBe("home-foo");
  });

  it("treats half-set pin (salt only, hash empty) as transparent", async () => {
    // Defensive: isTransparentHome() should return true when EITHER
    // column is empty, so a partial repair doesn't lock the user out.
    const { env } = makeMockEnv([
      { match: "FROM households", first: { pin_salt: "abc", pin_hash: "" } },
      { match: "UPDATE households" },
    ]);
    const res = await postSetPin(env, { type: "user", hid: "h" }, { pin: "1234" });
    expect(res.status).toBe(200);
  });
});

// Suppress unused-import lint when the test file doesn't reference
// the helper directly — it's used implicitly via postQuickSetup.
void issueDeviceToken;
