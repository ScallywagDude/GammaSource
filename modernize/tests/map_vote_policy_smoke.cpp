#include "../map_vote_policy.h"
#include <cassert>
#include <cstring>
#include <cstdio>
int main()
{
    using namespace HL2ModernVote;
    assert(SafeMapName("d1_trainstation_01"));
    assert(SafeMapName("mp_modern_test"));
    assert(!SafeMapName("../autoexec.cfg"));
    assert(!SafeMapName("map;quit"));
    assert(!SafeMapName("map name"));
    assert(!SafeMapName("map\nchangelevel"));
    assert(!SafeMapName(""));
    char overlong[70]; std::memset(overlong, 'a', 69); overlong[69] = 0;
    assert(!SafeMapName(overlong));
    int online[MAX_VOTERS] = {0};
    online[1] = 101; online[2] = 102; online[3] = 103;
    Session v;
    assert(v.Start("mp_modern_test", 10.0f, 30.0f, 120.0f));
    assert(!v.Start("mp_other", 11.0f, 30.0f, 120.0f));
    assert(v.Cast(1,101,YES,11.0f));
    assert(!v.Cast(1,101,YES,12.0f));
    assert(!v.Cast(1,101,NO,13.0f)); // cannot change a vote
    assert(v.Check(online,13.0f)==PENDING);
    Count c = v.Tally(online);
    assert(c.eligible==3 && c.yes==1 && c.needed==2);
    assert(v.Cast(2,102,NO,14.0f));
    assert(v.Check(online,14.0f)==PENDING);
    assert(v.Cast(3,103,YES,15.0f));
    assert(v.Check(online,15.0f)==PASSED);
    assert(!v.active);
    assert(!v.Start("mp_next", 16.0f, 10.0f, 120.0f)); // cooldown
    assert(v.Start("mp_next", 131.0f, 12.0f, 120.0f));
    assert(v.Cast(1, 101, YES, 132.0f));
    v.ClearSlot(1);
    online[1] = 104; // slot reused, cannot inherit 101's vote
    assert(v.Tally(online).yes==0);
    assert(v.Cast(1, 104, YES, 133.0f));
    assert(v.Tally(online).yes==1);
    assert(v.Check(online,144.0f)==EXPIRED);
    assert(v.Start("mp_reset", 0.0f, 30.0f, 10.0f)); // map time rewind
    assert(v.Cast(1,104,YES,1.0f));
    online[2]=0; online[3]=0;
    assert(v.Check(online,1.0f)==REJECTED); // one player cannot ever reach 2 required

    // Regression: all 64 engine player slots must remain addressable.
    int full[MAX_VOTERS] = {0};
    for (int i=1; i<=MAX_PLAYERS; ++i) full[i] = 1000+i;
    assert(v.Start("mp_full", 20.0f, 30.0f, 0.0f));
    assert(v.Cast(64, 1064, YES, 21.0f));
    assert(v.Tally(full).eligible == 64);
    assert(v.Tally(full).yes == 1);
    assert(v.Tally(full).needed == 39); // ceil(60% of 64)

    // Enough explicit NO votes should reject without waiting for timeout.
    v.Reset();
    int three[MAX_VOTERS] = {0};
    three[1] = 1; three[2] = 2; three[3] = 3;
    assert(v.Start("mp_deny", 0.0f, 60.0f, 0.0f));
    assert(v.Cast(1, 1, NO, 1.0f));
    assert(v.Check(three, 1.0f)==PENDING);
    assert(v.Cast(2, 2, NO, 2.0f));
    assert(v.Check(three, 2.0f)==REJECTED);
    assert(!v.active);
    std::puts("map vote policy tests passed");
    return 0;
}
