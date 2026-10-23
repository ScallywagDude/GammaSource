#ifndef HL2MODERN_MAP_VOTE_POLICY_H
#define HL2MODERN_MAP_VOTE_POLICY_H

// C++98, engine-independent map vote rules. The server owns player identity,
// eligibility, clocks, map validation and the final changelevel operation.
namespace HL2ModernVote
{
    // Indexed by player slot, with index 0 reserved. The engine supports
    // player slots 1..64, so the arrays need 65 elements.
    enum { MAX_PLAYERS = 64, MAX_VOTERS = MAX_PLAYERS + 1, MAX_MAP_NAME = 63 };
    enum Result { PENDING, PASSED, REJECTED, EXPIRED, NOT_ACTIVE };
    enum { NOT_VOTED = 0, YES = 1, NO = 2 };

    inline bool SafeMapName(const char *name)
    {
        if (!name || !name[0]) return false;
        unsigned int i = 0;
        for (; name[i] && i < MAX_MAP_NAME + 1; ++i)
        {
            const char c = name[i];
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '_')) return false;
        }
        return i > 0 && i <= MAX_MAP_NAME && name[i] == 0;
    }

    struct Ballot
    {
        int userid;
        int choice;
        Ballot() : userid(0), choice(NOT_VOTED) {}
    };

    struct Count
    {
        int eligible, yes, no, needed;
        Count() : eligible(0), yes(0), no(0), needed(0) {}
    };

    class Session
    {
    public:
        Session() : active(false), deadline(0.0f), next_call(0.0f), last_time(0.0f)
        { map[0] = 0; }

        void Reset()
        {
            active = false;
            deadline = next_call = last_time = 0.0f;
            map[0] = 0;
            for (int i=0; i<MAX_VOTERS; ++i) ClearSlot(i);
        }

        void ClearSlot(int slot)
        {
            if (slot < 0 || slot >= MAX_VOTERS) return;
            ballots[slot].userid = 0;
            ballots[slot].choice = NOT_VOTED;
        }

        // Callers must also verify the map exists and there are >=2 players.
        bool Start(const char *name, float now, float duration, float cooldown)
        {
            RewindIfNeeded(now);
            if (active || now < next_call || !SafeMapName(name)) return false;
            if (duration < 10.0f) duration = 10.0f;
            if (duration > 120.0f) duration = 120.0f;
            if (cooldown < 0.0f) cooldown = 0.0f;
            if (cooldown > 600.0f) cooldown = 600.0f;
            int i=0;
            for (; name[i]; ++i) map[i] = name[i];
            map[i] = 0;
            for (i=0; i<MAX_VOTERS; ++i) ClearSlot(i);
            active = true;
            deadline = now + duration;
            next_call = now + cooldown;
            last_time = now;
            return true;
        }

        bool Cast(int slot, int userid, int choice, float now)
        {
            RewindIfNeeded(now);
            if (!active || now >= deadline || slot < 1 || slot >= MAX_VOTERS ||
                userid <= 0 || (choice != YES && choice != NO)) return false;
            Ballot &b = ballots[slot];
            // A changed userid never inherits a prior occupant's vote.
            if (b.userid != userid) ClearSlot(slot);
            if (b.choice != NOT_VOTED) return false;
            b.userid = userid;
            b.choice = choice;
            return true;
        }

        Count Tally(const int online_userids[MAX_VOTERS]) const
        {
            Count c;
            for (int i=1; i<MAX_VOTERS; ++i)
            {
                if (online_userids[i] <= 0) continue;
                ++c.eligible;
                if (online_userids[i] != ballots[i].userid) continue;
                if (ballots[i].choice == YES) ++c.yes;
                if (ballots[i].choice == NO) ++c.no;
            }
            // 60% approval, rounded up; no single player can pass a vote.
            c.needed = (c.eligible * 3 + 4) / 5;
            if (c.needed < 2) c.needed = 2;
            return c;
        }

        Result Check(const int online_userids[MAX_VOTERS], float now)
        {
            RewindIfNeeded(now);
            if (!active) return NOT_ACTIVE;
            if (now >= deadline) { active = false; return EXPIRED; }
            Count c = Tally(online_userids);
            if (c.eligible >= 2 && c.yes >= c.needed)
            {
                active = false;
                return PASSED;
            }
            // Reject immediately if all remaining eligible voters voting yes
            // would still be insufficient. Do not wait out a doomed ballot.
            const int uncast = c.eligible - c.yes - c.no;
            if (c.yes + uncast < c.needed)
            {
                active = false;
                return REJECTED;
            }
            return PENDING;
        }

        bool active;
        char map[MAX_MAP_NAME + 1];
        float deadline;
        float next_call;

    private:
        void RewindIfNeeded(float now)
        {
            // curtime can rewind across transitions; Reset on LevelInit as well.
            if (now + 0.01f < last_time) Reset();
            last_time = now;
        }
        Ballot ballots[MAX_VOTERS];
        float last_time;
    };
}
#endif
