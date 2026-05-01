# FeedMe end-to-end deploy script (PowerShell).
#
# Mirror of scripts/deploy.sh for native PowerShell use on Windows
# (the .sh version works in Git Bash / WSL too). Same flag semantics.
#
# ASCII-only on purpose: Windows PowerShell 5.1 reads .ps1 files as the
# system ANSI codepage unless the file has a UTF-8 BOM, and any
# unicode arrow / em-dash / checkmark gets mangled into garbage that
# breaks string literals. PowerShell 7+ defaults to UTF-8 and handles
# unicode fine, but 5.1 is still the in-box version on Windows 10/11
# so this script targets the lowest common denominator.
#
# Usage:
#   .\scripts\deploy.ps1                  # backend + webapp
#   .\scripts\deploy.ps1 -Firmware        # all three
#   .\scripts\deploy.ps1 -BackendOnly
#   .\scripts\deploy.ps1 -WebappOnly
#   .\scripts\deploy.ps1 -FirmwareOnly
#   .\scripts\deploy.ps1 -SkipMigrations  # skip D1 migrations
#
# Prereqs:
#   - Node 20+ and npm in PATH
#   - `wrangler login` already run (or CLOUDFLARE_API_TOKEN env var)
#   - For -Firmware: PlatformIO CLI (pio) and a connected device

[CmdletBinding()]
param(
    [switch]$BackendOnly,
    [switch]$WebappOnly,
    [switch]$FirmwareOnly,
    [switch]$Firmware,
    [switch]$SkipMigrations
)

$ErrorActionPreference = 'Stop'

$RepoRoot    = Resolve-Path (Join-Path $PSScriptRoot '..')
$BackendDir  = Join-Path $RepoRoot 'backend'
$WebappDir   = Join-Path $RepoRoot 'webapp'
$FirmwareDir = Join-Path $RepoRoot 'firmware'

# Resolve which steps to run.
$doBackend  = $true
$doWebapp   = $true
$doFirmware = $false
if ($BackendOnly)  { $doBackend = $true;  $doWebapp = $false; $doFirmware = $false }
if ($WebappOnly)   { $doBackend = $false; $doWebapp = $true;  $doFirmware = $false }
if ($FirmwareOnly) { $doBackend = $false; $doWebapp = $false; $doFirmware = $true  }
if ($Firmware -and -not $FirmwareOnly) { $doFirmware = $true }

function Step($msg) { Write-Host "==> $msg" -ForegroundColor Cyan }
function Ok($msg)   { Write-Host "OK  $msg"  -ForegroundColor Green }
function Warn($msg) { Write-Host "!   $msg"  -ForegroundColor Yellow }

# Pre-flight.
if (-not (Get-Command npm -ErrorAction SilentlyContinue)) { throw 'npm not found in PATH' }
if (-not (Get-Command npx -ErrorAction SilentlyContinue)) { throw 'npx not found in PATH' }
if ($doFirmware -and -not (Get-Command pio -ErrorAction SilentlyContinue)) {
    throw 'pio (PlatformIO) not found - install with `pip install platformio` or use the VS Code extension'
}

# -- Backend --------------------------------------------------------
if ($doBackend) {
    Step 'Backend: D1 migrations + Worker deploy'
    Push-Location $BackendDir
    try {
        if ($SkipMigrations) {
            Warn 'skipping D1 migrations (-SkipMigrations)'
        } else {
            # Apply each migration in order. Errors on already-applied
            # ALTER TABLE steps are expected and harmless - keep going.
            Get-ChildItem 'migrations' -Filter '*.sql' | Sort-Object Name | ForEach-Object {
                Write-Host "  applying $($_.Name)..." -ForegroundColor DarkGray
                npx wrangler d1 execute feedme --remote --file=$($_.FullName)
                if ($LASTEXITCODE -ne 0) {
                    Warn "migration $($_.Name) errored (expected if already applied)"
                }
            }
            Ok 'migrations done'
        }

        npm run deploy
        if ($LASTEXITCODE -ne 0) { throw 'backend deploy failed' }
        Ok 'backend deployed'
    } finally { Pop-Location }
}

# -- Webapp ---------------------------------------------------------
if ($doWebapp) {
    Step 'Webapp: build + Pages deploy'
    Push-Location $WebappDir
    try {
        $needsInstall = -not (Test-Path 'node_modules') -or `
                        ((Get-Item 'package-lock.json').LastWriteTime -gt (Get-Item 'node_modules').LastWriteTime)
        if ($needsInstall) {
            Write-Host '  installing webapp deps...' -ForegroundColor DarkGray
            npm install --silent
            if ($LASTEXITCODE -ne 0) { throw 'npm install failed' }
        }

        npm run build
        if ($LASTEXITCODE -ne 0) { throw 'webapp build failed' }

        # Run from webapp/ so functions/ (the /api/* -> Worker proxy)
        # is picked up alongside dist/.
        #
        # --branch main forces the deployment to the *production* URL
        # (feedme-webapp.pages.dev). Without it, wrangler picks up the
        # current git branch (e.g. dev-20) and publishes to a preview
        # URL like dev-20.feedme-webapp.pages.dev -- which leaves the
        # prod URL showing the "Nothing is here yet" placeholder,
        # breaking the device's QR (which is hard-coded to prod).
        npx wrangler pages deploy dist --project-name feedme-webapp --branch main
        if ($LASTEXITCODE -ne 0) { throw 'pages deploy failed' }
        Ok 'webapp deployed'
    } finally { Pop-Location }
}

# -- Firmware -------------------------------------------------------
if ($doFirmware) {
    Step 'Firmware: PlatformIO build + flash'
    Push-Location $FirmwareDir
    try {
        pio run -e esp32-s3-lcd-1_28 -t upload
        if ($LASTEXITCODE -ne 0) { throw 'firmware upload failed' }
        Ok 'firmware uploaded - device should reboot momentarily'
    } finally { Pop-Location }
}

Write-Host ''
Ok 'deploy complete'
