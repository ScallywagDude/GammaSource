#include "../hl2_player_asset_guard.h"
#include <assert.h>
#include <string.h>

int main()
{
    using HL2ModernPlayerAssets::VertexCompanionPath;
    unsigned char h[80]; memset(h, 0, sizeof(h));
    char out[128];
    memcpy(h+12, "player/gordon.mdl", 18);
    assert(VertexCompanionPath(h, 80, ".dx80.vtx", out, sizeof(out)));
    assert(!strcmp(out, "models/player/gordon.dx80.vtx"));
    assert(VertexCompanionPath(h, 80, ".dx7_3bone.vtx", out, sizeof(out)));
    assert(!strcmp(out, "models/player/gordon.dx7_3bone.vtx"));
    assert(VertexCompanionPath(h, 80, ".dx7_2bone.vtx", out, sizeof(out)));
    assert(!strcmp(out, "models/player/gordon.dx7_2bone.vtx"));
    assert(!VertexCompanionPath(h, 80, ".dx90.vtx", out, sizeof(out)));
    assert(!VertexCompanionPath(h, 80, ".vvd", out, sizeof(out)));
    assert(!VertexCompanionPath(h, 75, ".dx80.vtx", out, sizeof(out)));
    assert(!VertexCompanionPath(h, 80, ".dx80.vtx", out, 12));
    assert(!VertexCompanionPath(0, 80, ".dx80.vtx", out, sizeof(out)));
    assert(!VertexCompanionPath(h, 80, ".dx80.vtx", 0, sizeof(out)));
    memset(h+12, 'a', 64); // unterminated embedded studio name
    assert(!VertexCompanionPath(h, 80, ".dx80.vtx", out, sizeof(out)));
    const char *bad[] = {"../escape.mdl", "foo/../escape.mdl", "foo//bar.mdl", "/abs.mdl",
                         "foo\\bar.mdl", "foo:bar.mdl", "foo/.mdl", "foo.exe", "foo.mdl/", "", ".mdl"};
    for (unsigned int i=0; i<sizeof(bad)/sizeof(bad[0]); ++i)
    {
        memset(h+12, 0, 64); memcpy(h+12, bad[i], strlen(bad[i]));
        assert(!VertexCompanionPath(h, 80, ".dx80.vtx", out, sizeof(out)));
    }
    memset(h+12, 0, 64);
    memcpy(h+12, "long.model.mdl", 15);
    assert(VertexCompanionPath(h, 80, ".dx80.vtx", out, sizeof(out)));
    assert(!strcmp(out, "models/long.model.dx80.vtx"));
    return 0;
}
