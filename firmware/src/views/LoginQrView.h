#pragma once

#include "views/IView.h"

#include <stdint.h>

namespace feedme::application { class SyncService; }

namespace feedme::views {

// Phase F — One-shot Login QR for already-paired devices.
//
// Reached from H menu → "Login QR" (a row that's greyed when the
// device isn't paired). On entry:
//   1. Calls SyncService::loginTokenCreate(), which POSTs to
//      /api/auth/login-token-create with the DeviceToken in the
//      Authorization header. Server mints a 32-hex random token bound
//      to this device's home and returns { token, expiresAt }.
//   2. Encodes a QR with URL
//        https://feedme-webapp.pages.dev/qr-login?device=<deviceId>&token=<token>
//   3. Shows a 60-s countdown so the user knows how long to scan.
//
// The QR is single-use: the server marks the row consumed on the first
// /api/auth/login-qr exchange and 410s any replay. After ~60 s of
// inactivity (or on long-press / long-touch), the view returns to
// "home" — the token is left to expire server-side. No need to call
// the cancel endpoint; the row is already worthless past expiresAt.
//
// On loginTokenCreate() failure (network down, 401 = pairing revoked,
// JSON parse error, …), the view shows a brief error splash and
// auto-returns to "home" after 1.5 s. Authoring keeps it dead simple:
// no retry button, no diagnostic detail UI — the user just goes back
// and tries again, OR re-pairs from H → Reset if the 401 path was hit.
class LoginQrView : public IView {
public:
    void setSyncService(feedme::application::SyncService* svc) { svc_ = svc; }

    const char* name()   const override { return "loginQr"; }
    const char* parent() const override { return "home"; }
    void  build(lv_obj_t* parent) override;
    void  onEnter() override;
    void  onLeave() override;
    void  render(const feedme::ports::DisplayFrame& frame) override;
    const char* handleInput(feedme::ports::TapEvent ev) override;
    const char* nextView() override;

private:
    feedme::application::SyncService* svc_ = nullptr;

    lv_obj_t* root_     = nullptr;
    lv_obj_t* titleLbl_ = nullptr;
    lv_obj_t* qrcode_   = nullptr;
    lv_obj_t* timerLbl_ = nullptr;
    lv_obj_t* hintLbl_  = nullptr;

    enum class Phase { Starting, Showing, Failed, Expired };
    Phase    phase_       = Phase::Starting;
    uint32_t enteredMs_   = 0;
    uint32_t failedAtMs_  = 0;
    int      lastSecsLeft_ = -1;

    // QR payload buffer — full URL with deviceId + token. 64+ chars
    // worst-case (host + path + 32-hex token + 16-hex device id);
    // 192 leaves comfortable headroom for future query-string growth.
    char     payloadBuf_[192] = {0};

    // Held-error splash duration before falling back to "home".
    static constexpr uint32_t FAIL_HOLD_MS = 1500;
    // Match server-side QR_TOKEN_TTL_SEC (transparent.ts).
    static constexpr int      TOKEN_TTL_SEC = 60;
};

}  // namespace feedme::views
