#ifndef HL2MODERN_SPAWN_PROTECTION_POLICY_H
#define HL2MODERN_SPAWN_PROTECTION_POLICY_H

// Server-owned, engine-independent spawn protection. C++98; no dynamic memory.
// Slots are 1..64 and user ids are authoritative engine connection identities.
// Protection prevents only damage attributed to OTHER players. World damage,
// drowning, fall damage and self damage are never intercepted by this policy.
namespace HL2ModernSpawn
{
    // MAX_CLIENTS is already an engine macro in real player translation units.
    enum { kMaxClientSlots = 64, SLOT_COUNT = kMaxClientSlots + 1 };
    struct Entry
    {
        int userid;
        float spawnTime;
        float duration;
        bool cancelled;
        Entry() : userid(0), spawnTime(0.0f), duration(0.0f), cancelled(true) {}
    };

    class Guard
    {
    public:
        void Clear(int slot)
        {
            if (slot < 1 || slot >= SLOT_COUNT) return;
            slots_[slot] = Entry();
        }

        void Begin(int slot, int userid, float now, float seconds)
        {
            Clear(slot);
            if (slot < 1 || slot >= SLOT_COUNT || userid <= 0 || !(now >= 0.0f)) return;
            if (!(seconds > 0.0f)) return;
            if (seconds > 10.0f) seconds = 10.0f;
            Entry &e = slots_[slot];
            e.userid = userid;
            e.spawnTime = now;
            e.duration = seconds;
            e.cancelled = false;
        }

        void Cancel(int slot, int userid)
        {
            if (slot < 1 || slot >= SLOT_COUNT || userid <= 0) return;
            Entry &e = slots_[slot];
            if (e.userid == userid) e.cancelled = true;
        }

        bool Protected(int slot, int userid, float now) const
        {
            if (slot < 1 || slot >= SLOT_COUNT || userid <= 0 || !(now >= 0.0f)) return false;
            const Entry &e = slots_[slot];
            return !e.cancelled && e.userid == userid && now >= e.spawnTime &&
                   now - e.spawnTime < e.duration;
        }

    private:
        Entry slots_[SLOT_COUNT];
    };
}
#endif
