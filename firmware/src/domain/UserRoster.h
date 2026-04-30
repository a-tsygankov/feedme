#pragma once

#include "domain/Palette.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace feedme::domain {

// A household member who logs feeds. `id` is stable (event rows
// reference it once the backend learns about user-by-id); `name` is
// display-only and what currently stamps `by` on events.
struct User {
    static constexpr int NAME_CAP = 16;
    uint8_t  id   = 0;
    char     name[NAME_CAP] = {0};
    // 0xRRGGBB tint used for this user's name labels and per-user
    // accents (FedView "by" line, FeederPicker rows, UsersList rows).
    // Auto-assigned at add() / appendLoaded; persisted in NVS.
    uint32_t avatarColor = 0xFFFFFF;
};

// Household user roster.
//
// Per [handoff.md § "Entities…"]: users are 1..N household-scope, and
// — clarified 2026-04-28 — multiple users may use the same device and
// feed the same cat. There is **no** "signed-in user" per device, and
// no mutual-exclusion semantics. The roster is just the set of names
// the household has; attribution at feed-time is a separate concern.
//
// Today's PouringView (v0, while count() == 1) uses primaryName() as
// the `by` value on logged events. Once count() > 1 the Feed flow
// needs an explicit "by whom?" picker before logging — deferred from
// D.6 to a follow-up; see feedmeknob-plan.md.
//
// `dirty_` flags pending NVS writes.
class UserRoster {
public:
    static constexpr int  MAX_USERS    = 4;
    static constexpr char DEFAULT_NAME[] = "User";

    int  count() const { return count_; }
    const User& at(int i) const { return users_[i]; }

    // First-listed user's name, or "you" if the roster is empty.
    // Used as the silent attribution path when count() == 1.
    const char* primaryName() const {
        return (count_ > 0) ? users_[0].name : "you";
    }

    // Transient "who is feeding right now" pointer. Set by
    // FeederPickerView when count() >= 2; cleared by PouringView once
    // the feed is logged. NOT persisted, NOT dirty-tracked — purely
    // a per-feed UI state.
    int currentFeederIdx() const { return currentFeederIdx_; }
    void setCurrentFeeder(int idx) {
        if (idx < 0 || idx >= count_) return;
        currentFeederIdx_ = idx;
    }
    void clearCurrentFeeder() { currentFeederIdx_ = -1; }
    // Name for the next event's `by` field: explicit pick if set, else
    // primary user as fallback (covers count() == 1 path silently).
    const char* currentFeederName() const {
        if (currentFeederIdx_ >= 0 && currentFeederIdx_ < count_) {
            return users_[currentFeederIdx_].name;
        }
        return primaryName();
    }

    int add() {
        if (count_ >= MAX_USERS) return -1;
        User& u = users_[count_];
        u.id = nextId_++;
        snprintf(u.name, User::NAME_CAP, "%s %d", DEFAULT_NAME, u.id);
        u.avatarColor = autoUserColor(u.id);
        ++count_;
        dirty_ = true;
        return count_ - 1;
    }

    void setName(int i, const char* name) {
        if (i < 0 || i >= count_ || !name) return;
        if (strncmp(users_[i].name, name, User::NAME_CAP) == 0) return;
        strncpy(users_[i].name, name, User::NAME_CAP - 1);
        users_[i].name[User::NAME_CAP - 1] = '\0';
        dirty_ = true;
    }

    bool consumeDirty() {
        const bool d = dirty_;
        dirty_ = false;
        return d;
    }

    void clear() {
        count_ = 0;
        nextId_ = 0;
        dirty_ = false;
    }
    void appendLoaded(uint8_t id, const char* name, uint32_t avatarColor = 0) {
        if (count_ >= MAX_USERS) return;
        User& u = users_[count_];
        u.id = id;
        if (name) { strncpy(u.name, name, User::NAME_CAP - 1); u.name[User::NAME_CAP - 1] = '\0'; }
        else      { snprintf(u.name, User::NAME_CAP, "%s %d", DEFAULT_NAME, id); }
        // 0 sentinel = "no stored color" → use round-robin default
        // (handles users from a pre-color era on first boot).
        u.avatarColor = (avatarColor != 0) ? avatarColor : autoUserColor(id);
        ++count_;
        if (id >= nextId_) nextId_ = id + 1;
    }
    void markClean() { dirty_ = false; }

    void seedDefaultIfEmpty() { if (count_ == 0) add(); }

private:
    User    users_[MAX_USERS]{};
    int     count_             = 0;
    uint8_t nextId_            = 0;
    bool    dirty_             = false;
    int     currentFeederIdx_  = -1;  // transient, not persisted
};

}  // namespace feedme::domain
