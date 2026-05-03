#pragma once

#include "views/IView.h"

namespace feedme::application { class SyncService; }

namespace feedme::views {

// Syncing splash. Shown:
//   - immediately after pairing confirmation, for the initial roster sync
//   - manually from H menu → Sync
//   - (Phase E) automatically at sleep-entry / wake-entry when the
//     time-since-lastSyncAt exceeds the configured interval
//
// The HTTP call is synchronous and bounded by WifiNetwork's 5-s
// HTTPClient timeout. While it's in flight the screen freezes — the
// running-dots animation hitches. A long-tap requests cancellation;
// the cancel flag is checked at the request boundaries (before the
// POST starts and after the response lands), so worst-case wait
// after cancel is the 5-s timeout.
//
// On terminal state, transitions back to "idle" regardless of
// success/failure. The user sees the result via the brief "Synced!"
// or "Failed" splash text held for ~1 s before the transition (the
// minDisplayMs grace mechanism is a follow-up — for now we use a
// simple wall-clock timer in render()).
class SyncingView : public IView {
public:
    void setSyncService(feedme::application::SyncService* svc) { svc_ = svc; }

    const char* name()   const override { return "syncing"; }
    const char* parent() const override { return "idle"; }
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
    lv_obj_t* dotsLbl_  = nullptr;
    lv_obj_t* hintLbl_  = nullptr;

    enum class Phase { Starting, Working, Done, Failed, Cancelled };
    Phase     phase_           = Phase::Starting;
    uint32_t  enteredMs_       = 0;
    uint32_t  finishedMs_      = 0;
    uint32_t  lastDotsMs_      = 0;
    int       dotsFrame_       = 0;

    static constexpr uint32_t DOTS_INTERVAL_MS = 350;
    static constexpr uint32_t RESULT_HOLD_MS   = 1000;
};

}  // namespace feedme::views
