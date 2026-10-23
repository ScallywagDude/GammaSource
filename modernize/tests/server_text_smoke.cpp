#include "../server_text_policy.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
using namespace HL2ModernServerText;

static bool Installed(const char *name, void *) {
    return strcmp(name, "d1_trainstation_01") == 0 ||
           strcmp(name, "dm_lockdown") == 0 || strcmp(name, "dm_overwatch") == 0;
}
int main() {
    assert(SafeMapName("d1_trainstation_01"));
    assert(SafeMapName("a-b_c123"));
    assert(!SafeMapName("../dm_overwatch"));
    assert(!SafeMapName(";quit"));
    assert(!SafeMapName(""));
    assert(!SafeMapName("abcdefghijklmnopqrstuvwxyzabcdef")); // 32, not 31
    const char cycle[] = "\xef\xbb\xbf#cycle\r\ndm_lockdown\r\n"
        "// ignored comment\n" "../../invalid\n"
        "a_very_very_very_long_map_name_overflow\n"
        "dm_overwatch // comment\n"
        "; an admin comment\n" "d1_trainstation_01\n";
    MapList list = ParseMapcycle(cycle, sizeof(cycle)-1); // no trailing NUL read
    assert(list.count == 3);
    assert(strcmp(list.name[0], "dm_lockdown") == 0);
    assert(strcmp(list.name[1], "dm_overwatch") == 0);
    assert(strcmp(list.name[2], "d1_trainstation_01") == 0);
    char selected[32] = {0};
    assert(SelectNextMap(list, "dm_lockdown", Installed, 0, selected, sizeof(selected)));
    assert(strcmp(selected, "dm_overwatch") == 0);
    assert(SelectNextMap(list, "d1_trainstation_01", Installed, 0, selected, sizeof(selected)));
    assert(strcmp(selected, "dm_lockdown") == 0);
    assert(SelectNextMap(list, "nonexistent", Installed, 0, selected, sizeof(selected)));
    assert(strcmp(selected, "dm_lockdown") == 0);
    char tiny[3] = {0};
    assert(!SelectNextMap(list, "dm_lockdown", Installed, 0, tiny, sizeof(tiny)));
    assert(tiny[0] == 0);
    // Explicit length means the bytes outside a mapcycle buffer are ignored.
    const char cut[] = {'d','m','_','l','o','c','k','d','o','w','n','\n','e','v','i','l'};
    list = ParseMapcycle(cut, 12);
    assert(list.count == 1);
    assert(strcmp(list.name[0], "dm_lockdown") == 0);
    const char broken[] = {'d','m','_','l','o','c','k','d','o','w','n','\0',
                           'd','m','_','o','v','e','r','w','a','t','c','h'};
    list = ParseMapcycle(broken, sizeof(broken));
    assert(list.count == 1);
    // 239 bytes, four packets: 60, 60, 60, 59.
    char motd[240];
    memset(motd, 'a', sizeof(motd));
    int offset = 0, pieces = 0;
    while (offset < 239) {
        MotdChunk piece = ReadMotdChunk(motd, 239, offset);
        assert(piece.bytes > 0 && piece.bytes <= 60);
        assert(piece.final == (offset + piece.bytes == 239));
        offset += piece.bytes;
        ++pieces;
    }
    assert(offset == 239 && pieces == 4);
    // A full 240-byte file ends on its fourth packet and never requires NUL.
    MotdChunk last = ReadMotdChunk(motd, 240, 180);
    assert(last.bytes == 60 && last.final);
    // Larger files still cannot escape the established 240-byte protocol cap.
    char huge[300]; memset(huge, 'b', sizeof(huge));
    last = ReadMotdChunk(huge, sizeof(huge), 180);
    assert(last.bytes == 60 && last.final);
    // Embedded NULs end the transfer without reading the remainder.
    huge[63] = 0;
    MotdChunk first = ReadMotdChunk(huge, sizeof(huge), 0);
    assert(first.bytes == 60 && !first.final);
    last = ReadMotdChunk(huge, sizeof(huge), 60);
    assert(last.bytes == 3 && last.final);
    assert(ReadMotdChunk(0, 200, 0).bytes == 0);
    assert(ReadMotdChunk(huge, 200, -1).bytes == 0);
    assert(ReadMotdChunk(huge, 200, 200).bytes == 0);
    puts("server text policy tests passed");
    return 0;
}
