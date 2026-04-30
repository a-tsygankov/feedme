#include "views/UsersListView.h"

#include "views/Theme.h"

#include <Arduino.h>
#include <stdio.h>

namespace feedme::views {

namespace {
constexpr int ROW_HEIGHT_PX  = 28;
constexpr int ROW_SPACING_PX = 36;
constexpr int LIST_PAD_X     = 28;
}  // namespace

int UsersListView::rowCount() const {
    if (!roster_) return 1;  // just Done
    const bool hasRoom = roster_->count() < feedme::domain::UserRoster::MAX_USERS;
    return 1 + roster_->count() + (hasRoom ? 1 : 0);
}

void UsersListView::rowText(int idx, char* buf, int bufLen) const {
    if (idx == 0) { snprintf(buf, bufLen, "Done"); return; }
    if (!roster_) { buf[0] = '\0'; return; }
    const int userIdx = idx - 1;
    if (userIdx < roster_->count()) {
        snprintf(buf, bufLen, "%s", roster_->at(userIdx).name);
    } else {
        snprintf(buf, bufLen, "+ Add user");
    }
}

void UsersListView::build(lv_obj_t* parent) {
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

void UsersListView::redraw() {
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

        // User-row labels carry the user's avatar tint (so the colour
        // identifies the user, not the focus). Done / +Add rows fall
        // back to selection-state colour.
        const bool isSel = (offset == 0);
        const int  uIdx  = i - 1;
        const bool isUserRow = roster_
                               && uIdx >= 0
                               && uIdx < roster_->count();
        if (isUserRow) {
            lv_obj_set_style_text_color(labels_[i],
                lv_color_hex(roster_->at(uIdx).avatarColor), 0);
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

void UsersListView::onEnter() {
    selectedIdx_ = 1;
    if (selectedIdx_ >= rowCount()) selectedIdx_ = 0;
    firstRender_ = true;
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void UsersListView::onLeave() {
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void UsersListView::render(const feedme::ports::DisplayFrame&) {
    redraw();
}

const char* UsersListView::handleInput(feedme::ports::TapEvent ev) {
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
            if (selectedIdx_ == 0) return "settings";
            const int userIdx = selectedIdx_ - 1;
            if (userIdx < roster_->count()) {
                // No per-user editor in v0 — name editing needs the
                // captive portal. Log + stay so the press registers.
                Serial.printf("[users] selected '%s' (no editor in v0)\n",
                              roster_->at(userIdx).name);
                return nullptr;
            }
            // Add row.
            const int newIdx = roster_->add();
            if (newIdx >= 0) {
                Serial.printf("[users] added user slot=%d id=%d name='%s'\n",
                              newIdx, roster_->at(newIdx).id,
                              roster_->at(newIdx).name);
            }
            return nullptr;
        }
        default:
            return nullptr;
    }
}

}  // namespace feedme::views
