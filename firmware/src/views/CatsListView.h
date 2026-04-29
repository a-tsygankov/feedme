#pragma once

#include "domain/CatRoster.h"
#include "views/IView.h"

namespace feedme::views {

class CatEditView;  // forward declaration — view-to-view setter wiring

// Phase D.5 — Household cat roster list.
//
// Rows: ["← Done", Cat 0, Cat 1, ..., "+ Add cat" (if room)].
// Selected centre row gets accent highlighting; outer rows fade.
//
// Gestures:
//   RotateCW / RotateCCW → move selection
//   Press / Tap          → action on selected row
//                           - Done   → return to Settings
//                           - Cat N  → open CatEditView on that cat
//                           - Add    → roster.add() then open editor on new cat
//   LongPress / LongTouch → routed to LockConfirm by dispatcher.
class CatsListView : public IView {
public:
    static constexpr int MAX_VISIBLE_ROWS = 6;  // Done + 4 cats + Add

    void setRoster(feedme::domain::CatRoster* roster) { roster_ = roster; }
    void setEditTarget(CatEditView* editView) { editView_ = editView; }

    const char* name()   const override { return "catsList"; }
    const char* parent() const override { return "settings"; }
    void  build(lv_obj_t* parent) override;
    void  onEnter() override;
    void  onLeave() override;
    void  render(const feedme::ports::DisplayFrame& frame) override;
    const char* handleInput(feedme::ports::TapEvent ev) override;

private:
    void redraw();
    int  rowCount() const;             // 1 (Done) + N + (room? 1 : 0)
    void rowText(int idx, char* buf, int bufLen) const;

    feedme::domain::CatRoster* roster_ = nullptr;
    CatEditView*               editView_ = nullptr;

    lv_obj_t* root_                          = nullptr;
    lv_obj_t* rows_[MAX_VISIBLE_ROWS]        = {nullptr};
    lv_obj_t* labels_[MAX_VISIBLE_ROWS]      = {nullptr};

    int  selectedIdx_     = 0;
    int  lastDrawnIdx_    = -1;
    int  lastDrawnCount_  = -1;
    bool firstRender_     = true;
};

}  // namespace feedme::views
