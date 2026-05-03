import { describe, it, expect } from "vitest";
import { confirmPairingFor } from "../src/pair";
import { verifyToken } from "../src/auth";
import type { Env } from "../src/env";

// Tests for the shared confirmPairingFor helper extracted from
// pair.ts in the inline-pair-confirm rework. This is the function
// every auth path now uses (POST /api/auth/setup, /login,
// /quick-setup, plus the legacy POST /api/pair/confirm wrapper).
//
// We use the same in-memory D1 mock pattern as transparent.test.ts:
// scripted query responses by SQL-substring match. Imported here
// instead of refactored into a shared test helper since the two
// files have diverged enough (slightly different Script shape needs)
// that sharing would obscure more than it'd save.

interface Script {
  match: string | string[];
  first?: unknown;
  run?: { meta: { changes: number } };
  capture?: (params: unknown[]) => void;
}

function makeMockEnv(scripts: Script[]) {
  const consumed = { i: 0 };
  const matches = (sql: string, m: string | string[]): boolean =>
    Array.isArray(m) ? m.every((s) => sql.includes(s)) : sql.includes(m);

  const env = {
    AUTH_SECRET: "test-secret",
    DB: {
      prepare(sql: string) {
        return {
          bind(...params: unknown[]) {
            const script = scripts[consumed.i];
            if (!script) {
              throw new Error(`mock D1: unscripted call #${consumed.i + 1}: ${sql.slice(0, 80)}…`);
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

  return { env, consumed };
}

const SECRET = "test-secret";

describe("confirmPairingFor", () => {
  it("404 when no pending_pairings row exists", async () => {
    const { env } = makeMockEnv([
      { match: "FROM pending_pairings", first: null },
    ]);
    const res = await confirmPairingFor(env, "home-x", "feedme-y", SECRET);
    expect(res.ok).toBe(false);
    if (!res.ok) {
      expect(res.status).toBe(404);
      expect(res.error).toMatch(/no pending pairing/i);
    }
  });

  it("410 when device cancelled", async () => {
    const now = Math.floor(Date.now() / 1000);
    const { env } = makeMockEnv([
      { match: "FROM pending_pairings",
        first: { expires_at: now + 60, home_hid: null,
                 confirmed_at: null, cancelled_at: now - 5 } },
    ]);
    const res = await confirmPairingFor(env, "home-x", "feedme-y", SECRET);
    expect(res.ok).toBe(false);
    if (!res.ok) {
      expect(res.status).toBe(410);
      expect(res.error).toMatch(/cancelled/i);
    }
  });

  it("410 when window expired (and never confirmed)", async () => {
    const now = Math.floor(Date.now() / 1000);
    const { env } = makeMockEnv([
      { match: "FROM pending_pairings",
        first: { expires_at: now - 10, home_hid: null,
                 confirmed_at: null, cancelled_at: null } },
    ]);
    const res = await confirmPairingFor(env, "home-x", "feedme-y", SECRET);
    expect(res.ok).toBe(false);
    if (!res.ok) {
      expect(res.status).toBe(410);
      expect(res.error).toMatch(/expired/i);
    }
  });

  it("idempotent re-issue when already confirmed for SAME home", async () => {
    const now = Math.floor(Date.now() / 1000);
    const updateParams: unknown[][] = [];
    const { env, consumed } = makeMockEnv([
      { match: "FROM pending_pairings",
        first: { expires_at: now - 60, home_hid: "home-andrey",
                 confirmed_at: now - 10, cancelled_at: null } },
      { match: ["UPDATE pending_pairings", "device_token"],
        capture: (p) => updateParams.push(p) },
    ]);
    const res = await confirmPairingFor(env, "home-andrey", "feedme-abc", SECRET);
    expect(res.ok).toBe(true);
    if (res.ok) {
      expect(res.alreadyPaired).toBe(true);
      // Token re-issued; verify it parses.
      const payload = await verifyToken(res.deviceToken, SECRET);
      expect(payload?.type).toBe("device");
    }
    expect(consumed.i).toBe(2);   // SELECT + UPDATE only — no fresh INSERT path
    // UPDATE bound to (token, deviceId). Token is at index 0.
    expect(updateParams[0]?.[1]).toBe("feedme-abc");
  });

  it("409 when already confirmed for a DIFFERENT home", async () => {
    const now = Math.floor(Date.now() / 1000);
    const { env } = makeMockEnv([
      { match: "FROM pending_pairings",
        first: { expires_at: now - 60, home_hid: "home-other",
                 confirmed_at: now - 10, cancelled_at: null } },
    ]);
    const res = await confirmPairingFor(env, "home-andrey", "feedme-abc", SECRET);
    expect(res.ok).toBe(false);
    if (!res.ok) {
      expect(res.status).toBe(409);
      expect(res.error).toMatch(/different home/i);
    }
  });

  it("fresh confirmation: INSERTs pairings + devices, UPDATEs pending_pairings", async () => {
    const now = Math.floor(Date.now() / 1000);
    const pairingsInsertParams: unknown[][] = [];
    const devicesUpsertParams: unknown[][] = [];
    const pendingUpdateParams: unknown[][] = [];
    const { env, consumed } = makeMockEnv([
      { match: "FROM pending_pairings",
        first: { expires_at: now + 60, home_hid: null,
                 confirmed_at: null, cancelled_at: null } },
      { match: ["SELECT id, is_deleted FROM pairings"], first: null },
      { match: "INSERT INTO pairings",
        capture: (p) => pairingsInsertParams.push(p) },
      { match: ["INSERT INTO devices", "ON CONFLICT"],
        capture: (p) => devicesUpsertParams.push(p) },
      { match: ["UPDATE pending_pairings", "device_token"],
        capture: (p) => pendingUpdateParams.push(p) },
    ]);
    const res = await confirmPairingFor(env, "home-andrey", "feedme-fresh", SECRET);
    expect(res.ok).toBe(true);
    if (res.ok) {
      expect(res.alreadyPaired).toBe(false);
      const payload = await verifyToken(res.deviceToken, SECRET);
      expect(payload?.type).toBe("device");
      if (payload?.type === "device") {
        expect(payload.hid).toBe("home-andrey");
        expect(payload.deviceId).toBe("feedme-fresh");
      }
    }
    expect(consumed.i).toBe(5);
    // pairings INSERT bound to (deviceId, hid, now, now).
    expect(pairingsInsertParams[0]?.[0]).toBe("feedme-fresh");
    expect(pairingsInsertParams[0]?.[1]).toBe("home-andrey");
    // devices upsert bound to (deviceId, hid, joined_at, created_at, updated_at).
    expect(devicesUpsertParams[0]?.[0]).toBe("feedme-fresh");
    expect(devicesUpsertParams[0]?.[1]).toBe("home-andrey");
    // pending_pairings UPDATE bound to (hid, now, deviceToken, deviceId).
    expect(pendingUpdateParams[0]?.[0]).toBe("home-andrey");
    expect(pendingUpdateParams[0]?.[3]).toBe("feedme-fresh");
  });

  it("re-pair after unpair: UPDATEs the existing pairings row instead of INSERTing", async () => {
    // Repro of the pair.ts "soft-restore on re-pair" path.
    const now = Math.floor(Date.now() / 1000);
    const updateParams: unknown[][] = [];
    const { env, consumed } = makeMockEnv([
      { match: "FROM pending_pairings",
        first: { expires_at: now + 60, home_hid: null,
                 confirmed_at: null, cancelled_at: null } },
      { match: ["SELECT id, is_deleted FROM pairings"],
        first: { id: 99, is_deleted: 1 } },
      { match: ["UPDATE pairings", "is_deleted = 0"],
        capture: (p) => updateParams.push(p) },
      { match: ["INSERT INTO devices", "ON CONFLICT"] },
      { match: ["UPDATE pending_pairings", "device_token"] },
    ]);
    const res = await confirmPairingFor(env, "home-back", "feedme-resurrected", SECRET);
    expect(res.ok).toBe(true);
    expect(consumed.i).toBe(5);
    // pairings UPDATE bound to (hid, now, id).
    expect(updateParams[0]?.[0]).toBe("home-back");
    expect(updateParams[0]?.[2]).toBe(99);
  });
});
