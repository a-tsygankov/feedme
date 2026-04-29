#pragma once

#include "views/IView.h"

namespace feedme::views {

// Phase B.4 placeholder. Renders the screen name centred on the theme
// background so tap/press from the menu lands somewhere visible.
// Tap/Press returns to the menu; double-gesture jumps back to idle.
//
// One instance per menu destination (feedConfirm / schedule / quiet /
// settings). Each is removed from LvglDisplay when its real view class
// lands in Phase C — the StubView class itself can stay as a scaffold
// for any future deferred screen.
class StubView : public IView {
public:
    StubView(const char* viewName, const char* label);

    const char* name() const override { return name_; }
    void  build(lv_obj_t* parent) override;
    void  onEnter() override;
    void  onLeave() override;
    void  render(const feedme::ports::DisplayFrame& frame) override;
    const char* handleInput(feedme::ports::TapEvent ev) override;

private:
    const char* name_;
    const char* label_;
    lv_obj_t* root_     = nullptr;
    lv_obj_t* titleLbl_ = nullptr;
    lv_obj_t* hintLbl_  = nullptr;
};

}  // namespace feedme::views
