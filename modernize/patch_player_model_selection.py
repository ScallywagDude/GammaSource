#!/usr/bin/env python3
"""Branch-safe Multiplayer tab player-model selection for the HL2 client/server.

The legacy GameUI has a static bitmap, not an engine 3D preview. This patch
preserves the bitmap while adding a real installed-model picker and server-side
allowlist validation. A live 3D renderer is a separate integration task.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
def patch(rel, fn):
    path=root/rel
    raw=path.read_bytes(); crlf=b'\r\n' in raw
    text=raw.decode('latin-1').replace('\r\n','\n')
    modified=fn(text)
    if modified!=text:
        path.write_bytes((modified.replace('\n','\r\n') if crlf else modified).encode('latin-1'))

def dialog(s):
    marker='HL2MODERN_MULTIPLAYER_TAB'
    if marker in s:return s
    old='''\tif ( ModInfo().IsSinglePlayerOnly() ) 
\t{
\t\tAddPage(new CMultiplayerAdvancedPage(this), "#GameUI_AdvancedNoEllipsis");
\t}'''
    assert s.count(old)==1
    new='''\t// HL2MODERN_MULTIPLAYER_TAB: show the multiplayer model selector in
\t// HL2 even when the original liblist metadata says single-player only.
\tconst bool modernHL2 = HL2ModernPlayerModels::IsHL2GameDir(engine->GetGameDirectory());
\tif ( ModInfo().IsSinglePlayerOnly() )
\t{
\t\tif (modernHL2)
\t\t\tAddPage(new COptionsSubMultiplayer(this), "#GameUI_Multiplayer");
\t\telse
\t\t\tAddPage(new CMultiplayerAdvancedPage(this), "#GameUI_AdvancedNoEllipsis");
\t}'''
    s=s.replace(old,new)
    assert '#include "OptionsSubMultiplayer.h"' in s
    s=s.replace('#include "OptionsSubMultiplayer.h"','#include "OptionsSubMultiplayer.h"\n#include "../modernize/hl2_playermodel_policy.h"',1)
    return s

def menu(s):
    marker='HL2MODERN_MULTIPLAYER_MODELS'
    if marker in s:return s
    s=s.replace('#include "ModInfo.h"','#include "ModInfo.h"\n#include "../modernize/hl2_playermodel_policy.h"',1)
    # Don't hide the selector for HL2's historical nomodels flag.
    old='if ( ModInfo().NoModels() )'
    assert s.count(old)==1
    s=s.replace(old,'if ( ModInfo().NoModels() && !HL2ModernPlayerModels::IsHL2GameDir(engine->GetGameDirectory()) )',1)
    start=s.index('void COptionsSubMultiplayer::InitModelList( CLabeledCommandComboBox *cb )')
    openbrace=s.index('{',start)
    s=s[:openbrace+1]+'''
\t// HL2MODERN_MULTIPLAYER_MODELS: only enumerate this branch's vetted,
\t// installed model paths. Other branches keep their 2003 behavior.
\tif (HL2ModernPlayerModels::IsHL2GameDir(engine->GetGameDirectory()))
\t{
\t\tConVar *cv=(ConVar *)cvar->FindVar("model");
\t\tconst char *current=cv ? cv->GetString() : "";
\t\tint index=0, selected=0;
\t\tfor (unsigned int i=0; i<HL2ModernPlayerModels::Count(); ++i)
\t\t{
\t\t\tconst HL2ModernPlayerModels::Entry &entry=HL2ModernPlayerModels::kModels[i];
\t\t\tif (!filesystem()->FileExists(entry.path)) continue;
\t\t\tchar command[160];
\t\t\t_snprintf(command, sizeof(command)-1, "model \\"%s\\"\\n", entry.path);
\t\t\tcommand[sizeof(command)-1]=0;
\t\t\tcb->AddItem(entry.label,command);
\t\t\tif (!stricmp(current,entry.path)) selected=index;
\t\t\t++index;
\t\t}
\t\tif(index>0) cb->SetInitialItem(selected);
\t\treturn;
\t}
''' +s[openbrace+1:]
    # Preview: do not pass non-BMP paths to legacy remapping code.
    old='''\tif( !modelname[ 0 ] )
\t\treturn;

\tchar texture[ 256 ];'''
    assert s.count(old)==1
    s=s.replace(old,'''	if( !modelname[ 0 ] )
		return;

	// The legacy RemapPalette() only understands models/player/*.bmp.
	// HL2 MDL assets need an engine-backed 3D viewport; don't try to
	// reinterpret MDL data as a BMP or generate corrupt remapped textures.
	if (HL2ModernPlayerModels::IsHL2GameDir(engine->GetGameDirectory()))
	{
		// Live selection preview: prefer installed VGUI portrait materials,
		// with a clear status label instead of rendering a missing texture.
		const char *command=m_pModelList->GetActiveItemCommand();
		const char *path=command && !strncmp(command, "model \\"", 7) ? command+7 : NULL;
		const HL2ModernPlayerModels::Entry *entry=NULL;
		if(path)
		{
			char safePath[100];
			const char *end=strchr(path,'\"');
			if(end && end-path>0 && end-path<sizeof(safePath))
			{
				memcpy(safePath,path,end-path);
				safePath[end-path]=0;
				entry=HL2ModernPlayerModels::Find(safePath);
			}
		}
		if(entry && m_pModelImage)
		{
			char material[160], asset[190];
			_snprintf(material,sizeof(material)-1,"vgui/playermodels/%s",entry->portrait);
			material[sizeof(material)-1]=0;
			_snprintf(asset,sizeof(asset)-1,"materials/%s.vmt",material);
			asset[sizeof(asset)-1]=0;
			if(filesystem()->FileExists(asset))
			{
				m_pModelImage->SetVisible(true);
				m_pModelImage->setTexture(material);
			}
			else m_pModelImage->SetVisible(false);
		}
		if (m_pPreviewLabel)
		{
			m_pPreviewLabel->SetText(entry ? entry->label : "No compatible player model");
		}
		return;
	}

	char texture[ 256 ];''',1)
    s=s.replace('\tm_pModelImage = new CBitmapImagePanel', '\tm_pPreviewLabel = NULL;\n\tm_pModelImage = new CBitmapImagePanel',1)
    s=s.replace('\tLoadControlSettings("Resource/OptionsSubMultiplayer.res");',
        '\tLoadControlSettings("Resource/OptionsSubMultiplayer.res");\n'
        '\t// HL2MODERN_LIVE_PORTRAIT: installed VGUI portrait updates on selection.\n'
        '\tm_pPreviewLabel = new Label(this, "ModernModelPreviewStatus", "");\n'
        '\tint previewX, previewY, previewW, previewH;\n'
        '\tm_pModelImage->GetBounds(previewX, previewY, previewW, previewH);\n'
        '\tm_pPreviewLabel->SetBounds(previewX, previewY+previewH+4, previewW>0?previewW:160, 22);\n'
        '\tRemapModel();',1)
    # prevent hidden 2003 picker from being reset into 2D legacy previews.
    old='''\tm_pModelList->ApplyChanges();'''
    assert s.count(old)==1
    s=s.replace(old,'''	if (m_pModelList) m_pModelList->ApplyChanges();''',1)
    return s

def server(s):
    marker='HL2MODERN_HL2_PLAYERMODEL'
    if marker in s:return s
    s=s.replace('#include "globals.h"','#include "globals.h"\n#include "../../modernize/hl2_playermodel_policy.h"',1)
    old='''void CHL2_Player::Precache( void )
{
\tBaseClass::Precache();
}'''
    assert s.count(old)==1
    s=s.replace(old,'''void CHL2_Player::Precache( void )
{
	BaseClass::Precache();
	// HL2MODERN_HL2_PLAYERMODEL: precache only installed, allowlisted models.
	for (unsigned int i=0; i<HL2ModernPlayerModels::Count(); ++i)
	{
		const char *path=HL2ModernPlayerModels::kModels[i].path;
		if (filesystem->FileExists(path)) engine->PrecacheModel(path);
	}
}''',1)
    old='''void CHL2_Player::Spawn(void)
{
\tSetModel( "models/player.mdl" );'''
    assert s.count(old)==1
    s=s.replace(old,'''void CHL2_Player::Spawn(void)
{
	const char *path="models/player.mdl";
	if (edict())
	{
		char *info=engine->GetInfoKeyBuffer(edict());
		const char *requested=info ? engine->InfoKeyValue(info,"model") : NULL;
		const HL2ModernPlayerModels::Entry *entry=HL2ModernPlayerModels::Find(requested);
		if (entry && filesystem->FileExists(entry->path)) path=entry->path;
	}
	SetModel(path);''',1)
    return s
def menu_header(s):
    if 'm_pPreviewLabel;' in s:return s
    target='class CBitmapImagePanel;'
    assert target in s
    s=s.replace(target,target+'\nnamespace vgui { class Label; }',1)
    target='\tCBitmapImagePanel *m_pModelImage;'
    assert target in s
    return s.replace(target,target+'\n\tvgui::Label *m_pPreviewLabel;',1)
def combo(s):
    if 'HL2MODERN_COMBO_LIVE_UPDATE' in s:return s
    marker='\tif (HasBeenModified())\n\t{'
    assert marker in s
    insert='''\t// HL2MODERN_COMBO_LIVE_UPDATE: notify the owner with the originating panel.
\tKeyValues *changed = new KeyValues("TextChanged");
\tchanged->SetPtr("panel", this);
\tPostMessage(GetParent(), changed);
'''
    return s.replace(marker, insert+marker, 1)
patch('gameui/LabeledCommandComboBox.cpp',combo)
patch('gameui/OptionsSubMultiplayer.h',menu_header)
patch('gameui/OptionsDialog.cpp',dialog)
patch('gameui/OptionsSubMultiplayer.cpp',menu)
patch('dlls/hl2_dll/hl2_player.cpp',server)
print('HL2 Multiplayer tab / branch-safe player model choices / server allowlist patched.')
