#pragma once

#include <stdint.h>

namespace feedme::domain {

// Avatar palettes for cat and user profiles. Colors are assigned once
// at profile creation (round-robin by id), persisted in NVS, and only
// changed via the per-profile edit UI (not implemented yet — the
// auto-assignment is the v0 path).
//
// Cat colors (6) — soft, washed-out tints that look right behind the
// monochrome silhouette PNGs when applied via lv_obj_set_style_img_recolor
// at full opacity. Original PNG pixel ARGB multiplies the tint, so
// shading curves stay intact.
//
// User colors (4) — saturated, identifiable. Used for name labels and
// per-user UI affordances (the "by <Alice>" attribution on FedView,
// row labels in FeederPicker / UsersList).

// Stored as 0xRRGGBB. Use lv_color_hex(...) when feeding into LVGL.
struct PaletteColor {
    uint32_t    rgb;
    const char* name;  // for diagnostics / future "pick a color" UI
};

constexpr PaletteColor kCatPalette[] = {
    { 0xF3EAD3, "Linen"        },
    { 0xEBF6F7, "Cup Cake"     },
    { 0xF0F8FF, "Alice Blue"   },
    { 0xF8F4FF, "Magnolia"     },
    { 0xE7FEFF, "Bubbles"      },
    { 0xE3DFD2, "Oyster White" },
};
constexpr int kCatPaletteSize =
    sizeof(kCatPalette) / sizeof(kCatPalette[0]);

constexpr PaletteColor kUserPalette[] = {
    { 0xFF8200, "Princeton Orange"   },
    { 0x50C878, "Emerald"            },
    { 0x40E0D0, "Turquoise"          },
    { 0x7B68EE, "Medium Slate Blue"  },
};
constexpr int kUserPaletteSize =
    sizeof(kUserPalette) / sizeof(kUserPalette[0]);

// Auto-assign helpers — round-robin by id so the first cat / user
// always gets palette[0], the second palette[1], etc. Cat ids are
// stable and never reused (per CatRoster doc), so once assigned a
// removed-then-readded slot doesn't collide colors.
inline uint32_t autoCatColor(uint8_t id) {
    return kCatPalette[id % kCatPaletteSize].rgb;
}
inline uint32_t autoUserColor(uint8_t id) {
    return kUserPalette[id % kUserPaletteSize].rgb;
}

// Sentinel for "white" — used by code paths that show a generic /
// all-cats placeholder image (FeedConfirm in FEED_ALL mode, the
// avatar-picker preview). Not a member of either palette.
constexpr uint32_t kWhiteAvatar = 0xFFFFFF;

}  // namespace feedme::domain
