#pragma once

#include "domain/Mood.h"

#include <stdio.h>
#include <string.h>

namespace feedme::assets {

// All 12 cat poses from the design's cats4/ set. PNGs live on
// LittleFS (firmware/data/cats/*.png on the host, packaged via
// `pio run --target uploadfs`) and LVGL reads them via the lv_fs
// adapter registered as drive 'L:' (adapters/LvglLittleFs).
//
// Locked mood mapping (firmware/design/handoff.md §1) names the 5
// canonical mood cats; the cat editor's slug picker can pick any of
// the 12 for visual variety.
constexpr const char* kAvailableSlugs[] = {
    "A1", "A2", "A3", "A4",
    "B1", "B2", "B3", "B4",
    "C1", "C2", "C3", "C4",
};
constexpr int kAvailableSlugCount =
    sizeof(kAvailableSlugs) / sizeof(kAvailableSlugs[0]);

// Resolve a slug to its filesystem path. Returns a static buffer —
// safe because lv_img_set_src copies the string before returning, so
// successive calls don't race even though the buffer is shared.
inline const char* slugToPath(const char* slug, int sizePx) {
    static char buf[24];
    if (!slug || strlen(slug) < 2) {
        snprintf(buf, sizeof(buf), "L:/cats/c2_%d.png", sizePx);  // fallback = happy
    } else {
        // slug "C2" → "L:/cats/c2_130.png" (lowercase)
        const char a = (slug[0] >= 'A' && slug[0] <= 'Z') ? slug[0] - 'A' + 'a' : slug[0];
        const char b = slug[1];
        snprintf(buf, sizeof(buf), "L:/cats/%c%c_%d.png", a, b, sizePx);
    }
    return buf;
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

// Per the locked mood mapping in firmware/design/handoff.md §1.
// IdleView calls this to render the right pose for the current mood.
inline const char* moodToSlug(feedme::domain::Mood m) {
    using M = feedme::domain::Mood;
    switch (m) {
        case M::Happy:   return "C2";
        case M::Neutral: return "B1";
        case M::Warning:
        case M::Hungry:  return "B2";
        case M::Sleepy:  return "B3";
        case M::Fed:     return "C4";
    }
    return "B1";
}

}  // namespace feedme::assets
