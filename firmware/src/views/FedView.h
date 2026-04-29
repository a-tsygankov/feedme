#pragma once

#include "views/IView.h"

namespace feedme::views {

// 06 Fed — fed-cat hero (C4, 130 px), heart accent, "next 13:00" stub.
// Auto-advance to Idle after AUTO_DISMISS_MS; tap/press jumps sooner.
class FedView : public IView {
public:
    static constexpr uint32_t AUTO_DISMISS_MS = 1500;

    const char* name() const override { return "fed"; }
    void  build(lv_obj_t* parent) override;
    void  onEnter() override;
    void  onLeave() override;
    void  render(const feedme::ports::DisplayFrame& frame) override;
    const char* handleInput(feedme::ports::TapEvent ev) override;
    const char* nextView() override;

private:
    lv_obj_t* root_     = nullptr;
    lv_obj_t* catImg_   = nullptr;
    lv_obj_t* heartLbl_ = nullptr;
    lv_obj_t* titleLbl_ = nullptr;
    lv_obj_t* footLbl_  = nullptr;
    uint32_t  enteredMs_ = 0;
    bool      dismissed_ = false;
};

}  // namespace feedme::views
