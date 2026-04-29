#pragma once

// Registers an `lv_fs_drv_t` that bridges LittleFS into LVGL so
// `lv_img_set_src(obj, "L:/cats/c2_130.png")` works.
//
// Drive letter is **'L'** (for LittleFS). Paths LVGL passes us look like
// `"L:/cats/c2_130.png"`; we strip the `L:` prefix and forward to
// `LittleFS.open()`.
//
// Call once after `LittleFS.begin()` and before any `lv_img_set_src`
// pointing at a file path.
namespace feedme::adapters {

void registerLvglLittleFs();

}  // namespace feedme::adapters
