#pragma once

#include "domain/UserRoster.h"
#include "views/IView.h"

namespace feedme::views {

// Phase D.6 — Household user roster list.
//
// Same shape as CatsListView: ["← Done", User 0, ..., "+ Add user"].
// No per-user editor in v0 — knob name editing is awful (deferred to
// captive portal). Press on a user row is a no-op for now; the screen
// is primarily a roster + add affordance.
//
// There is intentionally no "set signed-in user" action — multiple
// users may use the same device (per handoff.md § "Entities…"). The
// per-feed attribution picker for N>1 lands separately as a follow-up
// to D.6.
class UsersListView : public IView {
public:
    static constexpr int MAX_VISIBLE_ROWS = 6;

    void setRoster(feedme::domain::UserRoster* roster) { roster_ = roster; }

    const char* name() const override { return "usersList"; }
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
    lv_obj_t* rows_[MAX_VISIBLE_ROWS]        = {nullptr};
    lv_obj_t* labels_[MAX_VISIBLE_ROWS]      = {nullptr};

    int  selectedIdx_     = 0;
    int  lastDrawnIdx_    = -1;
    int  lastDrawnCount_  = -1;
    bool firstRender_     = true;
};

}  // namespace feedme::views
