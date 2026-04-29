#pragma once

#include "domain/UserRoster.h"
#include "views/IView.h"

namespace feedme::views {

// "By whom?" picker — fires from FeedConfirm.Press when the household
// has 2+ users (per [handoff.md § "Multiple users may use the same
// device at once"], devices are shared and there is no remembered
// "current user"; the picker runs every feed).
//
// Rows: ["← Cancel", User 0, User 1, ...]. Press a user → write the
// selection to UserRoster::setCurrentFeeder() and transition to
// "pouring". Press Cancel → return to "feedConfirm" without selecting.
//
// Long-press / long-touch is intercepted by the LockConfirm
// dispatcher; release returns to this view with no selection.
class FeederPickerView : public IView {
public:
    static constexpr int MAX_VISIBLE_ROWS = 5;  // Cancel + up to 4 users

    void setRoster(feedme::domain::UserRoster* roster) { roster_ = roster; }

    const char* name() const override { return "feederPick"; }
    void  build(lv_obj_t* parent) override;
    void  onEnter() override;
    void  onLeave() override;
    void  render(const feedme::ports::DisplayFrame& frame) override;
    const char* handleInput(feedme::ports::TapEvent ev) override;

private:
    void redraw();
    int  rowCount() const;
    void rowText(int idx, char* buf, int bufLen) const;

    feedme::domain::UserRoster* roster_ = nullptr;

    lv_obj_t* root_                          = nullptr;
    lv_obj_t* header_                        = nullptr;
    lv_obj_t* rows_[MAX_VISIBLE_ROWS]        = {nullptr};
    lv_obj_t* labels_[MAX_VISIBLE_ROWS]      = {nullptr};

    int  selectedIdx_     = 0;
    int  lastDrawnIdx_    = -1;
    int  lastDrawnCount_  = -1;
    bool firstRender_     = true;
};

}  // namespace feedme::views
