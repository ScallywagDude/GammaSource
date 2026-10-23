#!/usr/bin/env python3
"""Apply narrowly scoped 2003-era NPC developer console improvements.

The patch is opt-in: original spawn-time precache restriction remains the default.
Idempotent so the overlay can be re-applied when rebuilding the workspace.
"""
from pathlib import Path

p = Path(__file__).resolve().parents[1] / 'dlls' / 'ai_concommands.cpp'
s = p.read_bytes().decode('latin-1').replace('\r\n','\n')
if 'npc_allow_unprecached_create' not in s:
    expected = 'ConVar npc_create_equipment("npc_create_equipment", "");'
    assert expected in s
    s = s.replace(expected, expected + '''
// Cut/unfinished NPC testing: off by default, as some NPCs require map-start
// precaching, models, activities and scene resources unavailable in this tree.
static ConVar npc_allow_unprecached_create(
    "npc_allow_unprecached_create", "0", FCVAR_CHEAT,
    "Experimental: permit npc_create for registered classes not already in the map. Requires complete NPC assets; use only on test maps.");

static bool NPC_CanCreateFromConsole( const char *pClassName )
{
    if ( !pClassName || Q_strnicmp( pClassName, "npc_", 4 ) != 0 )
    {
        Msg("NPC classname must begin with npc_\\n");
        return false;
    }
    if ( !UTIL_GetCommandClient() )
    {
        Msg("npc_create requires an in-game player, not the dedicated server console.\\n");
        return false;
    }
    if ( !gEntList.FindEntityByClassname( NULL, pClassName ) &&
         !npc_allow_unprecached_create.GetBool() )
    {
        Msg("Can't create %s. Not precached; enable npc_allow_unprecached_create on a test map to try.\\n", pClassName);
        return false;
    }
    return true;
}
''')
    block = '''	// First make sure one of these types exists, becuase if it ain't
	// precached, we can't spawn one 
	CBaseEntity *pPrecached = gEntList.FindEntityByClassname(NULL, engine->Cmd_Argv(1));
	if (!pPrecached)
	{
		Msg("Can't create %s.  Not precached!\\n",engine->Cmd_Argv(1));
		return;
	}
'''
    assert s.count(block) == 2, s.count(block)
    s = s.replace(block, '''	if ( engine->Cmd_Argc() < 2 || !NPC_CanCreateFromConsole( engine->Cmd_Argv(1) ) )
		return;
''')
    # The entity dictionary accepts arbitrary classname, so never C-cast to
    # CAI_BaseNPC: a valid but non-NPC class would corrupt memory.
    typ = 'CAI_BaseNPC *baseNPC = (CAI_BaseNPC *)CreateEntityByName(engine->Cmd_Argv(1));'
    assert s.count(typ) == 2
    s = s.replace(typ, '''CBaseEntity *pCreated = CreateEntityByName( engine->Cmd_Argv(1) );
	CAI_BaseNPC *baseNPC = dynamic_cast<CAI_BaseNPC *>( pCreated );
	if ( pCreated && !baseNPC )
	{
		Msg("Can't create %s: this entity is not an NPC.\\n", engine->Cmd_Argv(1));
		UTIL_RemoveImmediate( pCreated );
		return;
	}
	if ( !pCreated )
	{
		Msg("Can't create %s: entity factory not registered.\\n", engine->Cmd_Argv(1));
		return;
	}''')
    # Don't spawn stranded NPCs when the placement trace misses the world.
    # The 'npc_create' version already spawns immediately (legacy behavior),
    # so handle only the previously leaky aimed case.
    s = s.replace('''				baseNPC->Relink();
			}
		}
	}
}
static ConCommand npc_create_aimed''', '''				baseNPC->Relink();
			}
		}
		else
		{
			UTIL_RemoveImmediate( baseNPC );
			Msg("Can't create %s: no placement surface found.\\n", engine->Cmd_Argv(1));
		}
	}
}
static ConCommand npc_create_aimed''')
    old = 'Note that this only works for npc classes that are already in the world.  You can not create an entity that doesn\'t have an instance in the level.'
    assert s.count(old) == 2
    s = s.replace(old, 'Normally requires an instance already in the map; npc_allow_unprecached_create enables experimental spawning for cut NPC testing.')
    p.write_bytes(s.replace('\n','\r\n').encode('latin-1'))
print('NPC console patch ready',p)

# Repair original NPC creation casts and the missing no-hit cleanup path.
s=p.read_bytes().decode('latin-1').replace('\r\n','\n')
if 'HL2MODERN_NPC_SPAWN_SAFETY' not in s:
    old='CAI_BaseNPC *baseNPC = dynamic_cast<CAI_BaseNPC *>( pCreated );'
    assert s.count(old)==2
    s=s.replace(old,'CAI_BaseNPC *baseNPC = pCreated ? pCreated->MyNPCPointer() : NULL;')
    target=("\t\t\t\tbaseNPC->Relink();\n\t\t\t}\n\t\t}\n\t}\n}\n"
            "static ConCommand npc_create(")
    assert s.count(target)==1
    replacement=("\t\t\t\tbaseNPC->Relink();\n\t\t\t}\n\t\t}\n"
                 "\t\telse\n\t\t{\n\t\t\t// HL2MODERN_NPC_SPAWN_SAFETY\n"
                 "\t\t\tUTIL_RemoveImmediate( baseNPC );\n"
                 "\t\t\tMsg(\"Can't create %s: no placement surface found.\\n\", engine->Cmd_Argv(1));\n"
                 "\t\t}\n\t}\n}\nstatic ConCommand npc_create(")
    s=s.replace(target,replacement)
    p.write_bytes(s.replace('\n','\r\n').encode('latin-1'))
