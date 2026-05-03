#pragma once

#include "domain/CatRoster.h"
#include "domain/UserRoster.h"
#include "views/IView.h"

namespace feedme::views {

// Home — navigation hub for everything tied to a household identity.
// Reached from the main menu's "H" glyph (replaces the old Quiet entry).
//
// Six items, vertically stacked, rotate-to-select + tap-to-open:
//   Cats     → catsList         (count shown on the right)
//   Users    → usersList        (count shown on the right)
//   Sync     → syncing          (manual sync; paired devices only,
//                                greyed when unpaired)
//   Login QR → loginQr          (Phase F: one-shot QR a phone scans
//                                to log in without typing PIN; paired
//                                devices only, greyed when unpaired)
//   Pair     → pairing          (re-show QR; long-press there resets)
//   Reset    → resetPairConfirm (DELETE /api/pair + wipe NVS + reboot)
//
// Cats/Users were previously under Settings; moved here so Settings can
// stay focused on per-device tunables (Wi-Fi, Wake, Threshold, …) and
// Home owns the household-scoped surface area.
class HomeView : public IView {
public:
    static constexpr int ITEM_COUNT = 6;

    void setRoster    (const feedme::domain::CatRoster*  r) { roster_     = r; }
    void setUserRoster(const feedme::domain::UserRoster* r) { userRoster_ = r; }
    // Optional — when set, the Sync row reads "is paired?" from this
    // pointer and disables the row when unpaired (prevents the user
    // from kicking sync against a backend that'll just 401).
    void setIsPairedSource(const bool* p) { isPaired_ = p; }

    const char* name()   const override { return "home"; }
    const char* parent() const override { return "menu"; }
    void  build(lv_obj_t* parent) override;
    void  onEnter() override;
    void  onLeave() override;
    void  render(const feedme::ports::DisplayFrame& frame) override;
    const char* handleInput(feedme::ports::TapEvent ev) override;

private:
    void redraw();

    const feedme::domain::CatRoster*  roster_     = nullptr;
    const feedme::domain::UserRoster* userRoster_ = nullptr;
    const bool*                       isPaired_   = nullptr;   // greys Sync row when false

    lv_obj_t* root_                          = nullptr;
    lv_obj_t* selectionArc_                  = nullptr;
    lv_obj_t* rowContainers_[ITEM_COUNT]     = {nullptr};
    lv_obj_t* rowIcons_     [ITEM_COUNT]     = {nullptr};
    lv_obj_t* rowLabels_    [ITEM_COUNT]     = {nullptr};
    lv_obj_t* rowValues_    [ITEM_COUNT]     = {nullptr};

    int  selectedIdx_       = 0;
    int  lastDrawnIdx_      = -1;
    int  lastDrawnCatCount_ = -1;
    int  lastDrawnUsrCount_ = -1;
    bool firstRender_       = true;
};

}  // namespace feedme::views
