#pragma once

#include "domain/Mood.h"

#include <lvgl.h>

namespace feedme::adapters {

// Simplified Simon's Cat-style face built from LVGL primitives — no bitmaps,
// per the design constraint in handoff.md. Lives in a 110x110 area and
// renders one of six mood faces. Mood transitions tear down the inner
// widgets (eyes, mouth, accents) and rebuild; the head, ears, whiskers and
// nose are persistent. Real-world mood changes happen every few minutes
// (every few seconds in the 720x simulator), so the rebuild cost is fine.
class CatFace {
public:
    static constexpr int SIZE = 110;

    void begin(lv_obj_t* parent);
    void setMood(feedme::domain::Mood mood);
    // Reposition the whole face within its parent. Call after begin().
    void align(lv_align_t a, int x, int y);

private:
    lv_obj_t* root_   = nullptr;  // 110x110 invisible container
    lv_obj_t* head_   = nullptr;  // round white face
    lv_obj_t* nose_   = nullptr;  // pink dot
    lv_obj_t* earL_   = nullptr;  // triangle approximation
    lv_obj_t* earR_   = nullptr;
    lv_obj_t* mood_   = nullptr;  // parent of mood-specific widgets

    // Persistent point storage for the lv_line widgets we rebuild per mood.
    // LVGL keeps a pointer to these, so they must outlive their widget.
    lv_point_t mouthPts_[4]{};
    lv_point_t leftEyeArc_[3]{};
    lv_point_t rightEyeArc_[3]{};
    lv_point_t whiskerPts_[6][2]{};
};

}  // namespace feedme::adapters
