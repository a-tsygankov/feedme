#pragma once

#include "domain/CatRoster.h"
#include "views/IView.h"

namespace feedme::views {

// Cat removal sub-list. Reached from CatsListView's "× Remove cat" row.
// Lists each cat; press a cat row → CatRoster::remove(slot) and bounce
// back to catsList. Refuses to delete the last cat (preserves the
// 1+ cats invariant) — rendered as a no-op press in that case (and
// CatsListView only shows the "× Remove" entry when count() >= 2 in
// the first place, so this is belt-and-braces).
//
// The cat's stable id is never reused; events still in the backend or
// LittleFS history that reference the deleted cat stay attributed to
// the gone-cat id. UI lookups by id will fail and fall back to no-name
// in the history overlay.
class CatRemoveView : public IView {
public:
    static constexpr int MAX_VISIBLE_ROWS = 6;  // Done + up to MAX_CATS

    void setRoster(feedme::domain::CatRoster* roster) { roster_ = roster; }

    const char* name()   const override { return "catRemove"; }
    const char* parent() const override { return "catsList"; }
    void  build(lv_obj_t* parent) override;
    void  onEnter() override;
    void  onLeave() override;
    void  render(const feedme::ports::DisplayFrame& frame) override;
    const char* handleInput(feedme::ports::TapEvent ev) override;

private:
    void redraw();
    int  rowCount() const;
    void rowText(int idx, char* buf, int bufLen) const;

    feedme::domain::CatRoster* roster_ = nullptr;

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
