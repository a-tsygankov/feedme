// Tiny fetch wrapper for the FeedMe Worker API. Reads the bearer
// token from localStorage on every call (so a logout in one tab is
// honoured by sibling tabs on the next call) and surfaces 401 as a
// typed error so the router can bounce the user back to /login.

const STORAGE_TOKEN = "feedme.token";
const STORAGE_HID   = "feedme.hid";

// In dev (`vite dev`) the proxy in vite.config.ts targets the Worker;
// for built artifacts hosted somewhere other than the same origin,
// override at build time via VITE_API_BASE=https://feedme.workers.dev.
const API_BASE = (import.meta.env.VITE_API_BASE as string | undefined) ?? "";

export class ApiError extends Error {
  status: number;
  constructor(status: number, message: string) {
    super(message);
    this.name = "ApiError";
    this.status = status;
  }
}

export const auth = {
  token(): string | null { return localStorage.getItem(STORAGE_TOKEN); },
  hid():   string | null { return localStorage.getItem(STORAGE_HID); },
  set(token: string, hid: string) {
    localStorage.setItem(STORAGE_TOKEN, token);
    localStorage.setItem(STORAGE_HID, hid);
  },
  clear() {
    localStorage.removeItem(STORAGE_TOKEN);
    localStorage.removeItem(STORAGE_HID);
  },
};

interface ApiOpts {
  method?: "GET" | "POST" | "PATCH" | "DELETE";
  body?:   unknown;
}

async function apiRaw<T>(path: string, opts: ApiOpts = {}): Promise<T> {
  const headers: Record<string, string> = {
    "content-type": "application/json",
  };
  const token = auth.token();
  if (token) headers["authorization"] = `Bearer ${token}`;

  const res = await fetch(`${API_BASE}${path}`, {
    method: opts.method ?? "GET",
    headers,
    body: opts.body !== undefined ? JSON.stringify(opts.body) : undefined,
  });

  let payload: unknown = null;
  const text = await res.text();
  if (text) {
    try { payload = JSON.parse(text); } catch { payload = { raw: text }; }
  }

  if (!res.ok) {
    const msg = (payload as { error?: string } | null)?.error ?? `HTTP ${res.status}`;
    throw new ApiError(res.status, msg);
  }
  return payload as T;
}

// ── Domain types — match the JSON shapes from src/cats.ts etc. ────
export interface Cat {
  slotId: number;
  name: string;
  color: number;            // 0xRRGGBB; 0 = "auto" (let backend pick on save)
  slug: string;
  defaultPortionG: number;
  hungryThresholdSec: number;
}

export interface User {
  slotId: number;
  name: string;
  color: number;
}

// ── Auth ──────────────────────────────────────────────────────────
export const api = {
  exists: (hid: string) =>
    apiRaw<{ exists: boolean }>("/api/auth/exists", { method: "POST", body: { hid } }),
  setup: (hid: string, pin: string) =>
    apiRaw<{ token: string; hid: string }>("/api/auth/setup", { method: "POST", body: { hid, pin } }),
  login: (hid: string, pin: string) =>
    apiRaw<{ token: string; hid: string }>("/api/auth/login", { method: "POST", body: { hid, pin } }),

  // Cats CRUD.
  catsList:   ()                                          => apiRaw<{ cats:  Cat[]  }>("/api/cats"),
  catsCreate: (body: Partial<Cat>)                        => apiRaw<{ cat:   Cat   }>("/api/cats",            { method: "POST",   body }),
  catsUpdate: (slotId: number, patch: Partial<Cat>)       => apiRaw<{ cat:   Cat   }>(`/api/cats/${slotId}`,  { method: "PATCH",  body: patch }),
  catsDelete: (slotId: number)                            => apiRaw<{ ok:    true  }>(`/api/cats/${slotId}`,  { method: "DELETE" }),

  // Users CRUD.
  usersList:   ()                                         => apiRaw<{ users: User[] }>("/api/users"),
  usersCreate: (body: Partial<User>)                      => apiRaw<{ user:  User  }>("/api/users",           { method: "POST",   body }),
  usersUpdate: (slotId: number, patch: Partial<User>)     => apiRaw<{ user:  User  }>(`/api/users/${slotId}`, { method: "PATCH",  body: patch }),
  usersDelete: (slotId: number)                           => apiRaw<{ ok:    true  }>(`/api/users/${slotId}`, { method: "DELETE" }),
};
