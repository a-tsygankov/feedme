#pragma once

#include "domain/MealSchedule.h"
#include "domain/PortionState.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace feedme::domain {

// One cat in the household. The `id` is stable and never reused — it's
// what (future) event rows reference, so renames/deletes don't orphan
// history. `name` is display-only and may change. `slug` indexes into
// the cats4 image set (today: 5 pre-converted slugs).
//
// Sized to fit comfortably on the round screen and in NVS:
//   - id:   uint8_t (max 255 cats — far past any real household)
//   - name: 16 bytes incl. NUL (12 + spare for emoji etc.)
//   - slug: 4 bytes (e.g. "C2\0" + spare)
struct Cat {
    static constexpr int     NAME_CAP            = 16;
    static constexpr int     SLUG_CAP            = 4;
    static constexpr int64_t DEFAULT_THRESHOLD_S = 5 * 3600;  // 5h

    uint8_t      id   = 0;
    char         name[NAME_CAP] = {0};
    char         slug[SLUG_CAP] = {0};
    // Per-cat tunables — Phase E.x. Defaults match what the global
    // values used to ship as. PortionState carries its own dirty flag
    // which CatRoster::consumeDirty() aggregates. Schedule + threshold
    // mutations come through CatRoster setters that mark the roster
    // dirty directly.
    PortionState portion;
    MealSchedule schedule;
    int64_t      hungryThresholdSec = DEFAULT_THRESHOLD_S;
};

// Fixed-capacity household cat roster. Per [handoff.md § "Entities…"]
// cats are 1..N household-scope; today the firmware persists the list
// to NVS only (Phase E backend sync is future work). Max 4 is a v0 cap
// — bump when there's a real reason.
//
// `dirty_` flags pending NVS writes; main.cpp polls consumeDirty() once
// per service tick.
class CatRoster {
public:
    static constexpr int  MAX_CATS = 4;
    static constexpr char DEFAULT_SLUG[] = "C2";  // C2 = happy, the canonical "default cat"
    static constexpr char DEFAULT_NAME[] = "Cat";

    int  count() const { return count_; }
    const Cat& at(int i) const { return cats_[i]; }
    Cat&       at(int i)       { return cats_[i]; }

    // Stable id → slot lookup. Used by FeedingService to route events
    // (which carry Cat::id, not slot index) into the right per-cat
    // FeedingState. Returns -1 if no cat with that id exists; callers
    // should treat that as "fall back to slot 0" for events from a
    // pre-multi-cat era.
    int findSlotById(uint8_t id) const {
        for (int i = 0; i < count_; ++i) {
            if (cats_[i].id == id) return i;
        }
        return -1;
    }

    // Active cat — the one whose per-cat tunables (portion, schedule,
    // threshold) are currently routed into the views. Today defaults
    // to slot 0; when a cat-selector lands (planned: long-rotate from
    // Idle for N≥2 households), it sets this. Not persisted in v0 —
    // each boot starts on the first cat; flip to persisted once the
    // selector ships.
    int  activeCatIdx() const { return (count_ > 0) ? activeCatIdx_ : -1; }
    void setActiveCatIdx(int i) {
        if (i < 0 || i >= count_) return;
        activeCatIdx_ = i;
    }
    const Cat& active() const { return cats_[activeCatIdx_]; }
    Cat&       active()       { return cats_[activeCatIdx_]; }

    // Convenience accessors so views read the active cat's tunables
    // each frame rather than caching a pointer that would go stale
    // when the cat-selector eventually switches active.
    PortionState&       activePortion()       { return active().portion; }
    const PortionState& activePortion() const { return active().portion; }
    const MealSchedule& activeSchedule() const { return active().schedule; }
    int64_t             activeThresholdSec() const { return active().hungryThresholdSec; }
    // Threshold mutator — single point of write so the roster's dirty
    // flag fires (Cat is a POD; no per-field dirty inside).
    void setActiveThresholdSec(int64_t v) {
        if (active().hungryThresholdSec == v) return;
        active().hungryThresholdSec = v;
        dirty_ = true;
    }
    // Schedule slot-hour mutator — same pattern: wraps via MealSchedule
    // and marks the roster dirty if the value actually changed.
    void setActiveSlotHour(int slot, int hour) {
        if (active().schedule.setSlotHour(slot, hour)) dirty_ = true;
    }
    void bumpActiveSlotHour(int slot, int delta) {
        if (active().schedule.bumpSlotHour(slot, delta)) dirty_ = true;
    }

    // Add a new cat with auto-assigned id (max id seen + 1) and default
    // name/slug. Returns the index of the newly-added cat, or -1 if the
    // roster is full.
    int add() {
        if (count_ >= MAX_CATS) return -1;
        Cat& c = cats_[count_];
        c.id = nextId_++;
        snprintf(c.name, Cat::NAME_CAP, "%s %d", DEFAULT_NAME, c.id);
        strncpy(c.slug, DEFAULT_SLUG, Cat::SLUG_CAP - 1);
        c.slug[Cat::SLUG_CAP - 1] = '\0';
        // Reset per-cat tunables — slot may have been reused after
        // clear(). loadFromStorage seeds + clears dirty so the new
        // cat doesn't trip persistence on its own creation.
        c.portion.loadFromStorage(PortionState::DEFAULT_G);
        c.hungryThresholdSec = Cat::DEFAULT_THRESHOLD_S;
        c.schedule = MealSchedule{};
        ++count_;
        dirty_ = true;
        return count_ - 1;
    }

    void setSlug(int i, const char* slug) {
        if (i < 0 || i >= count_ || !slug) return;
        if (strncmp(cats_[i].slug, slug, Cat::SLUG_CAP) == 0) return;
        strncpy(cats_[i].slug, slug, Cat::SLUG_CAP - 1);
        cats_[i].slug[Cat::SLUG_CAP - 1] = '\0';
        dirty_ = true;
    }

    void setName(int i, const char* name) {
        if (i < 0 || i >= count_ || !name) return;
        if (strncmp(cats_[i].name, name, Cat::NAME_CAP) == 0) return;
        strncpy(cats_[i].name, name, Cat::NAME_CAP - 1);
        cats_[i].name[Cat::NAME_CAP - 1] = '\0';
        dirty_ = true;
    }

    // Aggregates roster-level dirties (add/setName/setSlug) and per-cat
    // tunable dirties (PortionState, soon Schedule + Threshold). Call
    // each service tick; persistence walks the roster + per-cat fields.
    bool consumeDirty() {
        bool d = dirty_;
        dirty_ = false;
        for (int i = 0; i < count_; ++i) {
            if (cats_[i].portion.consumeDirty()) d = true;
        }
        return d;
    }

    // Wipe and reseed from persisted state. `count` clamped to MAX_CATS;
    // each loaded record's id contributes to nextId_ tracking so new
    // cats keep climbing past previously-deleted ones.
    void clear() {
        count_  = 0;
        nextId_ = 0;
        dirty_  = false;
    }
    void appendLoaded(uint8_t id, const char* name, const char* slug,
                      int portionGrams = PortionState::DEFAULT_G,
                      int64_t thresholdSec = Cat::DEFAULT_THRESHOLD_S) {
        if (count_ >= MAX_CATS) return;
        Cat& c = cats_[count_];
        c.id = id;
        if (name) { strncpy(c.name, name, Cat::NAME_CAP - 1); c.name[Cat::NAME_CAP - 1] = '\0'; }
        else      { snprintf(c.name, Cat::NAME_CAP, "%s %d", DEFAULT_NAME, id); }
        if (slug) { strncpy(c.slug, slug, Cat::SLUG_CAP - 1); c.slug[Cat::SLUG_CAP - 1] = '\0'; }
        else      { strncpy(c.slug, DEFAULT_SLUG, Cat::SLUG_CAP - 1); c.slug[Cat::SLUG_CAP - 1] = '\0'; }
        c.portion.loadFromStorage(portionGrams);
        c.hungryThresholdSec = thresholdSec;
        // Schedule defaults via MealSchedule's ctor; per-slot
        // persistence is a follow-up (no editor exists yet).
        ++count_;
        if (id >= nextId_) nextId_ = id + 1;
    }
    // Once main.cpp finishes loading on boot it calls this so the
    // load itself doesn't trip consumeDirty.
    void markClean() { dirty_ = false; }

    // First-run seed: add a single default cat so N≥1 always holds.
    void seedDefaultIfEmpty() {
        if (count_ == 0) add();
    }

private:
    Cat     cats_[MAX_CATS]{};
    int     count_        = 0;
    uint8_t nextId_       = 0;
    int     activeCatIdx_ = 0;
    bool    dirty_        = false;
};

}  // namespace feedme::domain
