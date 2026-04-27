#pragma once

#include "ports/IDisplay.h"

#include <lvgl.h>
#include <TFT_eSPI.h>

namespace feedme::adapters {

// LVGL + TFT_eSPI implementation of IDisplay.
// Layout: outer arc ring (color = mood), inner solid circle (cat face placeholder),
// big mood label centered, three meal dots at the bottom.
class LvglDisplay : public feedme::ports::IDisplay {
public:
    void begin() override;
    void render(const feedme::ports::DisplayFrame& frame) override;
    void tick() override;

private:
    // LVGL widgets owned by us.
    lv_obj_t* arc_     = nullptr;
    lv_obj_t* face_    = nullptr;
    lv_obj_t* moodLbl_ = nullptr;
    lv_obj_t* timeLbl_ = nullptr;
    lv_obj_t* dots_[3] = {nullptr, nullptr, nullptr};

    feedme::ports::DisplayFrame lastFrame_{};
    bool firstRender_ = true;

    void buildScene();
};

}  // namespace feedme::adapters
