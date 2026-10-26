#include "../team_balance_policy.h"
#include <cassert>
#include <cstdio>
int main()
{
    using namespace HL2ModernTeams;
    int equal[2]={4,4};
    assert(!PermitsMove(equal,2,0,1));
    int uneven[2]={5,3};
    assert(PermitsMove(uneven,2,0,1));
    assert(!PermitsMove(uneven,2,1,0));
    int mild[2]={3,4};
    assert(!PermitsMove(mild,2,0,1));
    assert(PermitsMove(mild,2,1,0));
    int triple[3]={7,5,3};
    assert(PermitsMove(triple,3,0,2));
    assert(!PermitsMove(triple,3,2,0));
    int one[2]={1,0};
    assert(PermitsMove(one,2,0,1));
    assert(!PermitsMove(one,2,1,0));
    assert(!PermitsMove(one,2,0,0));
    assert(!PermitsMove(one,2,-1,1));
    assert(!PermitsMove(one,2,0,2));
    assert(!PermitsMove(one,0,0,1));
    assert(!PermitsMove(0,2,0,1));
    int negative[2]={4,-1};
    assert(!PermitsMove(negative,2,0,1));
    std::puts("team balance policy tests passed");
    return 0;
}
