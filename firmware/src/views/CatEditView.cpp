#include "views/CatEditView.h"

#include "assets/cats/CatSlug.h"
#include "views/Theme.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

namespace feedme::views {

void CatEditView::build(lv_obj_t* parent) {
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, 240, 240);
    lv_obj_center(root_);
    lv_obj_set_style_bg_color(root_, lv_color_hex(kTheme.bg), 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);

    nameLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(nameLbl_, lv_color_hex(kTheme.dim), 0);
    lv_obj_set_style_text_font(nameLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(nameLbl_, "");
    lv_obj_align(nameLbl_, LV_ALIGN_TOP_MID, 0, 36);

    // Cat image preview — defaults to C2 happy at 88 px until redraw
    // populates it from the editing cat's slug.
    catImg_ = lv_img_create(root_);
    lv_img_set_src(catImg_, feedme::assets::slugToPath("C2", 88));
    lv_obj_align(catImg_, LV_ALIGN_CENTER, 0, -8);

    slugLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(slugLbl_, lv_color_hex(kTheme.accent), 0);
    lv_obj_set_style_text_font(slugLbl_, &lv_font_montserrat_18, 0);
    lv_label_set_text(slugLbl_, "C2");
    lv_obj_align(slugLbl_, LV_ALIGN_BOTTOM_MID, 0, -50);

    hint_ = lv_label_create(root_);
    lv_obj_set_style_text_color(hint_, lv_color_hex(kTheme.faint), 0);
    lv_obj_set_style_text_font(hint_, &lv_font_montserrat_14, 0);
    lv_label_set_text(hint_, "TURN POSE  PRESS SAVE");
    lv_obj_set_width(hint_, 160);
    lv_label_set_long_mode(hint_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(hint_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(hint_, LV_ALIGN_BOTTOM_MID, 0, -22);
}

void CatEditView::cycleSlug(int delta) {
    if (!roster_ || catIdx_ < 0 || catIdx_ >= roster_->count()) return;
    const auto& c = roster_->at(catIdx_);
    const int  cur = feedme::assets::slugIndex(c.slug);
    const int  N   = feedme::assets::kAvailableSlugCount;
    const int  next = ((cur + delta) % N + N) % N;
    roster_->setSlug(catIdx_, feedme::assets::kAvailableSlugs[next]);
}

void CatEditView::redraw() {
    if (!roster_ || catIdx_ < 0 || catIdx_ >= roster_->count()) return;
    const auto& c = roster_->at(catIdx_);
    const bool changed = firstRender_
                         || catIdx_ != lastDrawnCatIdx_
                         || strncmp(c.slug, lastDrawnSlug_, 4) != 0;
    if (!changed) return;

    lv_label_set_text(nameLbl_, c.name);
    lv_label_set_text(slugLbl_, c.slug);
    lv_img_set_src(catImg_, feedme::assets::slugToPath(c.slug, 88));

    strncpy(lastDrawnSlug_, c.slug, 3);
    lastDrawnSlug_[3] = '\0';
    lastDrawnCatIdx_ = catIdx_;
    firstRender_     = false;
}

void CatEditView::onEnter() {
    firstRender_ = true;
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void CatEditView::onLeave() {
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void CatEditView::render(const feedme::ports::DisplayFrame&) {
    redraw();
}

const char* CatEditView::handleInput(feedme::ports::TapEvent ev) {
    using E = feedme::ports::TapEvent;
    switch (ev) {
        case E::RotateCW:  cycleSlug(+1); return nullptr;
        case E::RotateCCW: cycleSlug(-1); return nullptr;
        case E::Tap:
        case E::Press:     return "catsList";  // mutations already saved
        default:           return nullptr;
    }
}

}  // namespace feedme::views
