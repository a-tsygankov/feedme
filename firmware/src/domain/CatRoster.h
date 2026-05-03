#pragma once

#include "domain/MealSchedule.h"
#include "domain/Palette.h"
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
    // 0xRRGGBB tint applied to this cat's avatar PNG and name labels.
    // Auto-assigned at add() / first appendLoaded(); persisted in NVS.
    // Mutable later via the per-cat edit UI (not implemented yet) but
    // not by the rest of the firmware.
    uint32_t     avatarColor = 0xFFFFFF;
    // Per-cat tunables — Phase E.x. Defaults match what the global
    // values used to ship as. PortionState carries its own dirty flag
    // which CatRoster::consumeDirty() aggregates. Schedule + threshold
    // mutations come through CatRoster setters that mark the roster
    // dirty directly.
    PortionState portion;
    MealSchedule schedule;
    int64_t      hungryThresholdSec = DEFAULT_THRESHOLD_S;

    // Sync timestamps (Phase C). createdAt is set once at add() time
    // and is immutable afterwards; updatedAt bumps on every mutation
    // through CatRoster's setters. Both unix seconds. The server's
    // LWW merge picks the side with the higher updatedAt; rows whose
    // updatedAt is 0 (the default) lose every comparison, which is
    // the correct behaviour for "this row was loaded from a pre-sync
    // NVS snapshot and we don't actually know when it was last
    // edited" — the server's value wins and overwrites the unknown.
    int64_t      createdAt = 0;
    int64_t      updatedAt = 0;
};

// Fixed-capacity household cat roster. Per [handoff.md § "Entities…"]
// cats are 1..N household-scope; today the firmware persists the list
// to NVS only (Phase E backend sync is future work). Max 4 is a v0 cap
// — bump when there's a real reason.
//
// `dirty_` flags pending NVS writes; main.cpp polls consumeDirty() once
// per service tick.
// Tombstone — record of a cat that was hard-removed locally and
// hasn't yet been reported to the backend via /api/sync. Lives in a
// small fixed-capacity list inside CatRoster and is persisted to
// NVS so a reboot mid-sync-cycle doesn't lose the deletion.
//
// On the next successful sync, CatRoster::clearPendingDeletes() is
// called and these rows go away. The server retains tombstones
// forever (per the sync handoff spec) so this list staying empty
// after a successful sync doesn't lose any propagation.
struct CatTombstone {
    uint8_t id        = 0;
    int64_t updatedAt = 0;   // unix sec; deletion timestamp drives LWW
};

class CatRoster {
public:
    static constexpr int  MAX_CATS = 4;
    static constexpr char DEFAULT_SLUG[] = "C2";  // C2 = happy, the canonical "default cat"
    static constexpr char DEFAULT_NAME[] = "Cat";

    // Threaded clock — main.cpp ticks this once per service tick so
    // every setter that mutates a cat can stamp updatedAt without
    // reaching for a global clock or having callers pass `now`.
    // Lag is at most one tick (1 s); plenty fine for LWW resolution
    // which only cares about ordering, not microsecond precision.
    void setNow(int64_t nowSec) { now_ = nowSec; }
    int64_t now() const { return now_; }

    int  count() const { return count_; }
    const Cat& at(int i) const { return cats_[i]; }
    Cat&       at(int i)       { return cats_[i]; }

    // Pending tombstones (local hard-deletes not yet acknowledged by
    // the server). SyncService reads these to build the request
    // payload, then calls clearPendingDeletes() on a successful
    // /api/sync response.
    int                  pendingDeleteCount() const { return pendingDeleteCount_; }
    const CatTombstone&  pendingDeleteAt(int i) const { return pendingDeletes_[i]; }
    void clearPendingDeletes() {
        if (pendingDeleteCount_ == 0) return;
        pendingDeleteCount_ = 0;
        dirty_ = true;
    }
    // Used by NVS load to repopulate after a reboot mid-sync-cycle.
    void appendTombstone(uint8_t id, int64_t updatedAt) {
        if (pendingDeleteCount_ >= MAX_CATS) return;
        pendingDeletes_[pendingDeleteCount_].id        = id;
        pendingDeletes_[pendingDeleteCount_].updatedAt = updatedAt;
        ++pendingDeleteCount_;
    }

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
    // threshold) are currently routed into the views. Set by the
    // IdleView cat-selector (rotate when N≥2). Persisted to NVS by
    // main.cpp's tick loop on change so multi-cat households don't
    // reset to slot 0 every reboot. Clamp on load handles the case
    // where the previously-active cat was removed.
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
        active().updatedAt = now_;
        dirty_ = true;
    }
    // Schedule slot-hour mutator — same pattern: wraps via MealSchedule
    // and marks the roster dirty if the value actually changed.
    void setActiveSlotHour(int slot, int hour) {
        if (active().schedule.setSlotHour(slot, hour)) {
            active().updatedAt = now_;
            dirty_ = true;
        }
    }
    void bumpActiveSlotHour(int slot, int delta) {
        if (active().schedule.bumpSlotHour(slot, delta)) {
            active().updatedAt = now_;
            dirty_ = true;
        }
    }

    // Transient per-feed-flow selection — what the next pour will
    // feed. FEED_ALL = -1 → every cat in the roster (default for N>=2);
    // 0..N-1 → only that cat (the "feed separately" path). Set by
    // FeedConfirmView before transition to Pouring; consumed by
    // PouringView in onComplete to drive how many logFeeding calls
    // happen. Not persisted.
    static constexpr int FEED_ALL = -1;
    int  feedSelection() const { return feedSelection_; }
    void setFeedSelection(int slotOrAll) {
        if (slotOrAll == FEED_ALL || (slotOrAll >= 0 && slotOrAll < count_)) {
            feedSelection_ = slotOrAll;
        }
    }

    // Add a new cat with auto-assigned id (max id seen + 1) and default
    // name/slug. Returns the index of the newly-added cat, or -1 if the
    // roster is full.
    // Remove the cat at `slot`. Refuses when count_ <= 1 (preserves
    // the N>=1 invariant from handoff.md) — single-cat households
    // can't delete their only cat. Returns true if the remove went
    // through.
    //
    // The cat's stable id (Cat::id) is NOT reused — `nextId_` keeps
    // climbing — so any persisted events that reference it stay
    // unambiguous in the backend / LittleFS history. UI lookups by id
    // (e.g. history overlay) will fail to find the deleted cat in the
    // roster and fall back to no-name display, which is the right
    // behaviour for "this event is from a cat that no longer exists".
    //
    // Slot indices renumber after removal — cats above `slot` shift
    // down. Persistence layer re-writes slot 0..count-1; stale higher
    // slots in NVS aren't cleared but are ignored on load (only
    // count_ slots are read).
    bool remove(int slot) {
        if (slot < 0 || slot >= count_) return false;
        if (count_ <= 1) return false;  // refuse last cat
        // Capture a tombstone BEFORE the array shift so SyncService
        // can report the deletion to the server on the next sync.
        // The tombstone's updatedAt is the deletion timestamp; LWW
        // on the server side compares this against any conflicting
        // edit from another device.
        if (pendingDeleteCount_ < MAX_CATS) {
            pendingDeletes_[pendingDeleteCount_].id        = cats_[slot].id;
            pendingDeletes_[pendingDeleteCount_].updatedAt = now_;
            ++pendingDeleteCount_;
        }
        for (int i = slot; i < count_ - 1; ++i) {
            cats_[i] = cats_[i + 1];
        }
        --count_;
        // Default-construct the now-vacated tail slot so leftover
        // PortionState dirty flags etc. don't bleed into a future add().
        cats_[count_] = Cat{};
        if (activeCatIdx_ >= count_) activeCatIdx_ = count_ - 1;
        if (feedSelection_ >= count_) feedSelection_ = FEED_ALL;
        dirty_ = true;
        return true;
    }

    int add() {
        if (count_ >= MAX_CATS) return -1;
        Cat& c = cats_[count_];
        c.id = nextId_++;
        snprintf(c.name, Cat::NAME_CAP, "%s %d", DEFAULT_NAME, c.id);
        strncpy(c.slug, DEFAULT_SLUG, Cat::SLUG_CAP - 1);
        c.slug[Cat::SLUG_CAP - 1] = '\0';
        // Round-robin avatar color from the cat palette. id is stable
        // and never reused, so a removed-then-added slot doesn't
        // collide with surviving cats' colors.
        c.avatarColor = autoCatColor(c.id);
        // Reset per-cat tunables — slot may have been reused after
        // clear(). loadFromStorage seeds + clears dirty so the new
        // cat doesn't trip persistence on its own creation.
        c.portion.loadFromStorage(PortionState::DEFAULT_G);
        c.hungryThresholdSec = Cat::DEFAULT_THRESHOLD_S;
        c.schedule = MealSchedule{};
        // Sync timestamps stamped at creation (Phase C). Both fields
        // start equal; updatedAt bumps on every later mutation.
        c.createdAt = now_;
        c.updatedAt = now_;
        ++count_;
        dirty_ = true;
        return count_ - 1;
    }

    void setSlug(int i, const char* slug) {
        if (i < 0 || i >= count_ || !slug) return;
        if (strncmp(cats_[i].slug, slug, Cat::SLUG_CAP) == 0) return;
        strncpy(cats_[i].slug, slug, Cat::SLUG_CAP - 1);
        cats_[i].slug[Cat::SLUG_CAP - 1] = '\0';
        cats_[i].updatedAt = now_;
        dirty_ = true;
    }

    void setName(int i, const char* name) {
        if (i < 0 || i >= count_ || !name) return;
        if (strncmp(cats_[i].name, name, Cat::NAME_CAP) == 0) return;
        strncpy(cats_[i].name, name, Cat::NAME_CAP - 1);
        cats_[i].name[Cat::NAME_CAP - 1] = '\0';
        cats_[i].updatedAt = now_;
        dirty_ = true;
    }

    void setAvatarColor(int i, uint32_t color) {
        if (i < 0 || i >= count_) return;
        if (cats_[i].avatarColor == color) return;
        cats_[i].avatarColor = color;
        cats_[i].updatedAt = now_;
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
                      int64_t thresholdSec = Cat::DEFAULT_THRESHOLD_S,
                      uint32_t avatarColor = 0,
                      int64_t createdAt = 0,
                      int64_t updatedAt = 0) {
        if (count_ >= MAX_CATS) return;
        Cat& c = cats_[count_];
        c.id = id;
        if (name) { strncpy(c.name, name, Cat::NAME_CAP - 1); c.name[Cat::NAME_CAP - 1] = '\0'; }
        else      { snprintf(c.name, Cat::NAME_CAP, "%s %d", DEFAULT_NAME, id); }
        if (slug) { strncpy(c.slug, slug, Cat::SLUG_CAP - 1); c.slug[Cat::SLUG_CAP - 1] = '\0'; }
        else      { strncpy(c.slug, DEFAULT_SLUG, Cat::SLUG_CAP - 1); c.slug[Cat::SLUG_CAP - 1] = '\0'; }
        // 0 sentinel = "no stored color" — fall back to the round-robin
        // assignment so existing cats from a pre-color era get a
        // sensible default on first boot.
        c.avatarColor = (avatarColor != 0) ? avatarColor : autoCatColor(id);
        c.portion.loadFromStorage(portionGrams);
        c.hungryThresholdSec = thresholdSec;
        // Schedule defaults via MealSchedule's ctor; per-slot
        // persistence is a follow-up (no editor exists yet).
        // Sync timestamps — 0 is the sentinel for "loaded from a
        // pre-Phase-C NVS snapshot" (first sync's server response
        // will overwrite both with whatever it has).
        c.createdAt = createdAt;
        c.updatedAt = updatedAt;
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
    int     count_         = 0;
    uint8_t nextId_        = 0;
    int     activeCatIdx_  = 0;
    int     feedSelection_ = -1;  // FEED_ALL by default (N>=2)
    bool    dirty_         = false;
    int64_t now_           = 0;          // wall-clock pushed in by main.cpp
    CatTombstone pendingDeletes_[MAX_CATS]{};
    int          pendingDeleteCount_ = 0;
};

}  // namespace feedme::domain
