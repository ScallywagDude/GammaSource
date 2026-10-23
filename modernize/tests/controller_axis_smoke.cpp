#include "../controller_axis_policy.h"
#include <cassert>
#include <cstdio>
int main()
{
    using namespace HL2ModernController;
    assert(ClampDeadzone(-50)==0);
    assert(ClampDeadzone(40000)==30000);
    int x=100, y=-200;
    RadialDeadzone(x,y,7849);
    assert(x==0 && y==0);
    x=0; y=0; RadialDeadzone(x,y,0);
    assert(x==0 && y==0);
    x=16384; y=0; RadialDeadzone(x,y,7849);
    assert(x>8000 && x<12000 && y==0);
    x=-32768; y=32767; RadialDeadzone(x,y,8689);
    assert(x<0 && y>0 && x>=-32768 && y<=32767);
    x=32767; y=0; RadialDeadzone(x,y,8689);
    assert(x==32767 && y==0);
    x=-5000; y=0; RadialDeadzone(x,y,0);
    assert(x == -5000 && y==0);
    std::puts("controller axis tests passed");
}
