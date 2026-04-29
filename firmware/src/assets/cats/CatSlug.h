#pragma once

#include "assets/cats/cats.h"

#include <string.h>

namespace feedme::assets {

// Slug strings the firmware can render today. The full cats4/ design
// asset set has 12 (A1–C4); only these 5 (the locked mood→cat mapping
// from firmware/design/handoff.md §1) are converted to lv_img_dsc_t.
// Add the remaining 7 in a follow-up if the cat editor needs them.
constexpr const char* kAvailableSlugs[] = {
    "B1",  // neutral
    "B2",  // hungry
    "B3",  // sleepy
    "C2",  // happy
    "C4",  // fed
};
constexpr int kAvailableSlugCount =
    sizeof(kAvailableSlugs) / sizeof(kAvailableSlugs[0]);

// Resolve a slug string to its lv_img_dsc_t pointer at the requested
// size (88 or 130). Returns nullptr if the slug isn't one of the
// available 5 — caller should fall back to a sensible default.
inline const lv_img_dsc_t* slugToImage(const char* slug, int sizePx) {
    if (!slug) return nullptr;
    if (sizePx == 130) {
        if (strcmp(slug, "B1") == 0) return &cat_b1_130;
        if (strcmp(slug, "B2") == 0) return &cat_b2_130;
        if (strcmp(slug, "B3") == 0) return &cat_b3_130;
        if (strcmp(slug, "C2") == 0) return &cat_c2_130;
        if (strcmp(slug, "C4") == 0) return &cat_c4_130;
    } else {  // 88 default
        if (strcmp(slug, "B1") == 0) return &cat_b1_88;
        if (strcmp(slug, "B2") == 0) return &cat_b2_88;
        if (strcmp(slug, "B3") == 0) return &cat_b3_88;
        if (strcmp(slug, "C2") == 0) return &cat_c2_88;
        if (strcmp(slug, "C4") == 0) return &cat_c4_88;
    }
    return nullptr;
}

// Index of the slug in kAvailableSlugs, or 0 if unknown (so callers
// can use it as a starting point for a "rotate through" picker).
inline int slugIndex(const char* slug) {
    if (!slug) return 0;
    for (int i = 0; i < kAvailableSlugCount; ++i) {
        if (strcmp(slug, kAvailableSlugs[i]) == 0) return i;
    }
    return 0;
}

}  // namespace feedme::assets
