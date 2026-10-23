#!/usr/bin/env python3
"""Replace vulnerable 2003 mapcycle/MOTD file parsing with bounded routines.

Maintains original mapcycle order and 240-byte/60-byte MOTD protocol limits.
This script is intentionally idempotent and requires recognizable original code.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=root/'game_shared/multiplay_gamerules.cpp'
raw=p.read_bytes()
crlf=b'\r\n' in raw
s=raw.decode('latin-1').replace('\r\n','\n')
if 'HL2MODERN_BOUNDED_MAPCYCLE_MOTD' not in s:
    anchor='#include <KeyValues.h>'
    assert s.count(anchor)==1
    s=s.replace(anchor,anchor+'\n#include "../modernize/server_text_policy.h"')
    anchor='\tvoid CMultiplayRules :: ChangeLevel( void )'
    begin=s.index(anchor)
    end=s.index('\n\t#define MAX_MOTD_CHUNK',begin)
    replacement='''// HL2MODERN_BOUNDED_MAPCYCLE_MOTD
    static bool HL2Modern_IsInstalledMap(const char *map, void *)
    {
        return engine->IsMapValid(map);
    }

    void CMultiplayRules :: ChangeLevel( void )
    {
        char nextMap[HL2ModernServerText::MAX_MAP_NAME + 1];
        nextMap[0] = 0;
        const char *current = STRING(gpGlobals->mapname);
        const char *mapcfile = mapcyclefile.GetString();
        int fileLength = 0;
        byte *fileData = NULL;
        if (mapcfile && *mapcfile)
            fileData = engine->LoadFileForMe(mapcfile, &fileLength);
        if (fileData)
        {
            if (fileLength > 0)
            {
                // The engine's loaded byte buffer need not end in '\\0'.
                const HL2ModernServerText::MapList maps =
                    HL2ModernServerText::ParseMapcycle(
                        (const char *)fileData, (size_t)fileLength);
                HL2ModernServerText::SelectNextMap(
                    maps, current, HL2Modern_IsInstalledMap, NULL,
                    nextMap, sizeof(nextMap));
            }
            engine->FreeFile(fileData);
        }
        // Without a usable cycle, stay on a valid existing map.
        if (!nextMap[0] && HL2ModernServerText::SafeMapName(current) &&
            engine->IsMapValid(current))
            Q_strncpy(nextMap, current, sizeof(nextMap));
        if (!nextMap[0] && engine->IsMapValid("hldm1"))
            Q_strncpy(nextMap, "hldm1", sizeof(nextMap));
        if (!nextMap[0])
        {
            Warning("CHANGE LEVEL: no valid map in mapcycle or fallback\\n");
            g_fGameOver = false;
            return;
        }
        g_fGameOver = true;
        Msg("CHANGE LEVEL: %s\\n", nextMap);
        engine->ChangeLevel(nextMap, NULL);
    }
'''
    s=s[:begin]+replacement+s[end:]
    anchor='\t#define MAX_MOTD_CHUNK'
    begin=s.index(anchor)
    end=s.index('\n#endif',begin)
    replacement='''    void CMultiplayRules :: SendMOTDToClient( edict_t *client )
    {
        int length = 0;
        byte *data = engine->LoadFileForMe("motd.txt", &length);
        if (!data) return;
        CBasePlayer *player = (CBasePlayer *)CBaseEntity::Instance(client);
        if (player && length > 0)
        {
            int offset = 0;
            while (offset < length && offset < HL2ModernServerText::MOTD_TOTAL_BYTES)
            {
                const HL2ModernServerText::MotdChunk chunk =
                    HL2ModernServerText::ReadMotdChunk(
                        (const char *)data, length, offset);
                if (!chunk.bytes) break;
                CSingleUserRecipientFilter user(player);
                user.MakeReliable();
                UserMessageBegin(user, "MOTD");
                    WRITE_BYTE(chunk.final ? 1 : 0);
                    WRITE_STRING(chunk.text);
                MessageEnd();
                offset += chunk.bytes;
                if (chunk.final) break;
            }
        }
        engine->FreeFile(data);
    }
'''
    s=s[:begin]+replacement+s[end:]
    p.write_bytes((s.replace('\n','\r\n') if crlf else s).encode('latin-1'))
    print('game_shared/multiplay_gamerules.cpp: bounded mapcycle and MOTD installed')
else:
    assert s.count('HL2MODERN_BOUNDED_MAPCYCLE_MOTD')==1
    assert 'HL2ModernServerText::ParseMapcycle' in s
    print('game_shared/multiplay_gamerules.cpp: already patched')
