#pragma once

#include <stdint.h>

namespace feedme::views {

// FeedMeKnob theme palettes from the design handoff
// (firmware/design/FeedMeKnobScreens.jsx ≈ THEMES). Aubergine is the
// production target; cream and mono are alternates for ergonomic
// experiments. Switch at build-time via -DFEEDME_THEME_<NAME> if needed.
struct Theme {
    uint32_t bg;          // device-idle background (outside any panel)
    uint32_t screenBg;    // active-screen base
    uint32_t ink;         // primary text / icon color
    uint32_t dim;         // secondary text (≈60% ink)
    uint32_t faint;       // tertiary text / dotted ticks (≈30% ink)
    uint32_t accent;      // single-screen highlight (warm pink)
    uint32_t accentSoft;  // accent gradient bottom / hover
    uint32_t line;        // 1.5 px stroke / thin separators
    uint32_t bezel;       // optional ring color when bezel == 'solid'
};

constexpr Theme AUBERGINE = {
    .bg          = 0x1a1226,
    .screenBg    = 0x221636,
    .ink         = 0xf6f1e6,
    .dim         = 0x9c97a4,  // ≈ ink 60% over bg
    .faint       = 0x4f485c,  // ≈ ink 30% over bg
    .accent      = 0xffb3c1,
    .accentSoft  = 0xd96a82,
    .line        = 0x2e2440,  // ≈ ink 15% over bg
    .bezel       = 0x0e0817,
};

constexpr Theme CREAM = {
    .bg          = 0xefe7d6,
    .screenBg    = 0x1a1226,
    .ink         = 0xf6f1e6,
    .dim         = 0x9c97a4,
    .faint       = 0x4f485c,
    .accent      = 0xffb3c1,
    .accentSoft  = 0xd96a82,
    .line        = 0x39314a,
    .bezel       = 0x3a2f23,
};

constexpr Theme MONO = {
    .bg          = 0x0a0a0a,
    .screenBg    = 0x000000,
    .ink         = 0xffffff,
    .dim         = 0x8c8c8c,
    .faint       = 0x474747,
    .accent      = 0xffffff,
    .accentSoft  = 0x888888,
    .line        = 0x333333,
    .bezel       = 0x000000,
};

// `inline constexpr` so the value lives in one place across TUs
// (every translation unit including Theme.h shares the same kTheme).
// Plain `constexpr const Theme&` would get per-TU storage and trip
// the linker with multiple-definition errors.
#if defined(FEEDME_THEME_CREAM)
inline constexpr Theme kTheme = CREAM;
#elif defined(FEEDME_THEME_MONO)
inline constexpr Theme kTheme = MONO;
#else
inline constexpr Theme kTheme = AUBERGINE;
#endif

}  // namespace feedme::views
