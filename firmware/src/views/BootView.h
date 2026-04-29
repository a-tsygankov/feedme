#pragma once

#include "views/IView.h"

namespace feedme::views {

// 01 Boot — splash shown for BOOT_DURATION_MS on power-on.
// Cat silhouette (B1 neutral) above "FeedMe" wordmark + three small
// accent dots. Auto-advances to Idle; no input is consumed.
class BootView : public IView {
public:
    static constexpr uint32_t BOOT_DURATION_MS = 1200;

    const char* name() const override { return "boot"; }
    void  build(lv_obj_t* parent) override;
    void  onEnter() override;
    void  onLeave() override;
    void  render(const feedme::ports::DisplayFrame& frame) override;
    const char* handleInput(feedme::ports::TapEvent ev) override { (void)ev; return nullptr; }
    const char* nextView() override;

private:
    lv_obj_t* root_       = nullptr;
    lv_obj_t* catImg_     = nullptr;
    lv_obj_t* nameLbl_    = nullptr;
    lv_obj_t* dot_[3]     = {nullptr, nullptr, nullptr};
    uint32_t  enteredMs_  = 0;
};

}  // namespace feedme::views
