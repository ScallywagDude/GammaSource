#ifndef HL2MODERN_TEAM_BALANCE_POLICY_H
#define HL2MODERN_TEAM_BALANCE_POLICY_H
// Standalone C++98 logic. Does not mutate client info, teams, or scores.
namespace HL2ModernTeams
{
    enum { MAX_TRACKED_TEAMS = 32 };
    inline bool PermitsMove(const int *members, int count, int from, int to)
    {
        // Unknown or self-selected teams are handled by the original game rules.
        if (!members || count < 2 || count > MAX_TRACKED_TEAMS ||
            from < 0 || from >= count || to < 0 || to >= count ||
            from == to || members[from] <= 0)
            return false;
        int oldMin = members[0], oldMax = members[0];
        int newMin = MAX_TRACKED_TEAMS * 1024, newMax = 0;
        for (int i=0; i<count; ++i)
        {
            if (members[i] < 0) return false;
            if (members[i] < oldMin) oldMin = members[i];
            if (members[i] > oldMax) oldMax = members[i];
            const int projected = members[i] + (i == to ? 1 : 0) - (i == from ? 1 : 0);
            if (projected < newMin) newMin = projected;
            if (projected > newMax) newMax = projected;
        }
        const int oldSpread = oldMax - oldMin;
        const int newSpread = newMax - newMin;
        // A balanced server stays balanced; an already unbalanced one must
        // become strictly fairer. No forced player moves or kill penalties.
        return newSpread <= 1 || newSpread < oldSpread;
    }
}
#endif
