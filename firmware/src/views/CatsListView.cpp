#include "views/CatsListView.h"

#include "views/CatEditView.h"
#include "views/Theme.h"

#include <Arduino.h>
#include <stdio.h>

namespace feedme::views {

namespace {
constexpr int ROW_HEIGHT_PX  = 28;
constexpr int ROW_SPACING_PX = 36;
constexpr int LIST_PAD_X     = 28;

// Slot kinds in display order:
//   0           = "Done"
//   1..N        = cat slots
//   N+1         = "Add cat" (only if N < MAX_CATS)
enum class Slot { Done, Cat, Add };
}  // namespace

int CatsListView::rowCount() const {
    if (!roster_) return 1;  // just Done
    const bool hasRoom    = roster_->count() < feedme::domain::CatRoster::MAX_CATS;
    const bool canRemove  = roster_->count() >= 2;  // preserve N>=1 invariant
    return 1 + roster_->count() + (hasRoom ? 1 : 0) + (canRemove ? 1 : 0);
}

void CatsListView::rowText(int idx, char* buf, int bufLen) const {
    if (idx == 0) { snprintf(buf, bufLen, "Done"); return; }
    if (!roster_) { buf[0] = '\0'; return; }
    const int catIdx = idx - 1;
    if (catIdx < roster_->count()) {
        const auto& c = roster_->at(catIdx);
        snprintf(buf, bufLen, "%s  [%s]", c.name, c.slug);
        return;
    }
    // Past the cats: "+ Add cat" if there's room, then "× Remove cat"
    // if N>=2. Build a small dispatch table so handleInput stays in
    // sync with rowText without duplicating the math.
    const int afterCats = catIdx - roster_->count();
    const bool hasRoom  = roster_->count() < feedme::domain::CatRoster::MAX_CATS;
    if (afterCats == 0 && hasRoom) { snprintf(buf, bufLen, "+ Add cat"); return; }
    snprintf(buf, bufLen, "x Remove cat");
}

void CatsListView::build(lv_obj_t* parent) {
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, 240, 240);
    lv_obj_center(root_);
    lv_obj_set_style_bg_color(root_, lv_color_hex(kTheme.bg), 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);

    for (int i = 0; i < MAX_VISIBLE_ROWS; ++i) {
        rows_[i] = lv_obj_create(root_);
        lv_obj_set_size(rows_[i], 240 - 2 * LIST_PAD_X, ROW_HEIGHT_PX);
        lv_obj_set_style_bg_opa(rows_[i], LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(rows_[i], 0, 0);
        lv_obj_set_style_pad_all(rows_[i], 0, 0);
        lv_obj_set_style_radius(rows_[i], 0, 0);
        lv_obj_clear_flag(rows_[i],
                          LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

        labels_[i] = lv_label_create(rows_[i]);
        lv_obj_set_style_text_font(labels_[i], &lv_font_montserrat_14, 0);
        lv_label_set_text(labels_[i], "");
        lv_obj_center(labels_[i]);
    }
}

void CatsListView::redraw() {
    const int count = rowCount();
    if (!firstRender_
        && selectedIdx_ == lastDrawnIdx_
        && (roster_ ? roster_->count() : 0) == lastDrawnCount_) return;

    for (int i = 0; i < MAX_VISIBLE_ROWS; ++i) {
        if (i >= count) {
            lv_obj_add_flag(rows_[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(rows_[i], LV_OBJ_FLAG_HIDDEN);

        const int offset = i - selectedIdx_;
        lv_obj_align(rows_[i], LV_ALIGN_CENTER, 0, offset * ROW_SPACING_PX);

        lv_opa_t opa;
        const int absOff = offset < 0 ? -offset : offset;
        switch (absOff) {
            case 0:  opa = LV_OPA_COVER; break;
            case 1:  opa = LV_OPA_60;    break;
            default: opa = LV_OPA_TRANSP; break;
        }
        lv_obj_set_style_opa(rows_[i], opa, 0);

        // Cat-row labels carry the cat's avatar tint regardless of
        // selection (so the colour identifies the cat, not the focus).
        // Done / +Add / ×Remove rows use the selection state colour
        // — they aren't tied to any specific cat.
        const bool isSel = (offset == 0);
        const int  catIdx = i - 1;
        const bool isCatRow = roster_
                              && catIdx >= 0
                              && catIdx < roster_->count();
        if (isCatRow) {
            lv_obj_set_style_text_color(labels_[i],
                lv_color_hex(roster_->at(catIdx).avatarColor), 0);
        } else {
            lv_obj_set_style_text_color(labels_[i],
                lv_color_hex(isSel ? kTheme.accent : kTheme.dim), 0);
        }

        char buf[32];
        rowText(i, buf, sizeof(buf));
        lv_label_set_text(labels_[i], buf);
    }

    lastDrawnIdx_   = selectedIdx_;
    lastDrawnCount_ = roster_ ? roster_->count() : 0;
    firstRender_    = false;
}

void CatsListView::onEnter() {
    selectedIdx_  = 1;   // default focus on first cat (Done is at 0)
    if (selectedIdx_ >= rowCount()) selectedIdx_ = 0;
    firstRender_  = true;
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void CatsListView::onLeave() {
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void CatsListView::render(const feedme::ports::DisplayFrame&) {
    redraw();
}

const char* CatsListView::handleInput(feedme::ports::TapEvent ev) {
    using E = feedme::ports::TapEvent;
    if (!roster_) return nullptr;
    const int N = rowCount();

    switch (ev) {
        case E::RotateCW:
            selectedIdx_ = (selectedIdx_ + 1) % N;
            return nullptr;
        case E::RotateCCW:
            selectedIdx_ = (selectedIdx_ + N - 1) % N;
            return nullptr;
        case E::Tap:
        case E::Press: {
            // Done row.
            if (selectedIdx_ == 0) return "settings";
            const int catIdx = selectedIdx_ - 1;
            // Cat row.
            if (catIdx < roster_->count()) {
                if (editView_) editView_->setEditingCatIndex(catIdx);
                return "catEdit";
            }
            // Tail rows: "+ Add cat" if there's room, then "× Remove cat".
            const int afterCats = catIdx - roster_->count();
            const bool hasRoom  = roster_->count() < feedme::domain::CatRoster::MAX_CATS;
            if (afterCats == 0 && hasRoom) {
                const int newIdx = roster_->add();
                if (newIdx >= 0) {
                    Serial.printf("[cats] added cat slot=%d id=%d\n",
                                  newIdx, roster_->at(newIdx).id);
                    if (editView_) editView_->setEditingCatIndex(newIdx);
                    return "catEdit";
                }
                return nullptr;
            }
            // × Remove cat — opens a sub-list for picking which cat to
            // remove. Selection happens there; this row is the entry
            // point. (Only ever shown when N>=2 per rowCount.)
            return "catRemove";
        }
        default:
            return nullptr;
    }
}

}  // namespace feedme::views
