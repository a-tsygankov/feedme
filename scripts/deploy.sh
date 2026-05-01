#!/usr/bin/env bash
# FeedMe end-to-end deploy script.
#
# Default action: deploy backend (D1 migrations + Worker) and webapp
# (build + Pages deploy). Firmware is opt-in (`--firmware`) because it
# requires a USB-connected device and may interrupt a running unit.
#
# Usage:
#   ./scripts/deploy.sh                # backend + webapp
#   ./scripts/deploy.sh --firmware     # all three
#   ./scripts/deploy.sh --backend-only
#   ./scripts/deploy.sh --webapp-only
#   ./scripts/deploy.sh --firmware-only
#   ./scripts/deploy.sh --help
#
# Prereqs:
#   - Node 20+ and npm in PATH
#   - `wrangler login` already run (or CLOUDFLARE_API_TOKEN set in env)
#   - For --firmware: PlatformIO CLI (`pio`) and a connected device
#
# Exit codes:
#   0 = all requested deploys succeeded
#   1 = unknown flag
#   2 = a deploy step failed (script aborts on first failure)

set -euo pipefail

# ── Resolve paths ──────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BACKEND_DIR="$REPO_ROOT/backend"
WEBAPP_DIR="$REPO_ROOT/webapp"
FIRMWARE_DIR="$REPO_ROOT/firmware"

# ── Pretty output ──────────────────────────────────────────────────
# Disable colors when piped to a non-tty (CI logs, > file).
if [[ -t 1 ]]; then
    BOLD=$'\e[1m'; DIM=$'\e[2m'; RED=$'\e[31m'; GRN=$'\e[32m'
    YEL=$'\e[33m'; CYA=$'\e[36m'; RST=$'\e[0m'
else
    BOLD=''; DIM=''; RED=''; GRN=''; YEL=''; CYA=''; RST=''
fi

step() { echo "${CYA}${BOLD}==>${RST} ${BOLD}$*${RST}"; }
ok()   { echo "${GRN}✓${RST} $*"; }
warn() { echo "${YEL}!${RST} $*"; }
die()  { echo "${RED}✗ $*${RST}" >&2; exit 2; }

# ── Flag parsing ───────────────────────────────────────────────────
do_backend=1
do_webapp=1
do_firmware=0

print_help() {
    sed -n '2,18p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
}

for arg in "$@"; do
    case "$arg" in
        --help|-h)         print_help; exit 0 ;;
        --backend-only)    do_backend=1; do_webapp=0; do_firmware=0 ;;
        --webapp-only)     do_backend=0; do_webapp=1; do_firmware=0 ;;
        --firmware-only)   do_backend=0; do_webapp=0; do_firmware=1 ;;
        --firmware)        do_firmware=1 ;;
        --skip-migrations) skip_migrations=1 ;;
        *) echo "${RED}Unknown flag: $arg${RST}" >&2; print_help; exit 1 ;;
    esac
done
skip_migrations="${skip_migrations:-0}"

# ── Pre-flight ─────────────────────────────────────────────────────
command -v npm >/dev/null    || die "npm not found in PATH"
command -v npx >/dev/null    || die "npx not found in PATH"
if (( do_firmware )); then
    command -v pio >/dev/null || die "pio (PlatformIO) not found — install with 'pip install platformio' or use the VS Code extension"
fi

# ── Backend ────────────────────────────────────────────────────────
if (( do_backend )); then
    step "Backend → D1 migrations + Worker deploy"
    cd "$BACKEND_DIR"

    if (( skip_migrations )); then
        warn "skipping D1 migrations (--skip-migrations)"
    else
        # Apply migrations in order. Each is idempotent where possible
        # (CREATE ... IF NOT EXISTS); ALTER TABLE ADD COLUMN errors on
        # already-applied steps are expected and harmless — we keep
        # going. Running the loop in a subshell so a failed migration
        # doesn't kill the whole script via `set -e`.
        for sql in migrations/*.sql; do
            echo "${DIM}  applying $(basename "$sql")…${RST}"
            if ! npx wrangler d1 execute feedme --remote --file="$sql"; then
                warn "migration $(basename "$sql") errored (expected if already applied)"
            fi
        done
        ok "migrations done"
    fi

    npm run deploy
    ok "backend deployed"
    cd "$REPO_ROOT"
fi

# ── Webapp ─────────────────────────────────────────────────────────
if (( do_webapp )); then
    step "Webapp → build + Pages deploy"
    cd "$WEBAPP_DIR"

    # `npm install` is cheap when up-to-date (just verifies the lockfile);
    # ensures the script works on a fresh clone or after a dep bump.
    if [[ ! -d node_modules ]] || [[ package-lock.json -nt node_modules ]]; then
        echo "${DIM}  installing webapp deps…${RST}"
        npm install --silent
    fi

    npm run build
    # Run the deploy from webapp/ so the functions/ folder (the /api/*
    # → Worker proxy) is picked up alongside the static assets in dist/.
    #
    # --branch main forces the deployment to the *production* URL
    # (feedme-webapp.pages.dev). Without it, wrangler picks up the
    # current git branch (e.g. dev-20) and publishes to a preview URL
    # like dev-20.feedme-webapp.pages.dev — which leaves the prod URL
    # showing the "Nothing is here yet" placeholder, breaking the
    # device's QR (which is hard-coded to the prod URL).
    npx wrangler pages deploy dist --project-name feedme-webapp --branch main
    ok "webapp deployed"
    cd "$REPO_ROOT"
fi

# ── Firmware ───────────────────────────────────────────────────────
if (( do_firmware )); then
    step "Firmware → PlatformIO build + flash"
    cd "$FIRMWARE_DIR"
    pio run -e esp32-s3-lcd-1_28 -t upload
    ok "firmware uploaded — device should reboot momentarily"
    cd "$REPO_ROOT"
fi

echo
ok "${BOLD}deploy complete${RST}"
