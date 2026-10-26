#include "../hl2_playermodel_policy.h"
#include <cassert>
#include <cstring>
#include <cstdio>
int main() {
    using namespace HL2ModernPlayerModels;
    assert(IsHL2GameDir("C:\\Steam\\steamapps\\common\\Half-Life 2\\hl2"));
    assert(IsHL2GameDir("/tmp/game/hl2mp"));
    assert(!IsHL2GameDir("/tmp/game/tf2"));
    assert(!IsHL2GameDir("/tmp/game/cstrike"));
    assert(!Find("models/tf2/commando.mdl"));
    assert(!Find("../../models/player.mdl"));
    assert(!Find("models/player.mdl;quit"));
    assert(Count()>=2);
    for(unsigned int i=0;i<Count();++i) {
        assert(Find(kModels[i].path)==&kModels[i]);
        assert(kModels[i].portrait && std::strlen(kModels[i].portrait)>0);
    }
    std::puts("playermodel policy passed");
}
