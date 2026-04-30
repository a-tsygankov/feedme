#pragma once

#include "domain/CatRoster.h"
#include "domain/UserRoster.h"
#include "views/IView.h"

namespace feedme::application { class FeedingService; }

namespace feedme::views {

// 05 Pouring — perimeter ring fills 0→100% over POUR_DURATION_MS.
// Per the dev-3 tracker semantics this is a brief logging animation
// only — no motor. On completion logs a feed event via FeedingService
// and self-transitions to Fed.
//
// Cancel: long-tap / long-press during pour returns to menu without
// logging (matches the JSX "hold · cancel" hint).
class PouringView : public IView {
public:
    static constexpr uint32_t POUR_DURATION_MS = 1500;

    void setRoster(feedme::domain::CatRoster* roster) { roster_ = roster; }
    void setFeedingService(feedme::application::FeedingService* svc) { feeding_ = svc; }
    // Owner attribution: pulled live from the UserRoster at log time
    // (currentFeederName(), which returns the picker selection or the
    // primary user as fallback). PouringView also clears the transient
    // picker selection after logging — non-const for that reason.
    void setUserRoster(feedme::domain::UserRoster* roster) { users_ = roster; }

    const char* name()   const override { return "pouring"; }
    const char* parent() const override { return "menu"; }  // long-press = cancel pour
    void  build(lv_obj_t* parent) override;
    void  onEnter() override;
    void  onLeave() override;
    void  render(const feedme::ports::DisplayFrame& frame) override;
    const char* handleInput(feedme::ports::TapEvent ev) override;
    const char* nextView() override;

private:
    feedme::domain::CatRoster*              roster_  = nullptr;
    feedme::application::FeedingService*    feeding_ = nullptr;
    feedme::domain::UserRoster*             users_   = nullptr;

    lv_obj_t* root_       = nullptr;
    lv_obj_t* arcBg_      = nullptr;
    lv_obj_t* arcFg_      = nullptr;
    lv_obj_t* catImg_     = nullptr;
    lv_obj_t* titleLbl_   = nullptr;
    lv_obj_t* progressLbl_ = nullptr;
    lv_obj_t* byLbl_      = nullptr;   // "by Alice" — multi-user only
    lv_obj_t* hintLbl_    = nullptr;

    uint32_t startMs_     = 0;
    bool     completed_   = false;
    bool     cancelled_   = false;
    int      lastSweep_   = -1;
};

}  // namespace feedme::views
