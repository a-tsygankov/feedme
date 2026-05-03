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
    static constexpr int UUID_LEN = 32;
    static constexpr int UUID_CAP = UUID_LEN + 1;
    uint8_t  id   = 0;
    char     name[NAME_CAP] = {0};
    char     uuid[UUID_CAP] = {0};   // 32-hex; same Phase D semantics as Cat::uuid
    // 0xRRGGBB tint used for this user's name labels and per-user
    // accents (FedView "by" line, FeederPicker rows, UsersList rows).
    // Auto-assigned at add() / appendLoaded; persisted in NVS.
    uint32_t avatarColor = 0xFFFFFF;

    // Sync timestamps (Phase C) — same semantics as Cat. createdAt
    // immutable after add(); updatedAt bumps on every mutation
    // through UserRoster's setters; both unix seconds; 0 is the
    // "loaded from pre-sync NVS" sentinel.
    int64_t createdAt = 0;
    int64_t updatedAt = 0;
};

// Tombstone — pending local hard-delete that hasn't been reported
// to the backend yet. See CatRoster.h's CatTombstone for full
// rationale; same shape, same lifecycle.
struct UserTombstone {
    uint8_t id        = 0;
    int64_t updatedAt = 0;
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

    // Threaded clock for stamping updatedAt on mutations — same
    // pattern as CatRoster::setNow(). main.cpp ticks per second.
    void setNow(int64_t nowSec) { now_ = nowSec; }
    int64_t now() const { return now_; }

    // Pending tombstones (local hard-deletes awaiting sync upload).
    int                   pendingDeleteCount() const { return pendingDeleteCount_; }
    const UserTombstone&  pendingDeleteAt(int i) const { return pendingDeletes_[i]; }
    void clearPendingDeletes() {
        if (pendingDeleteCount_ == 0) return;
        pendingDeleteCount_ = 0;
        dirty_ = true;
    }
    void appendTombstone(uint8_t id, int64_t updatedAt) {
        if (pendingDeleteCount_ >= MAX_USERS) return;
        pendingDeletes_[pendingDeleteCount_].id        = id;
        pendingDeletes_[pendingDeleteCount_].updatedAt = updatedAt;
        ++pendingDeleteCount_;
    }

    int  count() const { return count_; }
    const User& at(int i) const { return users_[i]; }

    // First-listed user's name, or "you" if the roster is empty.
    // Used as the silent attribution path when count() == 1.
    const char* primaryName() const {
        return (count_ > 0) ? users_[0].name : "you";
    }

    // Transient "who is feeding right now" pointer. Set by
    // FeederPickerView when count() >= 2; cleared by FedView::onLeave
    // once the feed is logged. NOT persisted, NOT dirty-tracked —
    // purely a per-feed UI state.
    int currentFeederIdx() const { return currentFeederIdx_; }
    void setCurrentFeeder(int idx) {
        if (idx < 0 || idx >= count_) return;
        currentFeederIdx_ = idx;
    }
    void clearCurrentFeeder() { currentFeederIdx_ = -1; }

    // The user who fed last — persisted across sessions in NVS. Used
    // as the FeederPicker's default focus AND as the silent
    // attribution when the picker is skipped (FEED_ALL+Press fast
    // path, or single-user homes). Updated by PouringView after a
    // successful feed.
    int lastFeederIdx() const {
        return (lastFeederIdx_ >= 0 && lastFeederIdx_ < count_)
               ? lastFeederIdx_ : 0;
    }
    void setLastFeederIdx(int idx) {
        if (idx < 0 || idx >= count_) return;
        if (lastFeederIdx_ == idx) return;
        lastFeederIdx_ = idx;
        lastFeederDirty_ = true;
    }
    void loadLastFeederIdx(int idx) {
        if (idx >= 0 && idx < count_) lastFeederIdx_ = idx;
        else                          lastFeederIdx_ = 0;
        lastFeederDirty_ = false;
    }
    bool consumeLastFeederDirty() {
        const bool d = lastFeederDirty_;
        lastFeederDirty_ = false;
        return d;
    }

    // Name for the next event's `by` field. Resolution order:
    //   1. explicit picker selection (currentFeederIdx_)
    //   2. last user who fed (lastFeederIdx_, persisted across boots)
    //   3. primary user (covers single-user homes silently)
    // Falls through naturally — empty roster lands on primaryName()'s
    // "you" fallback.
    const char* currentFeederName() const {
        if (currentFeederIdx_ >= 0 && currentFeederIdx_ < count_) {
            return users_[currentFeederIdx_].name;
        }
        if (lastFeederIdx_ >= 0 && lastFeederIdx_ < count_) {
            return users_[lastFeederIdx_].name;
        }
        return primaryName();
    }

    int add() {
        if (count_ >= MAX_USERS) return -1;
        User& u = users_[count_];
        u.id = nextId_++;
        snprintf(u.name, User::NAME_CAP, "%s %d", DEFAULT_NAME, u.id);
        u.avatarColor = autoUserColor(u.id);
        u.createdAt = now_;
        u.updatedAt = now_;
        ++count_;
        dirty_ = true;
        return count_ - 1;
    }

    // Remove the user at `slot`. Refuses when count_ <= 1 (preserves
    // the N>=1 invariant — a household always has someone to attribute
    // feeds to). Returns true if the remove went through.
    //
    // The user's stable id is NOT reused (nextId_ keeps climbing) so
    // any persisted events that reference it stay unambiguous in the
    // backend. Slot indices renumber after removal — users above
    // `slot` shift down. The picker selection clears (devices are
    // shared; the next feed picks fresh anyway).
    bool remove(int slot) {
        if (slot < 0 || slot >= count_) return false;
        if (count_ <= 1) return false;
        // Capture tombstone before the array shift — SyncService
        // sends these as is_deleted=true rows on the next /api/sync.
        if (pendingDeleteCount_ < MAX_USERS) {
            pendingDeletes_[pendingDeleteCount_].id        = users_[slot].id;
            pendingDeletes_[pendingDeleteCount_].updatedAt = now_;
            ++pendingDeleteCount_;
        }
        for (int i = slot; i < count_ - 1; ++i) {
            users_[i] = users_[i + 1];
        }
        --count_;
        users_[count_] = User{};
        currentFeederIdx_ = -1;  // any cached selection is now stale
        // Clamp the persisted "last feeder" — if the removed user was
        // the last-fed-by, fall back to slot 0; if a user above the
        // removed slot was last, slide their index down by one.
        if (lastFeederIdx_ == slot) {
            lastFeederIdx_ = 0;
            lastFeederDirty_ = true;
        } else if (lastFeederIdx_ > slot) {
            --lastFeederIdx_;
            lastFeederDirty_ = true;
        }
        dirty_ = true;
        return true;
    }

    void setName(int i, const char* name) {
        if (i < 0 || i >= count_ || !name) return;
        if (strncmp(users_[i].name, name, User::NAME_CAP) == 0) return;
        strncpy(users_[i].name, name, User::NAME_CAP - 1);
        users_[i].name[User::NAME_CAP - 1] = '\0';
        users_[i].updatedAt = now_;
        dirty_ = true;
    }
    void setAvatarColor(int i, uint32_t color) {
        if (i < 0 || i >= count_) return;
        if (users_[i].avatarColor == color) return;
        users_[i].avatarColor = color;
        users_[i].updatedAt = now_;
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
    void appendLoaded(uint8_t id, const char* name, uint32_t avatarColor = 0,
                      int64_t createdAt = 0, int64_t updatedAt = 0,
                      const char* uuid = nullptr) {
        if (count_ >= MAX_USERS) return;
        User& u = users_[count_];
        u.id = id;
        if (name) { strncpy(u.name, name, User::NAME_CAP - 1); u.name[User::NAME_CAP - 1] = '\0'; }
        else      { snprintf(u.name, User::NAME_CAP, "%s %d", DEFAULT_NAME, id); }
        if (uuid) { strncpy(u.uuid, uuid, User::UUID_LEN); u.uuid[User::UUID_LEN] = '\0'; }
        else      { u.uuid[0] = '\0'; }
        // 0 sentinel = "no stored color" → use round-robin default
        // (handles users from a pre-color era on first boot).
        u.avatarColor = (avatarColor != 0) ? avatarColor : autoUserColor(id);
        u.createdAt = createdAt;
        u.updatedAt = updatedAt;
        ++count_;
        if (id >= nextId_) nextId_ = id + 1;
    }
    void markClean() { dirty_ = false; }
    // See CatRoster::markDirty() for the rationale.
    void markDirty() { dirty_ = true; }

    void seedDefaultIfEmpty() { if (count_ == 0) add(); }

private:
    User    users_[MAX_USERS]{};
    int     count_             = 0;
    uint8_t nextId_            = 0;
    bool    dirty_             = false;
    int     currentFeederIdx_  = -1;  // transient, not persisted
    int     lastFeederIdx_     = 0;   // persisted; default to slot 0
    bool    lastFeederDirty_   = false;
    int64_t now_               = 0;   // wall-clock pushed in by main.cpp
    UserTombstone pendingDeletes_[MAX_USERS]{};
    int           pendingDeleteCount_ = 0;
};

}  // namespace feedme::domain
