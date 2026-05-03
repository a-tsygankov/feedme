import { describe, it, expect } from "vitest";
import {
  hashPin, verifyPin,
  issueUserToken, issueDeviceToken, verifyToken,
} from "../src/auth";

// Auth crypto unit tests. Webcrypto APIs (crypto.subtle, btoa) are
// available in modern Node/Vitest, so this runs without a Workers
// runtime mock.

const SECRET = "test-secret-do-not-ship-this";

describe("hashPin / verifyPin", () => {
  it("verifies a correct PIN", async () => {
    const { salt, hash } = await hashPin("1234");
    expect(await verifyPin("1234", salt, hash)).toBe(true);
  });
  it("rejects a wrong PIN with the right salt", async () => {
    const { salt, hash } = await hashPin("1234");
    expect(await verifyPin("0000", salt, hash)).toBe(false);
  });
  it("produces different hashes for the same PIN (salted)", async () => {
    const a = await hashPin("1234");
    const b = await hashPin("1234");
    expect(a.salt).not.toBe(b.salt);
    expect(a.hash).not.toBe(b.hash);
  });
});

describe("UserToken issue/verify roundtrip", () => {
  it("verifies a freshly-issued token and preserves hid", async () => {
    const token = await issueUserToken("home-andrey", SECRET);
    const payload = await verifyToken(token, SECRET);
    expect(payload).not.toBeNull();
    expect(payload?.type).toBe("user");
    expect(payload?.hid).toBe("home-andrey");
  });
  it("rejects a token signed with a different secret", async () => {
    const token = await issueUserToken("home-andrey", SECRET);
    const bad = await verifyToken(token, "wrong-secret");
    expect(bad).toBeNull();
  });
  it("rejects a tampered payload", async () => {
    const token = await issueUserToken("home-andrey", SECRET);
    // Flip a single character in the payload portion.
    const [payload, sig] = token.split(".");
    const flipped = (payload.charAt(0) === "a" ? "b" : "a") + payload.slice(1);
    expect(await verifyToken(`${flipped}.${sig}`, SECRET)).toBeNull();
  });
});

describe("DeviceToken issue/verify roundtrip", () => {
  it("preserves both hid and deviceId in the discriminated union", async () => {
    const token = await issueDeviceToken("home-andrey", "feedme-abc", SECRET);
    const payload = await verifyToken(token, SECRET);
    expect(payload).not.toBeNull();
    expect(payload?.type).toBe("device");
    if (payload?.type === "device") {
      expect(payload.hid).toBe("home-andrey");
      expect(payload.deviceId).toBe("feedme-abc");
    }
  });
  it("regression — args must NOT be swapped", async () => {
    // This caught a real bug in pair.ts where issueDeviceToken was
    // called as (deviceId, hid, secret) instead of (hid, deviceId,
    // secret). Every minted token had its hid+deviceId crossed and
    // /api/sync immediately 401'd with 'pairing revoked'.
    const HID = "the-home";
    const DEVICE = "feedme-the-device";
    const token = await issueDeviceToken(HID, DEVICE, SECRET);
    const payload = await verifyToken(token, SECRET);
    if (payload?.type !== "device") throw new Error("expected device token");
    expect(payload.hid).toBe(HID);
    expect(payload.deviceId).toBe(DEVICE);
  });
});

describe("token expiry", () => {
  it("rejects an obviously-expired token", async () => {
    // Build a token by hand with exp in the past.
    const enc = new TextEncoder();
    const b64url = (s: string) =>
      btoa(s).replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/, "");
    const past = Math.floor(Date.now() / 1000) - 60;
    const payload = b64url(JSON.stringify({ type: "user", hid: "x", exp: past }));
    const key = await crypto.subtle.importKey(
      "raw", enc.encode(SECRET),
      { name: "HMAC", hash: "SHA-256" }, false, ["sign"],
    );
    const sigBuf = await crypto.subtle.sign("HMAC", key, enc.encode(payload));
    const sig = b64url(String.fromCharCode(...new Uint8Array(sigBuf)));
    const token = `${payload}.${sig}`;
    expect(await verifyToken(token, SECRET)).toBeNull();
  });
});
