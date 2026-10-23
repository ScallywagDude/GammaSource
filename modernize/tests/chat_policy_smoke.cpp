// Host-compiler tests for the standalone multiplayer chat policy.
// Built with any C++98-or-newer compiler; no Source or Steam SDK dependency.
#include "../chat_policy.h"
#include <cassert>
#include <cstring>
#include <cstdio>

int main()
{
    using namespace HL2ModernChat;
    char out[32];
    assert(Clean(NULL, out, sizeof(out)) == 0);
    assert(out[0] == 0);
    assert(Clean("\"Hello world\"", out, sizeof(out)) == 11);
    assert(std::strcmp(out, "Hello world") == 0);
    assert(Clean("hello\r\nworld\t!", out, sizeof(out)) == 13);
    assert(std::strcmp(out, "hello world !") == 0);
    assert(Clean("\001\002  \177  ", out, sizeof(out)) == 0);
    assert(!HasVisibleText(out));
    assert(Clean("1234567890", out, 5) == 4);
    assert(std::strcmp(out, "1234") == 0);
    out[0] = 'X';
    assert(Clean("data", out, 1) == 0 && out[0] == 0);
    assert(Clean("anything", out, 0) == 0);
    assert(Clean("\"\"", out, sizeof(out)) == 0);

    Slot s;
    assert(Admit(s, 100, 1.0f, 0.65f));
    assert(!Admit(s, 100, 1.1f, 0.65f));
    assert(Admit(s, 100, 1.65f, 0.65f));
    assert(Admit(s, 101, 1.66f, 0.65f)); // new user on recycled slot
    assert(!Admit(s, 101, 1.67f, 0.65f));
    Reset(s);
    assert(Admit(s, 101, 1.68f, 0.65f));
    assert(Admit(s, 101, 2.0f, 0.0f)); // disabled cooldown
    assert(Admit(s, 101, 2.0f, -2.0f));
    Reset(s);
    assert(Admit(s, 101, 100.0f, 1000.0f)); // cooldown capped at 30
    assert(!Admit(s, 101, 120.0f, 1000.0f));
    assert(Admit(s, 101, 130.0f, 1000.0f));
    assert(Admit(s, 101, 0.0f, 0.65f)); // map transition resets time
    std::puts("chat policy tests passed");
    return 0;
}
