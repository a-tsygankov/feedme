#pragma once

#include "domain/UserRoster.h"
#include "views/IView.h"

namespace feedme::views {

// User removal sub-list. Reached from UsersListView's "× Remove user"
// row. Mirrors CatRemoveView: lists each user, press → UserRoster::remove
// and bounce back to usersList. Refuses to delete the last user
// (preserves the 1+ users invariant — feeds always need a `by`
// attribution). UsersListView only shows the "× Remove" entry when
// count() >= 2 in the first place, so this is belt-and-braces.
//
// User stable ids are never reused; events still in the backend or
// LittleFS history that reference a deleted user keep the gone id in
// their `by` field — they show as orphan attributions in the history
// overlay, which is the right behaviour for "this was logged by a
// user who no longer exists".
class UserRemoveView : public IView {
public:
    static constexpr int MAX_VISIBLE_ROWS = 6;  // Cancel + up to MAX_USERS

    void setRoster(feedme::domain::UserRoster* roster) { roster_ = roster; }

    const char* name()   const override { return "userRemove"; }
    const char* parent() const override { return "usersList"; }
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
