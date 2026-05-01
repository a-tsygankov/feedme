#include "views/ScreenManager.h"

#include <Arduino.h>
#include <string.h>

namespace feedme::views {

void ScreenManager::begin(lv_obj_t* parent) {
    parent_ = parent;
}

void ScreenManager::registerView(IView* view) {
    if (view == nullptr) return;
    if (viewCount_ >= MAX_VIEWS) {
        // Silent drop is a debugging trap — Settings rows that pointed
        // at the dropped views looked dead. Make this loud.
        Serial.printf("[screen] registerView('%s') DROPPED — MAX_VIEWS=%d full\n",
                      view->name() ? view->name() : "(null)", MAX_VIEWS);
        return;
    }
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
    lastTransitionMs_ = millis();   // start the input cooldown
}

void ScreenManager::render(const feedme::ports::DisplayFrame& frame) {
    if (!current_) return;
    current_->render(frame);
    // Poll for self-transitions (Pouring → Fed → Idle). Done after render
    // so the final frame of the outgoing view paints before the swap.
    const char* next = current_->nextView();
    if (next) transition(next);
}

const char* ScreenManager::handleInput(feedme::ports::TapEvent ev) {
    if (!current_) return nullptr;
    // Cooldown: drop input that arrives within TRANSITION_COOLDOWN_MS
    // of the last screen change. This stops the "press carries over"
    // pattern where the same tactile click appears to fire twice —
    // once for navigation, once for whatever the destination view's
    // primary press action is. LongPress / LongTouch are exempt
    // because they're always intentional and we don't want a fast
    // user trapping themselves in a screen.
    if (lastTransitionMs_ != 0
        && ev != feedme::ports::TapEvent::LongPress
        && ev != feedme::ports::TapEvent::LongTouch
        && millis() - lastTransitionMs_ < TRANSITION_COOLDOWN_MS) {
        Serial.printf("[screen] dropped %d during cooldown (%lums)\n",
                      static_cast<int>(ev),
                      static_cast<unsigned long>(millis() - lastTransitionMs_));
        return nullptr;
    }

    // Let the view claim the event first.
    const char* result = current_->handleInput(ev);
    if (result) return result;
    // Fallback: long-press / long-touch is the universal "back up
    // one level" gesture across the whole UI. Each view declares its
    // parent via IView::parent() (default "idle"). Views that want
    // long-press for something else (e.g. Pouring's cancel) can still
    // override handleInput and return their own destination.
    if (ev == feedme::ports::TapEvent::LongPress
        || ev == feedme::ports::TapEvent::LongTouch) {
        return current_->parent();
    }
    return nullptr;
}

}  // namespace feedme::views
