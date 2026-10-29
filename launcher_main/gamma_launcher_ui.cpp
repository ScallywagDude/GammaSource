#include "gamma_launcher_ui.h"
#include "../game/shared/gamma/gamma_player_characters.h"
#include "tier0/platform.h"
#include "tier0/dbg.h"
#include "tier1/strtools.h"
#include "tier1/KeyValues.h"
#include "tier2/tier2.h"
#include "tier3/tier3.h"
#include "filesystem.h"
#include "const.h"
#include "studio.h"
#include "matsys_controls/mdlpanel.h"
#include "matsys_controls/matsyscontrols.h"
#include "vgui/IScheme.h"
#include "vgui/IVGui.h"
#include "vgui/IPanel.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Frame.h"
#include "vgui_controls/ComboBox.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/CheckButton.h"
#include "vgui_controls/TextEntry.h"
#include "tier0/memdbgon.h"

// All preview rendering is provided by the standalone tools' studio renderer.
// Its animation clock runs without engine.dll, a map or a game client.
class CGammaLauncherPreview : public CMDLPanel
{
	DECLARE_CLASS_SIMPLE( CGammaLauncherPreview, CMDLPanel );
public:
	CGammaLauncherPreview( vgui::Panel *parent ) : BaseClass( parent, "PlayerPreview" ), m_PaintCount( 0 )
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
		if ( !g_pFullFileSystem->FileExists( model, "GAME" ) )
		{
			SetMDL( MDLHANDLE_INVALID );
			return false;
		}
		SetMDL( model );
		studiohdr_t *raw = m_RootMDL.m_MDL.GetStudioHdr();
		if ( !raw ) return false;
		CStudioHdr header( raw, vgui::MDLCache() );
		int sequence = -1;
		// Includes are virtual sequences; reading only the body's local table
		// would miss its actual neutral animation.
		for ( int n = 0; n < header.GetNumSeq(); ++n )
			if ( !Q_stricmp( header.pSeqdesc( n ).pszLabel(), "gamma_neutral_idle_subtle" ) )
			{
				sequence = n;
				break;
			}
		SetSequence( sequence >= 0 ? sequence : 0, true );
		SetSkin( 0 );
		SetPoseParameterByName( "body_yaw", 0 );
		SetPoseParameterByName( "body_pitch", 0 );
		SetPoseParameterByName( "head_yaw", 0 );
		SetPoseParameterByName( "head_pitch", 0 );
		ResetView();
		return sequence >= 0;
	}

	void ResetView()
	{
		SetCameraPositionAndAngles( vec3_origin, vec3_angle );
		LookAtMDL();
	}

	void OnMouseWheeled( int delta ) OVERRIDE
	{
		// The tools panel's default wheel handler only forwards to a current
		// drag manipulator, whose zoom implementation has no wheel handler.
		Vector offset;
		GetCameraOffset( offset );
		offset.x = clamp( offset.x * powf( 1.1f, -delta ), 16.0f, 4096.0f );
		SetCameraOffset( offset );
		UpdateCameraTransform();
	}

	bool VerificationState( GammaLauncherVerificationState &state )
	{
		state.previewPaintCount = m_PaintCount;
		state.previewVisible = IsVisible();
		state.previewFullyVisible = vgui::ipanel()->IsFullyVisible( GetVPanel() );
		state.previewAlpha = GetAlpha();
		GetClipRect( state.previewClipLeft, state.previewClipTop, state.previewClipRight, state.previewClipBottom );
		MDLCACHE_CRITICAL_SECTION();
		if ( m_RootMDL.m_MDL.GetMDL() == MDLHANDLE_INVALID ) return false;
		studiohdr_t *raw = m_RootMDL.m_MDL.GetStudioHdr();
		if ( !raw ) return false;
		CStudioHdr header( raw, vgui::MDLCache() );
		Q_strncpy( state.loadedModel, vgui::MDLCache()->GetModelName( m_RootMDL.m_MDL.GetMDL() ), sizeof( state.loadedModel ) );
		const int sequence = m_RootMDL.m_MDL.m_nSequence;
		Q_strncpy( state.sequence, sequence >= 0 && sequence < header.GetNumSeq() ?
			header.pSeqdesc( sequence ).pszLabel() : "invalid", sizeof( state.sequence ) );
		// This is CMDLPanel's actual render/animation clock, not a test timer.
		state.animationTime = m_RootMDL.m_MDL.m_flTime;
		state.boneCount = header.numbones();
		return true;
	}
	void Paint() OVERRIDE
	{
		++m_PaintCount;
		BaseClass::Paint();
	}
private:
	unsigned int m_PaintCount;
};

class CGammaStandaloneLauncher : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CGammaStandaloneLauncher, vgui::Frame );
public:
	CGammaStandaloneLauncher( vgui::VPANEL parent, const GammaLauncherProfile &profile,
		GammaLauncherPlayCallback play, GammaLauncherExitCallback exit, void *context ) :
		BaseClass( NULL, "GammaSourceLauncher" ), m_PlayCallback( play ), m_ExitCallback( exit ),
		m_Context( context ), m_PreviewAvailable( false ), m_Closing( false ), m_PaintCount( 0 ), m_Preview( NULL )
	{
		SetParent( parent );
		// This is the game's real menu scheme, including its Frame border and
		// title bar; no imitation platform controls are used.
		SetScheme( vgui::scheme()->LoadSchemeFromFile( "resource/SourceScheme.res", "SourceScheme" ) );
		SetTitle( "GammaSource Launcher", true );
		SetProportional( false );
		SetSizeable( false );
		SetMoveable( false );
		SetMinimizeButtonVisible( false );
		SetMaximizeButtonVisible( false );
		SetKeyBoardInputEnabled( true );
		SetMouseInputEnabled( true );
		SetDeleteSelfOnClose( false );

		m_NameLabel = new vgui::Label( this, "PlayerNameLabel", "Player name" );
		m_Name = new vgui::TextEntry( this, "PlayerName" );
		m_Name->SetMaximumCharCount( MAX_PLAYER_NAME_LENGTH - 1 );
		m_Name->SetAllowNonAsciiCharacters( true );
		m_Name->SetTabPosition( 1 );
		m_Name->SetText( profile.playerName[0] ? profile.playerName : "Player" );
		m_CharacterLabel = new vgui::Label( this, "CharacterLabel", "Player model" );
		m_Characters = new vgui::ComboBox( this, "Characters", 10, false );
		m_Characters->SetTabPosition( 2 );
		m_Characters->AddActionSignalTarget( this );
		int selected = 0;
		for ( int n = 0; n < ARRAYSIZE( g_GammaPlayerCharacters ); ++n )
		{
			m_ItemIDs[n] = m_Characters->AddItem( g_GammaPlayerCharacters[n].name, NULL );
			if ( !Q_stricmp( profile.playerModel, g_GammaPlayerCharacters[n].alias ) ||
				 !Q_stricmp( profile.playerModel, g_GammaPlayerCharacters[n].model ) ) selected = n;
		}
		m_ThirdPerson = new vgui::CheckButton( this, "ThirdPerson", "Third-person view" );
		m_ThirdPerson->SetTabPosition( 3 );
		m_ThirdPerson->SetSelected( profile.thirdPerson );
		m_Status = new vgui::Label( this, "ProfileStatus", "" );
		m_Status->SetWrap( true );
		m_Status->SetContentAlignment( vgui::Label::a_northwest );
		m_PreviewLabel = new vgui::Label( this, "PreviewLabel", "Character preview" );
		m_Preview = new CGammaLauncherPreview( this );
		m_Reset = new vgui::Button( this, "ResetView", "Reset view", this, "resetview" );
		m_Reset->SetTabPosition( 4 );
		m_Help = new vgui::Label( this, "PreviewHelp", "Drag to rotate. Scroll to zoom." );
		m_Play = new vgui::Button( this, "Play", "Play", this, "play" );
		m_Play->SetTabPosition( 5 );
		m_Play->SetAsDefaultButton( 1 );
		m_Exit = new vgui::Button( this, "Exit", "Exit", this, "exit" );
		m_Exit->SetTabPosition( 6 );
		SetBounds( 0, 0, 800, 580 );
		InvalidateLayout( true );
		m_Characters->ActivateItemByRow( selected );
		UpdatePreview();
		Activate();
		m_Name->RequestFocus();
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
		m_Status->SetBounds( 24, 250, leftWide, 110 );
		m_PreviewLabel->SetBounds( previewX, 44, previewWide, 20 );
		m_Preview->SetBounds( previewX, 70, previewWide, tall - 150 );
		m_Reset->SetBounds( previewX, tall - 70, 90, 24 );
		m_Help->SetBounds( previewX + 100, tall - 68, previewWide - 100, 20 );
		m_Play->SetBounds( wide - 240, tall - 40, 112, 28 );
		m_Exit->SetBounds( wide - 120, tall - 40, 96, 28 );
		m_Preview->LookAtMDL();
	}

	void OnCommand( const char *command ) OVERRIDE
	{
		if ( m_Closing ) return;
		if ( !Q_stricmp( command, "resetview" ) ) { m_Preview->ResetView(); return; }
		if ( !Q_stricmp( command, "exit" ) ) { ExitLauncher(); return; }
		if ( !Q_stricmp( command, "play" ) )
		{
			GammaLauncherProfile profile = {};
			m_Name->GetText( profile.playerName, sizeof( profile.playerName ) );
			Q_StripPrecedingAndTrailingWhitespace( profile.playerName );
			if ( !profile.playerName[0] )
			{
				m_Status->SetText( "Enter a player name." );
				m_Name->RequestFocus();
				return;
			}
			for ( const char *p = profile.playerName; *p; ++p )
				if ( (unsigned char)*p < 32 || (unsigned char)*p == 127 )
				{
					m_Status->SetText( "Enter a name without control characters." );
					return;
				}
			const int selected = SelectedCharacter();
			if ( selected < 0 || !m_PreviewAvailable ) return;
			Q_strncpy( profile.playerModel, g_GammaPlayerCharacters[selected].alias, sizeof( profile.playerModel ) );
			profile.thirdPerson = m_ThirdPerson->IsSelected();
			char error[512] = "Could not start GammaSource.";
			m_Play->SetEnabled( false );
			if ( m_PlayCallback && m_PlayCallback( profile, error, sizeof( error ), m_Context ) )
			{
				ExitLauncher();
				return;
			}
			m_Play->SetEnabled( true );
			m_Status->SetText( error );
			return;
		}
		BaseClass::OnCommand( command );
	}

	void OnClose() OVERRIDE { ExitLauncher(); }
	void Paint() OVERRIDE
	{
		++m_PaintCount;
		BaseClass::Paint();
	}

	bool VerifySelect( const char *alias )
	{
		if ( !alias ) return false;
		for ( int n = 0; n < ARRAYSIZE( g_GammaPlayerCharacters ); ++n )
			if ( !Q_stricmp( alias, g_GammaPlayerCharacters[n].alias ) )
			{
				// The real combo posts TextChanged through VGUI's message queue.
				// Let that same path update the preview during the next frame.
				m_Characters->ActivateItemByRow( n );
				return true;
			}
		return false;
	}
	bool VerifyProfileSet( const GammaLauncherProfile &profile )
	{
		if ( !VerifySelect( profile.playerModel ) ) return false;
		m_Name->SetText( profile.playerName );
		m_ThirdPerson->SetSelected( profile.thirdPerson );
		return true;
	}
	bool VerifyReport( GammaLauncherVerificationState &state )
	{
		memset( &state, 0, sizeof( state ) );
		state.dropdownIndex = SelectedCharacter();
		m_Name->GetText( state.profile.playerName, sizeof( state.profile.playerName ) );
		if ( state.dropdownIndex >= 0 )
			Q_strncpy( state.profile.playerModel, g_GammaPlayerCharacters[state.dropdownIndex].alias, sizeof( state.profile.playerModel ) );
		state.profile.thirdPerson = m_ThirdPerson->IsSelected();
		m_Preview->GetBounds( state.previewX, state.previewY, state.previewWide, state.previewTall );
		state.framePaintCount = m_PaintCount;
		state.schemeHandle = GetScheme();
		state.frameBgAlpha = GetBgColor().a();
		state.surfaceAlpha = vgui::surface()->DrawGetAlphaMultiplier();
		const bool loaded = m_Preview->VerificationState( state );
		state.previewAvailable = m_PreviewAvailable && loaded;
		return true;
	}

private:
	MESSAGE_FUNC_PARAMS( OnControlChanged, "TextChanged", data )
	{
		if ( data->GetPtr( "panel" ) == m_Characters && m_Preview ) UpdatePreview();
	}
	int SelectedCharacter() const
	{
		const int item = m_Characters->GetActiveItem();
		for ( int n = 0; n < ARRAYSIZE( m_ItemIDs ); ++n )
			if ( m_ItemIDs[n] == item ) return n;
		return -1;
	}
	void UpdatePreview()
	{
		const int selected = SelectedCharacter();
		m_PreviewAvailable = selected >= 0 && m_Preview->ShowCharacter( g_GammaPlayerCharacters[selected].model );
		m_Play->SetEnabled( m_PreviewAvailable );
		m_Status->SetText( m_PreviewAvailable ?
			"Choose your name and character, then play. Your selection will be remembered." :
			"This character's files or animations are not installed." );
	}
	void ExitLauncher()
	{
		if ( m_Closing ) return;
		m_Closing = true;
		if ( m_ExitCallback ) m_ExitCallback( m_Context );
		else vgui::ivgui()->Stop();
	}

	GammaLauncherPlayCallback m_PlayCallback;
	GammaLauncherExitCallback m_ExitCallback;
	void *m_Context;
	bool m_PreviewAvailable, m_Closing;
	unsigned int m_PaintCount;
	int m_ItemIDs[ARRAYSIZE( g_GammaPlayerCharacters )];
	vgui::TextEntry *m_Name;
	vgui::ComboBox *m_Characters;
	vgui::CheckButton *m_ThirdPerson;
	vgui::Label *m_NameLabel, *m_CharacterLabel, *m_Status, *m_PreviewLabel, *m_Help;
	vgui::Button *m_Play, *m_Exit, *m_Reset;
	CGammaLauncherPreview *m_Preview;
};

vgui::Panel *GammaCreateLauncherUI( vgui::VPANEL parent, const GammaLauncherProfile &profile,
	GammaLauncherPlayCallback play, GammaLauncherExitCallback exit, void *context )
{
	return new CGammaStandaloneLauncher( parent, profile, play, exit, context );
}

void GammaResizeLauncherUI( vgui::Panel *panel, int wide, int tall )
{
	if ( !panel ) return;
	panel->SetBounds( 0, 0, wide, tall );
	panel->InvalidateLayout( true );
}

bool GammaLauncherVerifySelect( vgui::Panel *panel, const char *alias )
{
	CGammaStandaloneLauncher *launcher = dynamic_cast<CGammaStandaloneLauncher *>( panel );
	return launcher && launcher->VerifySelect( alias );
}

bool GammaLauncherVerifyProfileSet( vgui::Panel *panel, const GammaLauncherProfile &profile )
{
	CGammaStandaloneLauncher *launcher = dynamic_cast<CGammaStandaloneLauncher *>( panel );
	return launcher && launcher->VerifyProfileSet( profile );
}

bool GammaLauncherVerifyReport( vgui::Panel *panel, GammaLauncherVerificationState &state )
{
	CGammaStandaloneLauncher *launcher = dynamic_cast<CGammaStandaloneLauncher *>( panel );
	return launcher && launcher->VerifyReport( state );
}

void GammaLauncherVerifyPlay( vgui::Panel *panel )
{
	CGammaStandaloneLauncher *launcher = dynamic_cast<CGammaStandaloneLauncher *>( panel );
	if ( launcher ) launcher->OnCommand( "play" );
}
