//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "BasePanel.h"
#include "OptionsDialog.h"
#include "EngineInterface.h"
#include "GameUI_Interface.h"
#include "cvarslider.h"
#include "tier1/convar.h"
#include "tier1/utlstring.h"
#include "tier1/utlvector.h"
#include "vgui_controls/ComboBox.h"
#include "vgui_controls/Slider.h"
#include "vgui_controls/TextEntry.h"
#include "vgui_controls/SectionedListPanel.h"

#include "vgui_controls/Button.h"
#include "vgui_controls/CheckButton.h"
#include "vgui_controls/PropertySheet.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/QueryBox.h"

#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"
#include "vgui/ISystem.h"
#include "vgui/IVGui.h"

#include "KeyValues.h"
#include "OptionsSubKeyboard.h"
#include "OptionsSubMouse.h"
#include "OptionsSubTouch.h"
#include "OptionsSubAudio.h"
#include "OptionsSubVideo.h"
#include "OptionsSubVoice.h"
#include "OptionsSubMultiplayer.h"
#include "OptionsSubDifficulty.h"
#include "OptionsSubPortal.h"
#include "OptionsSubHUD.h"
#include "OptionsSubAdvanced.h"
#ifdef WIN32
// NVNT haptic configuration dialog
#include "OptionsSubHaptics.h"
#endif
#include "ModInfo.h"

using namespace vgui;

static DHANDLE<COptionsDialog> s_GammaOptionsDialog;
extern Panel *GammaKeyboardAdvancedPanel();
extern Panel *GammaVideoAdvancedPanel();
extern Panel *GammaVideoGammaPanel();

static Panel *GammaNameOptionsPage( Panel *page, const char *name )
{
	page->SetName( name );
	return page;
}

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

//-----------------------------------------------------------------------------
// Purpose: Basic help dialog
//-----------------------------------------------------------------------------
COptionsDialog::COptionsDialog(vgui::Panel *parent) : PropertyDialog(parent, "OptionsDialog")
{
	s_GammaOptionsDialog = this;
	SetDeleteSelfOnClose(true);

	int w = 512;
	int h = 406;
	if (IsProportional())
	{
		w = scheme()->GetProportionalScaledValueEx(GetScheme(), w);
		h = scheme()->GetProportionalScaledValueEx(GetScheme(), h);
	}

	SetBounds(0, 0, w, h);

	SetSizeable( false );

	SetTitle("#GameUI_Options", true);
	// Set the minimum before tabs are constructed so all eight fit the 2003 frame.
	GetPropertySheet()->SetTabWidth( 0 );

	// debug timing code, this function takes too long
//	double s4 = system()->GetCurrentTime();

#if defined( WIN32 ) && !defined( _X360 )
	// NVNT START see if the user has a haptic device via convar. if so create haptics dialog.
	ConVarRef checkHap("hap_HasDevice");
	checkHap.Init("hap_HasDevice",true);
	if(checkHap.GetBool())
	{
		AddPage(new COptionsSubHaptics(this), "#GameUI_Haptics_TabTitle");
	}
	// NVNT END
#endif
	// GammaSource: the 2003 tab set and order (Keyboard, Mouse, Audio, Video, Voice, Advanced, HUD, Multiplayer);
	// there was no difficulty tab
	if (ModInfo().HasPortals())
	{
		AddPage(new COptionsSubPortal(this), "#GameUI_Portal");
	}

	AddPage(GammaNameOptionsPage(new COptionsSubKeyboard(this), "keyboard"), "#GameUI_Keyboard");
	AddPage(GammaNameOptionsPage(new COptionsSubMouse(this), "mouse"), "#GameUI_Mouse");

#ifdef ANDROID
	AddPage(new COptionsSubTouch(this), "Touch");
#endif

	m_pOptionsSubAudio = new COptionsSubAudio(this);
	m_pOptionsSubAudio->SetName( "audio" );
	AddPage(m_pOptionsSubAudio, "#GameUI_Audio");
	m_pOptionsSubVideo = new COptionsSubVideo(this);
	m_pOptionsSubVideo->SetName( "video" );
	AddPage(m_pOptionsSubVideo, "#GameUI_Video");

	AddPage(GammaNameOptionsPage(new COptionsSubVoice(this), "voice"), "#GameUI_Voice");
	AddPage(GammaNameOptionsPage(new COptionsSubAdvanced(this), "lock"), "Lock");

	// the HUD color convars come from the GammaSource client
	if ( ConVarRef( "hud_color_custom", true ).IsValid() )
	{
		AddPage(GammaNameOptionsPage(new COptionsSubHUD(this), "hud"), "HUD");
	}

	m_pOptionsSubMultiplayer = new COptionsSubMultiplayer(this);
	m_pOptionsSubMultiplayer->SetName( "multiplayer" );
	AddPage(m_pOptionsSubMultiplayer, "#GameUI_Multiplayer");

//	double s5 = system()->GetCurrentTime();
//	Msg("COptionsDialog::COptionsDialog(): %.3fms\n", (float)(s5 - s4) * 1000.0f);

	SetApplyButtonVisible(true);

}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
COptionsDialog::~COptionsDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: Brings the dialog to the fore
//-----------------------------------------------------------------------------
void COptionsDialog::Activate()
{
	BaseClass::Activate();
	EnableApplyButton(false);
}

void COptionsDialog::OnKeyCodePressed( KeyCode code )
{
	switch ( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_B:
		OnCommand( "Cancel" );
		return;
	}

	BaseClass::OnKeyCodePressed( code );
}

//-----------------------------------------------------------------------------
// Purpose: Opens the dialog
//-----------------------------------------------------------------------------
void COptionsDialog::Run()
{
	SetTitle("#GameUI_Options", true);
	Activate();
}

//-----------------------------------------------------------------------------
// Purpose: Called when the GameUI is hidden
//-----------------------------------------------------------------------------
void COptionsDialog::OnGameUIHidden()
{
	// tell our children about it
	for ( int i = 0 ; i < GetChildCount() ; i++ )
	{
		Panel *pChild = GetChild( i );
		if ( pChild )
		{
			PostMessage( pChild, new KeyValues( "GameUIHidden" ) );
		}
	}
}


// GammaSource diagnostic driver. These commands operate on VGUI controls and
// their normal Apply/Cancel handlers without synthesizing OS keyboard or mouse input.
static Panel *GammaUIFindTarget( const char *name )
{
	if ( !Q_stricmp( name, "keyboard_advanced" ) ) return GammaKeyboardAdvancedPanel();
	if ( !Q_stricmp( name, "video_advanced" ) ) return GammaVideoAdvancedPanel();
	if ( !Q_stricmp( name, "gamma" ) ) return GammaVideoGammaPanel();
	COptionsDialog *dialog = s_GammaOptionsDialog.Get();
	if ( !dialog ) return NULL;
	if ( !Q_stricmp( name, "options" ) ) return dialog;
	PropertySheet *sheet = dialog->GetPropertySheet();
	for ( int i = 0; i < sheet->GetNumPages(); ++i )
	{
		Panel *page = sheet->GetPage( i );
		if ( !Q_stricmp( page->GetName(), name ) ) return page;
	}
	return NULL;
}

static void GammaUIPostCommand( Panel *panel, const char *command )
{
	if ( panel ) ivgui()->PostMessage( panel->GetVPanel(), new KeyValues( "Command", "command", command ), 0 );
}

CON_COMMAND( gamma_ui_page, "Show a Gamma options page, keyboard_advanced, video_advanced, gamma, loading or menu" )
{
	if ( args.ArgC() < 2 ) return;
	const char *name = args[1];
	if ( !Q_stricmp( name, "loading" ) || !Q_stricmp( name, "menu" ) )
	{
		GammaUIPostCommand( GammaKeyboardAdvancedPanel(), "Close" );
		GammaUIPostCommand( GammaVideoAdvancedPanel(), "Close" );
		GammaUIPostCommand( GammaVideoGammaPanel(), "Close" );
		if ( s_GammaOptionsDialog.Get() ) s_GammaOptionsDialog->SetVisible( false );
		if ( !Q_stricmp( name, "loading" ) )
		{
			GameUI().OnLevelLoadingStarted( true );
			GameUI().SetShowProgressText( true );
			GameUI().UpdateProgressBar( 0.8f, "Retrieving game data..." );
		}
		else
		{
			GameUI().OnLevelLoadingFinished( false, "", "" );
		}
		engine->ClientCmd_Unrestricted( "gameui_activate\n" );
		Msg( "GAMMA_UI_PAGE %s\n", name );
		return;
	}
	if ( !Q_stricmp( name, "options" ) )
	{
		engine->ClientCmd_Unrestricted( "gamemenucommand OpenOptionsDialog\n" );
		return;
	}
	const bool keyboardAdvanced = !Q_stricmp( name, "keyboard_advanced" );
	const bool videoAdvanced = !Q_stricmp( name, "video_advanced" );
	const bool gamma = !Q_stricmp( name, "gamma" );
	Panel *page = GammaUIFindTarget( keyboardAdvanced ? "keyboard" : ( videoAdvanced || gamma ? "video" : name ) );
	if ( !page || !s_GammaOptionsDialog.Get() )
	{
		Warning( "GAMMA_UI: open options first with gamemenucommand OpenOptionsDialog\n" );
		return;
	}
	s_GammaOptionsDialog->GetPropertySheet()->SetActivePage( page );
	if ( keyboardAdvanced || videoAdvanced || gamma )
	{
		const char *button = keyboardAdvanced ? "KeyAdvancedButton" : ( videoAdvanced ? "AdvancedButton" : "GammaButton" );
		Panel *control = page->FindChildByName( button, true );
		if ( control ) ivgui()->PostMessage( control->GetVPanel(), new KeyValues( "PressButton" ), 0 );
	}
	Msg( "GAMMA_UI_PAGE %s\n", name );
}

static void GammaUIDumpControls( Panel *panel, const char *root, int depth )
{
	if ( !panel || depth > 5 ) return;
	int x, y, wide, tall; panel->GetBounds( x, y, wide, tall );
	if ( ComboBox *combo = dynamic_cast<ComboBox *>( panel ) )
	{
		char text[256]; combo->GetText( text, sizeof( text ) );
		Msg( "GAMMA_UI_COMBO root=%s name=%s items=%d active=%d enabled=%d visible=%d rect=%d,%d,%d,%d text=%s\n", root, panel->GetName(), combo->GetItemCount(), combo->GetActiveItem(), panel->IsEnabled(), panel->IsVisible(), x,y,wide,tall,text );
		for ( int row = 0; row < combo->GetItemCount(); ++row )
		{
			combo->GetItemText( combo->GetItemIDFromRow(row), text, sizeof(text) );
			Msg( "GAMMA_UI_ITEM root=%s name=%s row=%d text=%s\n", root,panel->GetName(),row,text );
		}
		return;
	}
	if ( Slider *slider = dynamic_cast<Slider *>( panel ) )
	{
		int low,high; slider->GetRange( low,high );
		CCvarSlider *cvarSlider = dynamic_cast<CCvarSlider *>( slider );
		Msg( "GAMMA_UI_SLIDER root=%s name=%s value=%.3f raw=%d range=%d,%d enabled=%d visible=%d rect=%d,%d,%d,%d\n", root,panel->GetName(),cvarSlider?cvarSlider->GetSliderValue():(float)slider->GetValue(),slider->GetValue(),low,high,panel->IsEnabled(),panel->IsVisible(),x,y,wide,tall );
	}
	else if ( CheckButton *check = dynamic_cast<CheckButton *>( panel ) )
		Msg( "GAMMA_UI_CHECK root=%s name=%s selected=%d enabled=%d visible=%d\n",root,panel->GetName(),check->IsSelected(),panel->IsEnabled(),panel->IsVisible() );
	else if ( SectionedListPanel *list = dynamic_cast<SectionedListPanel *>( panel ) )
		Msg( "GAMMA_UI_LIST root=%s name=%s items=%d\n",root,panel->GetName(),list->GetItemCount() );
	else if ( TextEntry *entry = dynamic_cast<TextEntry *>( panel ) )
	{
		char text[256]; entry->GetText( text,sizeof(text) );
		Msg( "GAMMA_UI_ENTRY root=%s name=%s text=%s enabled=%d visible=%d\n",root,panel->GetName(),text,panel->IsEnabled(),panel->IsVisible() );
	}
	for ( int i=0; i<panel->GetChildCount(); ++i ) GammaUIDumpControls( panel->GetChild(i),root,depth+1 );
}

CON_COMMAND( gamma_ui_dump, "Print actual option widget values, enabled state, combo rows, and underlying cvars" )
{
	if ( args.ArgC()>1 )
	{
		Panel *panel = GammaUIFindTarget( args[1] );
		if ( panel ) GammaUIDumpControls( panel,args[1],0 );
	}
	else if ( s_GammaOptionsDialog.Get() )
	{
		PropertySheet *sheet=s_GammaOptionsDialog->GetPropertySheet();
		Msg( "GAMMA_UI_OPTIONS pages=%d active=%d\n",sheet->GetNumPages(),sheet->GetActivePageNum() );
		for ( int i=0;i<sheet->GetNumPages();++i ) GammaUIDumpControls( sheet->GetPage(i),sheet->GetPage(i)->GetName(),0 );
		if ( GammaVideoAdvancedPanel() ) GammaUIDumpControls( GammaVideoAdvancedPanel(),"video_advanced",0 );
		if ( GammaKeyboardAdvancedPanel() ) GammaUIDumpControls( GammaKeyboardAdvancedPanel(),"keyboard_advanced",0 );
	}
	const char *vars[]={"sensitivity","m_customaccel","m_customaccel_exponent","volume","snd_musicvolume","snd_surround_speakers","dsp_slow_cpu","snd_pitchquality","closecaption","cc_subtitles","con_enable","hud_fastswitch","vid_borderless","mat_picmip","r_rootlod","mat_forceaniso","mat_vsync","mat_queue_mode","fov_desired","hud_color_custom","hud_color_r","hud_color_g","hud_color_b","name","cl_logofile","cl_downloadfilter"};
	for ( int i=0;i<ARRAYSIZE(vars);++i )
	{
		ConVarRef var(vars[i],true);
		if(var.IsValid()) Msg("GAMMA_UI_CVAR %s=%s\n",vars[i],var.GetString());
	}
}

CON_COMMAND( gamma_ui_set, "gamma_ui_set <target> <control> <pending value>; uses real widget APIs" )
{
	if(args.ArgC()<4)return;
	Panel *root=GammaUIFindTarget(args[1]);
	Panel *panel=root?root->FindChildByName(args[2],true):NULL;
	if(!panel){Warning("GAMMA_UI: missing target/control %s/%s\n",args[1],args[2]);return;}
	if(!panel->IsEnabled()){Warning("GAMMA_UI: disabled control %s/%s\n",args[1],args[2]);return;}
	if(ComboBox *combo=dynamic_cast<ComboBox*>(panel))
	{
		int row=atoi(args[3]);if(row<0||row>=combo->GetItemCount())return;
		combo->ActivateItemByRow(row);
	}
	else if(CCvarSlider *slider=dynamic_cast<CCvarSlider*>(panel)) slider->SetSliderValue((float)atof(args[3]));
	else if(Slider *slider=dynamic_cast<Slider*>(panel)) slider->SetValue(atoi(args[3]));
	else if(CheckButton *check=dynamic_cast<CheckButton*>(panel)) check->SetSelected(atoi(args[3])!=0);
	else if(TextEntry *entry=dynamic_cast<TextEntry*>(panel)) entry->SetText(args[3]);
	KeyValues *modified=new KeyValues("ControlModified");modified->SetPtr("panel",panel);
	ivgui()->PostMessage(root->GetVPanel(),modified,0);
	if(s_GammaOptionsDialog.Get()) ivgui()->PostMessage(s_GammaOptionsDialog->GetVPanel(),new KeyValues("ApplyButtonEnable"),0);
	Msg("GAMMA_UI_PENDING target=%s control=%s value=%s\n",args[1],args[2],args[3]);
}

CON_COMMAND( gamma_ui_command, "gamma_ui_command <target> <Apply|OK|Cancel|Close> via the normal VGUI handler" )
{
	if(args.ArgC()<3)return;
	Panel *panel=GammaUIFindTarget(args[1]);
	if(panel)GammaUIPostCommand(panel,args[2]);
}

CON_COMMAND( gamma_ui_dropdown, "gamma_ui_dropdown <target> <control>; open the actual populated dropdown" )
{
	if(args.ArgC()<3)return;
	Panel *root=GammaUIFindTarget(args[1]);
	ComboBox *combo=root?dynamic_cast<ComboBox*>(root->FindChildByName(args[2],true)):NULL;
	if(combo&&combo->IsEnabled())combo->DoClick();
}

struct GammaUITask_t { float when; CUtlString command; };
static CUtlVector<GammaUITask_t> s_GammaUITasks;
class CGammaUITestRunner : public Panel
{
	DECLARE_CLASS_SIMPLE(CGammaUITestRunner,Panel);
public:
	CGammaUITestRunner():BaseClass(BasePanel(),"GammaUITestRunner")
	{
		SetVisible(false);SetPaintEnabled(false);SetMouseInputEnabled(false);SetKeyBoardInputEnabled(false);
		ivgui()->AddTickSignal(GetVPanel(),25);
	}
	virtual void OnTick()
	{
		for(int i=0;i<s_GammaUITasks.Count();)
		{
			if(engine->Time()<s_GammaUITasks[i].when){++i;continue;}
			CUtlString command=s_GammaUITasks[i].command;s_GammaUITasks.Remove(i);
			engine->ClientCmd_Unrestricted(command.Get());
		}
	}
};
static DHANDLE<CGammaUITestRunner> s_GammaUITestRunner;
CON_COMMAND( gamma_ui_delay, "gamma_ui_delay <seconds> <command...>; wall-clock test timer that also runs in paused menus" )
{
	if(args.ArgC()<3)return;
	if(!s_GammaUITestRunner.Get())s_GammaUITestRunner=new CGammaUITestRunner;
	int i=s_GammaUITasks.AddToTail();
	s_GammaUITasks[i].when=engine->Time()+MAX(0.0f,(float)atof(args[1]));
	s_GammaUITasks[i].command=args.ArgS()+Q_strlen(args[1])+1;
}
CON_COMMAND( gamma_ui_clear, "Cancel pending Gamma UI test timer commands" ) { s_GammaUITasks.Purge(); }
