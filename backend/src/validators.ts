// Shared input validators. Pulled out of index.ts so tests can
// import them without dragging in the full Worker handler module.

// User-chosen home name = households.hid (post-migration-0004).
// Trim, cap, allow common printable characters. The hid lives in
// URLs, JSON bodies, and the SQLite primary key — so we forbid
// anything that needs special escaping (control chars, newlines).
// Spaces are fine; users will type "Smith Family" and we keep it.
export function validateHomeName(raw: string | undefined | null): string | null {
  if (typeof raw !== "string") return null;
  const trimmed = raw.trim();
  if (trimmed.length < 1 || trimmed.length > 64) return null;
  // Reject control chars (0x00-0x1F, 0x7F) — covers tab, newline,
  // null, etc. Everything else (letters, digits, spaces, punct,
  // unicode) is fine.
  if (/[\x00-\x1f\x7f]/.test(trimmed)) return null;
  return trimmed;
}
