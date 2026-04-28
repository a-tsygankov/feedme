#include "views/ScreenManager.h"

#include <Arduino.h>
#include <string.h>

namespace feedme::views {

void ScreenManager::begin(lv_obj_t* parent) {
    parent_ = parent;
}

void ScreenManager::registerView(IView* view) {
    if (viewCount_ >= MAX_VIEWS || view == nullptr) return;
    views_[viewCount_++] = view;
    view->build(parent_);
    // Build leaves widgets hidden; current_ stays nullptr until first
    // transition().
}

IView* ScreenManager::find(const char* name) const {
    if (!name) return nullptr;
    for (int i = 0; i < viewCount_; ++i) {
        if (views_[i] && strcmp(views_[i]->name(), name) == 0) {
            return views_[i];
        }
    }
    return nullptr;
}

void ScreenManager::transition(const char* name) {
    IView* next = find(name);
    if (!next) {
        Serial.printf("[screen] unknown view '%s' — staying on '%s'\n",
                      name ? name : "(null)",
                      current_ ? current_->name() : "(none)");
        return;
    }
    if (next == current_) return;
    Serial.printf("[screen] %s -> %s\n",
                  current_ ? current_->name() : "(none)",
                  next->name());
    if (current_) current_->onLeave();
    current_ = next;
    current_->onEnter();
}

void ScreenManager::render(const feedme::ports::DisplayFrame& frame) {
    if (current_) current_->render(frame);
}

const char* ScreenManager::handleInput(feedme::ports::TapEvent ev) {
    if (!current_) return nullptr;
    return current_->handleInput(ev);
}

}  // namespace feedme::views
