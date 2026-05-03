import { describe, it, expect } from "vitest";
import { validateHomeName } from "../src/validators";
import { validateDeviceId } from "../src/pair";
import { isInt, isSyncCat, isSyncUser, isUuid, parseSchedule } from "../src/sync";

// Pure-function tests — no Workers runtime, no D1, no fetch.
// These cover the input-validation surface that gates every API
// touchpoint, plus the sync wire-format type guards. Anything past
// these checks is trusted to have well-formed shape.

describe("validateHomeName", () => {
  it("accepts simple names", () => {
    expect(validateHomeName("Smith Family")).toBe("Smith Family");
    expect(validateHomeName("home-andrey")).toBe("home-andrey");
    expect(validateHomeName("a")).toBe("a");
  });
  it("trims surrounding whitespace", () => {
    expect(validateHomeName("  Trimmed  ")).toBe("Trimmed");
    // \n and \t are stripped by String.prototype.trim(), so the
    // resulting "spaced" passes the control-char check.
    expect(validateHomeName("\tspaced\n")).toBe("spaced");
  });
  it("accepts unicode + emoji", () => {
    expect(validateHomeName("家")).toBe("家");
    expect(validateHomeName("🏡 Cozy")).toBe("🏡 Cozy");
  });
  it("rejects empty / whitespace-only", () => {
    expect(validateHomeName("")).toBeNull();
    expect(validateHomeName("   ")).toBeNull();
    expect(validateHomeName("\t\n")).toBeNull();
  });
  it("rejects too long (>64 chars)", () => {
    expect(validateHomeName("x".repeat(64))).toBe("x".repeat(64));
    expect(validateHomeName("x".repeat(65))).toBeNull();
  });
  it("rejects non-string input", () => {
    expect(validateHomeName(undefined)).toBeNull();
    expect(validateHomeName(null)).toBeNull();
    // @ts-expect-error testing runtime guard
    expect(validateHomeName(42)).toBeNull();
    // @ts-expect-error testing runtime guard
    expect(validateHomeName({})).toBeNull();
  });
  it("rejects embedded control characters", () => {
    expect(validateHomeName("bad\x00name")).toBeNull();
    expect(validateHomeName("bad\nname")).toBeNull();
    expect(validateHomeName("bad\x7fname")).toBeNull();
  });
});

describe("validateDeviceId", () => {
  it("accepts the firmware's hid format", () => {
    expect(validateDeviceId("feedme-a8b3c1d4e5f6")).toBe("feedme-a8b3c1d4e5f6");
    expect(validateDeviceId("feedme-abc-1")).toBe("feedme-abc-1");
  });
  it("trims whitespace", () => {
    expect(validateDeviceId("  feedme-x  ")).toBe("feedme-x");
  });
  it("rejects empty / non-string / too long", () => {
    expect(validateDeviceId("")).toBeNull();
    expect(validateDeviceId(null)).toBeNull();
    expect(validateDeviceId(undefined)).toBeNull();
    expect(validateDeviceId(42)).toBeNull();
    expect(validateDeviceId("x".repeat(65))).toBeNull();
  });
  it("rejects non-alphanumeric/hyphen chars", () => {
    expect(validateDeviceId("feedme abc")).toBeNull();    // space
    expect(validateDeviceId("feedme/abc")).toBeNull();    // slash — URL-injection risk
    expect(validateDeviceId("feedme.abc")).toBeNull();    // dot
    expect(validateDeviceId("feedme'or'1")).toBeNull();   // SQL-injection canary
  });
});

describe("isUuid", () => {
  it("accepts 32-char lowercase hex", () => {
    expect(isUuid("a8b3c1d4e5f60123456789abcdef0123")).toBe(true);
    expect(isUuid("0".repeat(32))).toBe(true);
    expect(isUuid("ffffffffffffffffffffffffffffffff")).toBe(true);
  });
  it("rejects wrong length", () => {
    expect(isUuid("a8b3c1d4e5f60123456789abcdef012")).toBe(false);   // 31
    expect(isUuid("a8b3c1d4e5f60123456789abcdef01234")).toBe(false); // 33
    expect(isUuid("")).toBe(false);
  });
  it("rejects uppercase + non-hex chars", () => {
    expect(isUuid("A8B3C1D4E5F60123456789ABCDEF0123")).toBe(false);
    expect(isUuid("g8b3c1d4e5f60123456789abcdef0123")).toBe(false);  // g not hex
    expect(isUuid("a8b3c1d4-e5f6-0123-4567-89abcdef0123")).toBe(false); // hyphenated
  });
  it("rejects non-string", () => {
    expect(isUuid(null)).toBe(false);
    expect(isUuid(undefined)).toBe(false);
    expect(isUuid(123)).toBe(false);
  });
});

describe("isSyncCat with optional uuid", () => {
  const valid = {
    slotId: 0, name: "Mochi", color: 0, slug: "C2",
    defaultPortionG: 40, hungryThresholdSec: 18000,
    scheduleHours: [7, 12, 18, 21],
    createdAt: 1700000000, updatedAt: 1700000000, isDeleted: false,
  };
  it("accepts a cat without uuid (legacy)", () => {
    expect(isSyncCat(valid)).toBe(true);
  });
  it("accepts a cat with valid uuid", () => {
    expect(isSyncCat({ ...valid, uuid: "0".repeat(32) })).toBe(true);
  });
  it("rejects a cat with malformed uuid", () => {
    expect(isSyncCat({ ...valid, uuid: "not-a-uuid" })).toBe(false);
    expect(isSyncCat({ ...valid, uuid: "A".repeat(32) })).toBe(false); // uppercase
  });
});

describe("isInt", () => {
  it("accepts safe integers", () => {
    expect(isInt(0)).toBe(true);
    expect(isInt(-1)).toBe(true);
    expect(isInt(2_000_000_000)).toBe(true);
  });
  it("rejects non-numbers, NaN, Infinity, fractions", () => {
    expect(isInt(1.5)).toBe(false);
    expect(isInt(NaN)).toBe(false);
    expect(isInt(Infinity)).toBe(false);
    expect(isInt("1")).toBe(false);
    expect(isInt(null)).toBe(false);
    expect(isInt(undefined)).toBe(false);
  });
});

describe("isSyncCat", () => {
  const valid = {
    slotId: 0, name: "Mochi", color: 0, slug: "C2",
    defaultPortionG: 40, hungryThresholdSec: 18000,
    scheduleHours: [7, 12, 18, 21],
    createdAt: 1700000000, updatedAt: 1700000000, isDeleted: false,
  };
  it("accepts a well-formed cat", () => {
    expect(isSyncCat(valid)).toBe(true);
  });
  it("rejects bad slot id", () => {
    expect(isSyncCat({ ...valid, slotId: -1 })).toBe(false);
    expect(isSyncCat({ ...valid, slotId: 256 })).toBe(false);
    expect(isSyncCat({ ...valid, slotId: 1.5 })).toBe(false);
  });
  it("rejects schedule of wrong length / out of range hours", () => {
    expect(isSyncCat({ ...valid, scheduleHours: [7, 12, 18] })).toBe(false);
    expect(isSyncCat({ ...valid, scheduleHours: [7, 12, 18, 21, 24] })).toBe(false);
    expect(isSyncCat({ ...valid, scheduleHours: [7, 12, 18, 24] })).toBe(false);
    expect(isSyncCat({ ...valid, scheduleHours: [7, 12, 18, -1] })).toBe(false);
  });
  it("rejects missing or wrongly-typed fields", () => {
    expect(isSyncCat({ ...valid, name: 42 })).toBe(false);
    expect(isSyncCat({ ...valid, isDeleted: "false" })).toBe(false);
    expect(isSyncCat({ ...valid, updatedAt: "now" })).toBe(false);
    expect(isSyncCat(null)).toBe(false);
    expect(isSyncCat({})).toBe(false);
  });
});

describe("isSyncUser", () => {
  const valid = {
    slotId: 0, name: "Andrey", color: 255,
    createdAt: 1700000000, updatedAt: 1700000000, isDeleted: false,
  };
  it("accepts a well-formed user", () => {
    expect(isSyncUser(valid)).toBe(true);
  });
  it("rejects bad shape", () => {
    expect(isSyncUser({ ...valid, slotId: -1 })).toBe(false);
    expect(isSyncUser({ ...valid, name: null })).toBe(false);
    expect(isSyncUser({})).toBe(false);
    expect(isSyncUser(null)).toBe(false);
  });
});

describe("parseSchedule", () => {
  it("round-trips a valid 4-hour array", () => {
    expect(parseSchedule("[7,12,18,21]")).toEqual([7, 12, 18, 21]);
    expect(parseSchedule("[0,6,12,23]")).toEqual([0, 6, 12, 23]);
  });
  it("falls back to defaults on bad JSON", () => {
    expect(parseSchedule("not json")).toEqual([7, 12, 18, 21]);
    expect(parseSchedule("")).toEqual([7, 12, 18, 21]);
    expect(parseSchedule("null")).toEqual([7, 12, 18, 21]);
  });
  it("falls back when wrong shape", () => {
    expect(parseSchedule("[7,12,18]")).toEqual([7, 12, 18, 21]);
    expect(parseSchedule("[7,12,18,21,24]")).toEqual([7, 12, 18, 21]);
    expect(parseSchedule('["a","b","c","d"]')).toEqual([7, 12, 18, 21]);
  });
});
