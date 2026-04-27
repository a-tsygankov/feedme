#include "adapters/CatFace.h"

namespace feedme::adapters {

namespace {

// Mockup palette (CatFeederMockups.jsx).
constexpr uint32_t COLOR_BODY    = 0xf4f4ef;  // catWhite
constexpr uint32_t COLOR_OUTLINE = 0x1c1c1c;  // catLine
constexpr uint32_t COLOR_PINK    = 0xffb3c1;  // catPink
constexpr uint32_t COLOR_TEAR    = 0x93c5fd;  // soft blue (hungry tears)

// Geometry: child widgets align relative to root_ center (LV_ALIGN_CENTER).
constexpr int HEAD_SIZE   = 80;
constexpr int HEAD_OFFY   = 6;   // shift face down so ears fit
constexpr int EYE_W_BASE  = 14;
constexpr int EYE_H_BASE  = 16;
constexpr int EYE_DX      = 14;  // horizontal offset of each eye from center
constexpr int EYE_DY      = -2;  // vertical offset of eye centers in face

lv_obj_t* makeFilledCircle(lv_obj_t* parent, int w, int h,
                           uint32_t fill, uint32_t border, int border_w) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(fill), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(o, lv_color_hex(border), 0);
    lv_obj_set_style_border_width(o, border_w, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return o;
}

lv_obj_t* makeLine(lv_obj_t* parent, lv_point_t* pts, uint16_t n,
                   uint32_t color, int width) {
    lv_obj_t* l = lv_line_create(parent);
    lv_line_set_points(l, pts, n);
    lv_obj_set_style_line_color(l, lv_color_hex(color), 0);
    lv_obj_set_style_line_width(l, width, 0);
    lv_obj_set_style_line_rounded(l, true, 0);
    return l;
}

}  // namespace

void CatFace::begin(lv_obj_t* parent) {
    // Invisible 110x110 container that owns everything.
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, SIZE, SIZE);
    lv_obj_set_style_bg_opa(root_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(root_, LV_ALIGN_CENTER, 0, 0);

    // Ears — drawn first so the head circle covers their bases.
    // Each ear is a small filled rotated square; rounding gives a tip-ish look.
    earL_ = makeFilledCircle(root_, 18, 18, COLOR_BODY, COLOR_OUTLINE, 2);
    lv_obj_set_style_radius(earL_, 3, 0);
    lv_obj_set_style_transform_angle(earL_, -300, 0);   // -30 deg in 0.1deg
    lv_obj_align(earL_, LV_ALIGN_CENTER, -22, -32);

    earR_ = makeFilledCircle(root_, 18, 18, COLOR_BODY, COLOR_OUTLINE, 2);
    lv_obj_set_style_radius(earR_, 3, 0);
    lv_obj_set_style_transform_angle(earR_, 300, 0);
    lv_obj_align(earR_, LV_ALIGN_CENTER, 22, -32);

    // Head — overlaps and clips ear bases for a clean silhouette.
    head_ = makeFilledCircle(root_, HEAD_SIZE, HEAD_SIZE,
                             COLOR_BODY, COLOR_OUTLINE, 2);
    lv_obj_align(head_, LV_ALIGN_CENTER, 0, HEAD_OFFY);

    // Whiskers — three on each side of the head.
    for (int side = 0; side < 2; ++side) {
        for (int row = 0; row < 3; ++row) {
            const int idx = side * 3 + row;
            const int sign = side ? 1 : -1;
            whiskerPts_[idx][0] = {static_cast<lv_coord_t>(sign * 14),
                                   static_cast<lv_coord_t>(row * 4)};
            whiskerPts_[idx][1] = {static_cast<lv_coord_t>(sign * 26),
                                   static_cast<lv_coord_t>(row * 4)};
            lv_obj_t* w = makeLine(head_, whiskerPts_[idx], 2,
                                   COLOR_OUTLINE, 1);
            lv_obj_align(w, LV_ALIGN_CENTER, 0, 6);
        }
    }

    // Nose — small pink triangle approximation (rounded rect).
    nose_ = makeFilledCircle(head_, 6, 5, COLOR_PINK, COLOR_OUTLINE, 1);
    lv_obj_set_style_radius(nose_, 2, 0);
    lv_obj_align(nose_, LV_ALIGN_CENTER, 0, 4);

    // Mood-specific layer: rebuilt on every setMood() call.
    mood_ = lv_obj_create(root_);
    lv_obj_set_size(mood_, SIZE, SIZE);
    lv_obj_set_style_bg_opa(mood_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(mood_, 0, 0);
    lv_obj_set_style_pad_all(mood_, 0, 0);
    lv_obj_clear_flag(mood_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(mood_, LV_ALIGN_CENTER, 0, 0);

    setMood(feedme::domain::Mood::Happy);
}

void CatFace::align(lv_align_t a, int x, int y) {
    if (root_) lv_obj_align(root_, a, x, y);
}

void CatFace::setMood(feedme::domain::Mood m) {
    using M = feedme::domain::Mood;
    lv_obj_clean(mood_);

    // Eye centers (relative to the cat's geometric centre at LV_ALIGN_CENTER).
    auto placeOval = [&](int dx, int dy, int w, int h, uint32_t fill) {
        lv_obj_t* e = makeFilledCircle(mood_, w, h, fill, fill, 0);
        lv_obj_align(e, LV_ALIGN_CENTER, dx, dy + EYE_DY);
        return e;
    };

    auto placeShine = [&](int dx, int dy, int r) {
        lv_obj_t* s = makeFilledCircle(mood_, r, r, COLOR_BODY, COLOR_BODY, 0);
        lv_obj_align(s, LV_ALIGN_CENTER, dx, dy + EYE_DY);
        return s;
    };

    switch (m) {
        case M::Happy: {
            // Round glossy eyes + wide smile.
            placeOval(-EYE_DX, 0, EYE_W_BASE, EYE_H_BASE, COLOR_OUTLINE);
            placeOval( EYE_DX, 0, EYE_W_BASE, EYE_H_BASE, COLOR_OUTLINE);
            placeShine(-EYE_DX + 3, -3, 5);
            placeShine( EYE_DX + 3, -3, 5);

            // Smile: 4-point polyline approximating a curve.
            mouthPts_[0] = {-10, 0};
            mouthPts_[1] = {-3,  6};
            mouthPts_[2] = { 3,  6};
            mouthPts_[3] = {10,  0};
            lv_obj_t* mouth = makeLine(mood_, mouthPts_, 4, COLOR_OUTLINE, 2);
            lv_obj_align(mouth, LV_ALIGN_CENTER, 0, 18);
            break;
        }

        case M::Neutral:
        case M::Warning: {
            // Half-lid eyes — black oval with white "lid" hiding the top half.
            placeOval(-EYE_DX, 1, EYE_W_BASE, EYE_H_BASE, COLOR_OUTLINE);
            placeOval( EYE_DX, 1, EYE_W_BASE, EYE_H_BASE, COLOR_OUTLINE);
            placeOval(-EYE_DX, -4, EYE_W_BASE + 2, 9, COLOR_BODY);
            placeOval( EYE_DX, -4, EYE_W_BASE + 2, 9, COLOR_BODY);

            // Flat mouth — single straight line.
            mouthPts_[0] = {-7, 0};
            mouthPts_[1] = { 7, 0};
            lv_obj_t* mouth = makeLine(mood_, mouthPts_, 2, COLOR_OUTLINE, 2);
            lv_obj_align(mouth, LV_ALIGN_CENTER, 0, 18);
            break;
        }

        case M::Hungry: {
            // Bigger, more anguished eyes; tear drops; open yowling mouth.
            placeOval(-EYE_DX, -1, EYE_W_BASE + 2, EYE_H_BASE + 2, COLOR_OUTLINE);
            placeOval( EYE_DX, -1, EYE_W_BASE + 2, EYE_H_BASE + 2, COLOR_OUTLINE);
            placeShine(-EYE_DX + 3, -4, 6);
            placeShine( EYE_DX + 3, -4, 6);

            // Tears — small blue ovals below each eye.
            lv_obj_t* tearL = makeFilledCircle(mood_, 3, 6, COLOR_TEAR, COLOR_TEAR, 0);
            lv_obj_align(tearL, LV_ALIGN_CENTER, -EYE_DX, EYE_DY + 11);
            lv_obj_t* tearR = makeFilledCircle(mood_, 3, 6, COLOR_TEAR, COLOR_TEAR, 0);
            lv_obj_align(tearR, LV_ALIGN_CENTER,  EYE_DX, EYE_DY + 11);

            // Open D-shape mouth: filled black oval.
            lv_obj_t* mouth = makeFilledCircle(mood_, 14, 12,
                                               COLOR_OUTLINE, COLOR_OUTLINE, 0);
            lv_obj_align(mouth, LV_ALIGN_CENTER, 0, 20);
            break;
        }

        case M::Fed: {
            // Squint ^_^ eyes drawn as 3-point polylines.
            leftEyeArc_[0]  = {-5, 3};
            leftEyeArc_[1]  = { 0, -2};
            leftEyeArc_[2]  = { 5, 3};
            rightEyeArc_[0] = {-5, 3};
            rightEyeArc_[1] = { 0, -2};
            rightEyeArc_[2] = { 5, 3};
            lv_obj_t* eL = makeLine(mood_, leftEyeArc_, 3, COLOR_OUTLINE, 3);
            lv_obj_align(eL, LV_ALIGN_CENTER, -EYE_DX, EYE_DY);
            lv_obj_t* eR = makeLine(mood_, rightEyeArc_, 3, COLOR_OUTLINE, 3);
            lv_obj_align(eR, LV_ALIGN_CENTER,  EYE_DX, EYE_DY);

            // Big satisfied grin — wider than the happy smile.
            mouthPts_[0] = {-12, 0};
            mouthPts_[1] = { -4, 7};
            mouthPts_[2] = {  4, 7};
            mouthPts_[3] = { 12, 0};
            lv_obj_t* mouth = makeLine(mood_, mouthPts_, 4, COLOR_OUTLINE, 3);
            lv_obj_align(mouth, LV_ALIGN_CENTER, 0, 18);
            break;
        }

        case M::Sleepy: {
            // Droopy eyes — dark ovals mostly covered by a heavy lid arc.
            placeOval(-EYE_DX, 2, EYE_W_BASE, EYE_H_BASE - 4, COLOR_OUTLINE);
            placeOval( EYE_DX, 2, EYE_W_BASE, EYE_H_BASE - 4, COLOR_OUTLINE);
            placeOval(-EYE_DX, -3, EYE_W_BASE + 2, 9, COLOR_BODY);
            placeOval( EYE_DX, -3, EYE_W_BASE + 2, 9, COLOR_BODY);

            // Small contented curve mouth.
            mouthPts_[0] = {-7, 0};
            mouthPts_[1] = { 0, 3};
            mouthPts_[2] = { 7, 0};
            lv_obj_t* mouth = makeLine(mood_, mouthPts_, 3, COLOR_OUTLINE, 2);
            lv_obj_align(mouth, LV_ALIGN_CENTER, 0, 18);

            // "Z" floating off to the upper right.
            lv_obj_t* z = lv_label_create(mood_);
            lv_label_set_text(z, "Z");
            lv_obj_set_style_text_color(z, lv_color_hex(0x6b6b80), 0);
            lv_obj_set_style_text_font(z, &lv_font_montserrat_14, 0);
            lv_obj_align(z, LV_ALIGN_CENTER, 28, -30);
            break;
        }
    }
}

}  // namespace feedme::adapters
