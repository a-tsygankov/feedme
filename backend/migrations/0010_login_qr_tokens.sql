-- Migration 0010 — one-shot login QR tokens (Phase F).
--
-- The device-side "Login QR" flow mints a short-lived (60 s) random
-- token, returns it for display in a QR code, and waits for the
-- phone to scan + exchange it for a session at /api/auth/login-qr.
-- Each row is consumed exactly once (consumed_at marked) and a
-- subsequent /login-qr with the same token is rejected — replay
-- protection beyond the TTL.
--
-- This intentionally lives in its own table rather than reusing
-- pending_pairings: pairing tokens are minted by the SERVER on
-- POST /api/pair/start, while login-qr tokens are minted by the
-- DEVICE on POST /api/auth/login-token-create. Distinct lifecycles,
-- distinct auth requirements (login-token-create needs a valid
-- DeviceToken; pairing has no auth).
--
-- Idempotent — CREATE TABLE IF NOT EXISTS plus an idempotent index.

CREATE TABLE IF NOT EXISTS login_qr_tokens (
  token       TEXT PRIMARY KEY,         -- 32-char random hex
  device_id   TEXT NOT NULL,
  home_hid    TEXT NOT NULL,            -- looked up from device's pairings row
  created_at  INTEGER NOT NULL,
  expires_at  INTEGER NOT NULL,         -- created_at + 60
  consumed_at INTEGER                   -- NULL until /login-qr exchanges it
);
CREATE INDEX IF NOT EXISTS idx_login_qr_device  ON login_qr_tokens(device_id);
CREATE INDEX IF NOT EXISTS idx_login_qr_expires ON login_qr_tokens(expires_at);
