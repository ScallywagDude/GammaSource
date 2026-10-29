#include "cbase.h"
#include "gamma_player_characters.h"
#include "filesystem.h"
#include "bone_setup.h"
#include "animation.h"
#include "matsys_controls/mdlpanel.h"
#include "matsys_controls/matsyscontrols.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "vgui/IPanel.h"
#include "vgui_controls/Frame.h"
#include "vgui_controls/ComboBox.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/CheckButton.h"
#include "vgui_controls/TextEntry.h"
#include "ienginevgui.h"
#include "tier0/memdbgon.h"

ConVar gamma_player_model( "gamma_player_model", "gordon", FCVAR_USERINFO | FCVAR_ARCHIVE, "Playable GammaSource character." );

static bool GammaHasPlayableLevel()
{
	return engine->IsInGame() && !engine->IsLevelMainMenuBackground();
}

// This uses the game's studio renderer, skinning, materials and animations.
// CMDLPanel's clock also runs before a server or map has been loaded.
class CGammaPlayerPreview : public CMDLPanel
{
	DECLARE_CLASS_SIMPLE( CGammaPlayerPreview, CMDLPanel );
public:
	CGammaPlayerPreview( vgui::Panel *parent ) : BaseClass( parent, "PlayerPreview" )
	{
		SetGroundGrid( false );
		SetCollsionModel( false );
		SetLookAtCamera( true );
		SetIgnoreDoubleClick( true );
		SetCameraFOV( 30.0f );
		SetModelAnglesAndPosition( QAngle( 0, 180, 0 ), vec3_origin );
	}
	bool ShowCharacter( const char *model )
	{
		MDLCACHE_CRITICAL_SECTION();
		if ( !filesystem->FileExists( model, "GAME" ) )
		{
			SetMDL( MDLHANDLE_INVALID );
			return false;
		}
		SetMDL( model );
		studiohdr_t *raw = m_RootMDL.m_MDL.GetStudioHdr();
		if ( !raw ) return false;
		CStudioHdr header( raw, vgui::MDLCache() );
		int sequence = LookupSequence( &header, "gamma_neutral_idle_subtle" );
		if ( sequence < 0 ) sequence = LookupSequence( &header, "idle_pistol" );
		if ( sequence < 0 ) sequence = 0;
		SetSequence( sequence, true );
		SetSkin( 0 );
		SetPoseParameterByName( "body_yaw", 0 );
		SetPoseParameterByName( "body_pitch", 0 );
		SetPoseParameterByName( "head_yaw", 0 );
		SetPoseParameterByName( "head_pitch", 0 );
		ResetView();
		return true;
	}
	void ResetView()
	{
		SetCameraPositionAndAngles( vec3_origin, vec3_angle );
		LookAtMDL();
	}
	void Report()
	{
		studiohdr_t *raw = m_RootMDL.m_MDL.GetStudioHdr();
		if ( !raw ) { Msg( "GAMMA_PREVIEW no_model\n" ); return; }
		CStudioHdr header( raw, vgui::MDLCache() );
		const int sequence = m_RootMDL.m_MDL.m_nSequence;
		Msg( "GAMMA_PREVIEW model=%s sequence=%s time=%.3f bones=%d\n", raw->pszName(),
			sequence >= 0 && sequence < header.GetNumSeq() ? header.pSeqdesc( sequence ).pszLabel() : "invalid",
			m_RootMDL.m_MDL.m_flTime, header.numbones() );
	}
};

class CGammaCharacterMenu : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CGammaCharacterMenu, vgui::Frame );
public:
	CGammaCharacterMenu() : BaseClass( NULL, "GammaCharacters" ),
		m_PreviewAvailable( false ), m_LastParentWide( 0 ), m_LastParentTall( 0 ), m_Preview( NULL )
	{
		SetParent( enginevgui->GetPanel( PANEL_GAMEUIDLL ) );
		// The actual menu scheme supplies the same Frame borders, title bar,
		// fonts and controls used by Options and New Game in this build.
		SetScheme( vgui::scheme()->LoadSchemeFromFile( "resource/SourceScheme.res", "SourceScheme" ) );
		SetTitle( "Player character", true );
		SetProportional( false );
		SetSizeable( false );
		SetMinimizeButtonVisible( false );
		SetMaximizeButtonVisible( false );
		SetDeleteSelfOnClose( true );
		SetKeyBoardInputEnabled( true );
		SetMouseInputEnabled( true );
		m_NameLabel = new vgui::Label( this, "PlayerNameLabel", "Player name" );
		m_Name = new vgui::TextEntry( this, "PlayerName" );
		m_Name->SetMaximumCharCount( MAX_PLAYER_NAME_LENGTH - 1 );
		m_Name->SetAllowNonAsciiCharacters( true );
		m_Name->SetTabPosition( 1 );
		ConVarRef name( "name" );
		m_Name->SetText( name.IsValid() ? name.GetString() : "Player" );
		m_CharacterLabel = new vgui::Label( this, "CharacterLabel", "Player model" );
		m_Characters = new vgui::ComboBox( this, "Characters", 10, false );
		m_Characters->SetTabPosition( 2 );
		m_Characters->AddActionSignalTarget( this );
		int selected = 0;
		for ( int n = 0; n < ARRAYSIZE( g_GammaPlayerCharacters ); ++n )
		{
			m_ItemIDs[n] = m_Characters->AddItem( g_GammaPlayerCharacters[n].name, NULL );
			if ( !Q_stricmp( gamma_player_model.GetString(), g_GammaPlayerCharacters[n].alias ) ||
				 !Q_stricmp( gamma_player_model.GetString(), g_GammaPlayerCharacters[n].model ) ) selected = n;
		}
		m_ThirdPerson = new vgui::CheckButton( this, "ThirdPerson", "Third-person view" );
		m_ThirdPerson->SetTabPosition( 3 );
		ConVarRef thirdPerson( "cl_thirdperson" );
		m_ThirdPerson->SetSelected( thirdPerson.IsValid() && thirdPerson.GetBool() );
		m_Status = new vgui::Label( this, "ProfileStatus", "" );
		m_Status->SetWrap( true );
		m_Status->SetContentAlignment( vgui::Label::a_northwest );
		m_PreviewLabel = new vgui::Label( this, "PreviewLabel", "Character preview" );
		m_Preview = new CGammaPlayerPreview( this );
		m_Reset = new vgui::Button( this, "ResetView", "Reset view", this, "resetview" );
		m_Reset->SetTabPosition( 4 );
		m_Help = new vgui::Label( this, "PreviewHelp", "Drag to rotate. Scroll to zoom." );
		m_Play = new vgui::Button( this, "Play", "Apply", this, "apply" );
		m_Play->SetTabPosition( 5 );
		m_Exit = new vgui::Button( this, "Cancel", "Cancel", this, "cancel" );
		m_Exit->SetTabPosition( 6 );
		m_Play->SetAsDefaultButton( 1 );
		SetSize( 800, 580 );
		SetPos( 0, 0 );
		InvalidateLayout( true );
		m_Characters->ActivateItemByRow( selected );
		UpdatePreview();
		UpdateWindowBounds();
	}
	void PerformLayout() OVERRIDE
	{
		BaseClass::PerformLayout();
		if ( !m_Preview ) return;
		const int wide = GetWide(), tall = GetTall();
		const int leftWide = clamp( (int)( wide * 0.38f ), 220, 290 );
		const int previewX = leftWide + 48;
		const int previewWide = wide - previewX - 24;
		m_NameLabel->SetBounds( 24, 56, leftWide, 20 );
		m_Name->SetBounds( 24, 80, leftWide, 26 );
		m_CharacterLabel->SetBounds( 24, 130, leftWide, 20 );
		m_Characters->SetBounds( 24, 154, leftWide, 26 );
		m_ThirdPerson->SetBounds( 24, 206, leftWide, 26 );
		m_Status->SetBounds( 24, 250, leftWide, 100 );
		m_PreviewLabel->SetBounds( previewX, 44, previewWide, 20 );
		m_Preview->SetBounds( previewX, 70, previewWide, tall - 150 );
		m_Reset->SetBounds( previewX, tall - 70, 90, 24 );
		m_Help->SetBounds( previewX + 100, tall - 68, previewWide - 100, 20 );
		m_Play->SetBounds( wide - 240, tall - 40, 112, 28 );
		m_Exit->SetBounds( wide - 120, tall - 40, 96, 28 );
		m_Preview->LookAtMDL();
	}
	void OnThink() OVERRIDE
	{
		BaseClass::OnThink();
		UpdateWindowBounds();
	}
	int SelectedCharacter() const
	{
		const int item = m_Characters->GetActiveItem();
		for ( int n = 0; n < ARRAYSIZE( m_ItemIDs ); ++n )
			if ( m_ItemIDs[n] == item ) return n;
		return -1;
	}
	bool SelectCharacter( const char *alias )
	{
		for ( int n = 0; n < ARRAYSIZE( g_GammaPlayerCharacters ); ++n )
			if ( !Q_stricmp( alias, g_GammaPlayerCharacters[n].alias ) )
			{
				m_Characters->ActivateItemByRow( n );
				UpdatePreview();
				return true;
			}
		return false;
	}
	void Report()
	{
		const int selected = SelectedCharacter();
		Msg( "GAMMA_CHARACTER_MENU visible=%d selected=%s available=%d\n", IsVisible(),
			selected >= 0 ? g_GammaPlayerCharacters[selected].alias : "none", m_PreviewAvailable );
		m_Preview->Report();
	}
	void OnCommand( const char *command ) OVERRIDE
	{
		if ( !Q_stricmp( command, "resetview" ) ) { m_Preview->ResetView(); return; }
		if ( !Q_stricmp( command, "cancel" ) ) { Close(); return; }
		if ( !Q_stricmp( command, "apply" ) )
		{
			char playerName[128];
			m_Name->GetText( playerName, sizeof( playerName ) );
			Q_StripPrecedingAndTrailingWhitespace( playerName );
			if ( !playerName[0] )
			{
				m_Status->SetText( "Enter a player name." );
				m_Name->RequestFocus();
				return;
			}
			const int selected = SelectedCharacter();
			if ( selected < 0 || !m_PreviewAvailable ) return;
			// Values go directly to convars, never into a command string.
			ConVarRef name( "name" );
			if ( name.IsValid() ) name.SetValue( playerName );
			gamma_player_model.SetValue( g_GammaPlayerCharacters[selected].alias );
			ConVarRef thirdPerson( "cl_thirdperson" );
			if ( thirdPerson.IsValid() ) thirdPerson.SetValue( m_ThirdPerson->IsSelected() ? 1 : 0 );
			engine->ClientCmd_Unrestricted( "host_writeconfig\n" );
			Close();
			if ( GammaHasPlayableLevel() ) engine->ClientCmd_Unrestricted( "gameui_hide\n" );
			return;
		}
		BaseClass::OnCommand( command );
	}
	void OnClose() OVERRIDE
	{
		BaseClass::OnClose();
	}
private:
	void UpdateWindowBounds()
	{
		int wide, tall;
		vgui::ipanel()->GetSize( vgui::ipanel()->GetParent( GetVPanel() ), wide, tall );
		// The engine's root initially reports 64x64. Wait for the real viewport
		// before centering, and update again if the video mode changes.
		if ( wide < 480 || tall < 360 || ( wide == m_LastParentWide && tall == m_LastParentTall ) ) return;
		m_LastParentWide = wide;
		m_LastParentTall = tall;
		SetSize( MIN( 800, wide - 32 ), MIN( 580, tall - 32 ) );
		SetPos( ( wide - GetWide() ) / 2, ( tall - GetTall() ) / 2 );
		InvalidateLayout( true );
	}
	MESSAGE_FUNC_PARAMS( OnControlChanged, "TextChanged", data )
	{
		if ( data->GetPtr( "panel" ) == m_Characters && m_Preview ) UpdatePreview();
	}
	void UpdatePreview()
	{
		const int selected = SelectedCharacter();
		m_PreviewAvailable = selected >= 0 && m_Preview->ShowCharacter( g_GammaPlayerCharacters[selected].model );
		m_Play->SetEnabled( m_PreviewAvailable );
		m_Status->SetText( m_PreviewAvailable ?
			"Your selection is saved when you apply it." :
			"This character's files are not installed." );
	}
	bool m_PreviewAvailable;
	int m_LastParentWide, m_LastParentTall;
	int m_ItemIDs[ARRAYSIZE( g_GammaPlayerCharacters )];
	vgui::TextEntry *m_Name;
	vgui::ComboBox *m_Characters;
	vgui::CheckButton *m_ThirdPerson;
	vgui::Label *m_NameLabel, *m_CharacterLabel, *m_Status, *m_PreviewLabel, *m_Help;
	vgui::Button *m_Play, *m_Exit, *m_Reset;
	CGammaPlayerPreview *m_Preview;
};

static vgui::DHANDLE<CGammaCharacterMenu> g_GammaCharacterMenu;

static void GammaShowCharacterMenu()
{
	engine->ClientCmd_Unrestricted( "gameui_activate\nhideconsole\n" );
	if ( !g_GammaCharacterMenu.Get() || !g_GammaCharacterMenu->IsVisible() )
		g_GammaCharacterMenu = new CGammaCharacterMenu();
	g_GammaCharacterMenu->Activate();
}

CON_COMMAND( gamma_characters, "Choose a playable character with a live preview." )
{
	GammaShowCharacterMenu();
}

CON_COMMAND( gamma_character_menu_status, "Report the live character preview model and animation clock." )
{
	if ( g_GammaCharacterMenu.Get() ) g_GammaCharacterMenu->Report();
	else Msg( "GAMMA_CHARACTER_MENU closed\n" );
}

CON_COMMAND_F( gamma_character_menu_select, "Select a roster alias in the character menu for preview verification.", FCVAR_DEVELOPMENTONLY )
{
	if ( g_GammaCharacterMenu.Get() && args.ArgC() == 2 ) g_GammaCharacterMenu->SelectCharacter( args[1] );
}
