// Match the engine header environment: this public macro must survive inclusion.
#define MAX_CLIENTS 256
#include "../spawn_protection_policy.h"
#include <cassert>
#include <cstdio>
int main()
{
    assert(MAX_CLIENTS == 256);
    HL2ModernSpawn::Guard guard;
    assert(!guard.Protected(1, 7, 0.0f));
    guard.Begin(1, 7, 100.0f, 3.0f);
    assert(guard.Protected(1, 7, 100.0f));
    assert(guard.Protected(1, 7, 102.99f));
    assert(!guard.Protected(1, 7, 103.0f));
    assert(!guard.Protected(1, 8, 101.0f)); // reused slot, new userid
    assert(!guard.Protected(1, 7, 99.0f)); // map time rewind
    guard.Cancel(1, 8); // wrong identity must not cancel
    assert(guard.Protected(1, 7, 101.0f));
    guard.Cancel(1, 7);
    assert(!guard.Protected(1, 7, 101.0f));
    guard.Begin(1, 8, 1.0f, 100.0f); // duration capped
    assert(guard.Protected(1, 8, 10.99f));
    assert(!guard.Protected(1, 8, 11.0f));
    guard.Begin(64, 8, 4.0f, 1.0f); // last valid slot
    assert(guard.Protected(64, 8, 4.5f));
    guard.Clear(64);
    assert(!guard.Protected(64, 8, 4.5f));
    guard.Begin(0, 8, 1.0f, 2.0f);
    guard.Begin(65, 8, 1.0f, 2.0f);
    assert(!guard.Protected(0, 8, 1.0f));
    assert(!guard.Protected(65, 8, 1.0f));
    guard.Begin(2, 9, 0.0f, -5.0f);
    assert(!guard.Protected(2, 9, 0.0f));
    guard.Begin(2, 9, 0.0f, 0.0f);
    assert(!guard.Protected(2, 9, 0.0f));
    guard.Begin(2, 9, 0.0f, 0.5f);
    assert(guard.Protected(2, 9, 0.25f));
    guard.Clear(2);
    assert(!guard.Protected(2, 9, 0.25f));
    std::puts("spawn protection policy tests passed");
}
