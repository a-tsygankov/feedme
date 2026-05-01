#include "views/CatRemoveView.h"

#include "views/LabelHelpers.h"
#include "views/Theme.h"

#include <Arduino.h>
#include <stdio.h>

namespace feedme::views {

namespace {
constexpr int ROW_HEIGHT_PX  = 28;
constexpr int ROW_SPACING_PX = 32;
constexpr int LIST_PAD_X     = 28;
}

int CatRemoveView::rowCount() const {
    if (!roster_) return 1;  // just Cancel
    return 1 + roster_->count();
}

void CatRemoveView::rowText(int idx, char* buf, int bufLen) const {
    if (idx == 0) { snprintf(buf, bufLen, "Cancel"); return; }
    if (!roster_) { buf[0] = '\0'; return; }
    const int u = idx - 1;
    if (u < roster_->count()) {
        const auto& c = roster_->at(u);
        snprintf(buf, bufLen, "x %s", c.name);
    } else {
        buf[0] = '\0';
    }
}

void CatRemoveView::build(lv_obj_t* parent) {
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, 240, 240);
    lv_obj_center(root_);
    lv_obj_set_style_bg_color(root_, lv_color_hex(kTheme.bg), 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);

    header_ = lv_label_create(root_);
    lv_obj_set_style_text_color(header_, lv_color_hex(kTheme.accent), 0);
    lv_obj_set_style_text_font(header_, &lv_font_montserrat_14, 0);
    lv_label_set_text(header_, "REMOVE  CAT");
    lv_obj_align(header_, LV_ALIGN_TOP_MID, 0, 30);

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

    addBackHint(root_);
}

void CatRemoveView::redraw() {
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
        lv_obj_align(rows_[i], LV_ALIGN_CENTER, 0, offset * ROW_SPACING_PX + 20);

        lv_opa_t opa;
        const int absOff = offset < 0 ? -offset : offset;
        switch (absOff) {
            case 0:  opa = LV_OPA_COVER; break;
            case 1:  opa = LV_OPA_60;    break;
            default: opa = LV_OPA_TRANSP; break;
        }
        lv_obj_set_style_opa(rows_[i], opa, 0);

        const bool isSel = (offset == 0);
        // Selected cat row → accent (red would be more conventional for
        // destructive but our palette doesn't have one — accent serves
        // as "this will act"). Non-selected cat rows wear their own
        // avatar tint so the user can identify which cat is which
        // before highlighting one. Cancel row (i==0) stays dim.
        const int  catIdx = i - 1;
        const bool isCatRow = roster_
                              && catIdx >= 0
                              && catIdx < roster_->count();
        uint32_t color = kTheme.dim;
        if (isSel && i != 0) {
            color = kTheme.accent;
        } else if (isCatRow) {
            color = roster_->at(catIdx).avatarColor;
        }
        lv_obj_set_style_text_color(labels_[i], lv_color_hex(color), 0);

        char buf[32];
        rowText(i, buf, sizeof(buf));
        lv_label_set_text(labels_[i], buf);
    }

    lastDrawnIdx_   = selectedIdx_;
    lastDrawnCount_ = roster_ ? roster_->count() : 0;
    firstRender_    = false;
}

void CatRemoveView::onEnter() {
    selectedIdx_ = 0;  // default focus on Cancel — safer than a cat row
    firstRender_ = true;
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void CatRemoveView::onLeave() {
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void CatRemoveView::render(const feedme::ports::DisplayFrame&) {
    redraw();
}

const char* CatRemoveView::handleInput(feedme::ports::TapEvent ev) {
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
            if (selectedIdx_ == 0) return "catsList";  // Cancel
            const int catSlot = selectedIdx_ - 1;
            if (catSlot < 0 || catSlot >= roster_->count()) return nullptr;
            const uint8_t goneId = roster_->at(catSlot).id;
            if (roster_->remove(catSlot)) {
                Serial.printf("[cats] removed slot=%d id=%d (events under that "
                              "id stay in storage as orphans)\n",
                              catSlot, goneId);
            } else {
                Serial.println("[cats] remove refused — last cat protected");
            }
            return "catsList";
        }
        default:
            return nullptr;
    }
}

}  // namespace feedme::views
