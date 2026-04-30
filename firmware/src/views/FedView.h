#pragma once

#include "domain/CatRoster.h"
#include "domain/UserRoster.h"
#include "views/IView.h"

namespace feedme::views {

// 06 Fed — fed-cat hero (C4, 130 px), heart accent, "next 13:00" stub.
// Auto-advance to Idle after AUTO_DISMISS_MS; tap/press jumps sooner.
//
// When a user roster is wired AND has 2+ entries, the footer reads
// "fed by <Alice>" using currentFeederName(). PouringView used to
// clear the picker right after logging; that clear now happens here
// in onLeave() instead so the attribution survives long enough for
// the user to see it. Single-user homes silently fall back to
// primaryName() (the same behaviour as the rest of the UI).
class FedView : public IView {
public:
    static constexpr uint32_t AUTO_DISMISS_MS = 1500;

    void setUserRoster(feedme::domain::UserRoster* users) { users_ = users; }
    void setCatRoster (feedme::domain::CatRoster*  cats)  { cats_  = cats;  }

    const char* name() const override { return "fed"; }
    void  build(lv_obj_t* parent) override;
    void  onEnter() override;
    void  onLeave() override;
    void  render(const feedme::ports::DisplayFrame& frame) override;
    const char* handleInput(feedme::ports::TapEvent ev) override;
    const char* nextView() override;

private:
    feedme::domain::UserRoster* users_ = nullptr;
    feedme::domain::CatRoster*  cats_  = nullptr;

    lv_obj_t* root_     = nullptr;
    lv_obj_t* catImg_   = nullptr;
    lv_obj_t* heartLbl_ = nullptr;
    lv_obj_t* titleLbl_ = nullptr;
    lv_obj_t* footLbl_  = nullptr;
    uint32_t  enteredMs_ = 0;
    bool      dismissed_ = false;
};

}  // namespace feedme::views
