//========= GammaSource: the 2003 build's HUD color options tab ============//
//
// HUD options share the client's archived console variables (hud_color_custom,
// hud_color_r/g/b). Changes apply on OK/Apply.
// The right column sets the portal gun's colours (cl_portal_color1/2, cl_portal_color_random).
//
//=============================================================================//
#ifndef OPTIONSSUBHUD_H
#define OPTIONSSUBHUD_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/convar.h"
#include "vgui_controls/PropertyPage.h"
#include "vgui_controls/CheckButton.h"
#include "vgui_controls/Slider.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/Button.h"
#include "KeyValues.h"

class COptionsSubHUD : public vgui::PropertyPage
{
	DECLARE_CLASS_SIMPLE( COptionsSubHUD, vgui::PropertyPage );

public:
	COptionsSubHUD( vgui::Panel *parent ) : BaseClass( parent, "HUD" )
	{
		m_pCustom = new vgui::CheckButton( this, "CustomHUDColor", "Use custom HUD color" );
		m_pCustom->AddActionSignalTarget( this );

		for ( int i = 0; i < 3; ++i )
		{
			char labelName[32]; Q_snprintf( labelName, sizeof( labelName ), "%sLabel", s_Names[i] );
			m_pLabels[i] = new vgui::Label( this, labelName, s_Names[i] );
			m_pRGB[i] = new vgui::Slider( this, s_Names[i] );
			m_pRGB[i]->SetRange( 0, 255 );
			m_pRGB[i]->AddActionSignalTarget( this );
			Q_snprintf( labelName, sizeof( labelName ), "%sValue", s_Names[i] );
			m_pValue[i] = new vgui::Label( this, labelName, "" );
		}

		m_pPreview = new vgui::Label( this, "Preview", "HEALTH 100   ENERGY 100" );

		m_pDefaults = new vgui::Button( this, "Defaults", "Restore default colors" );
		m_pDefaults->SetCommand( "HUDDefaults" );
		m_pDefaults->AddActionSignalTarget( this );

		m_pHint = new vgui::Label( this, "Hint", "Changes are saved when you press Apply or OK." );

		// Portal gun colours
		m_pPortalTitle = new vgui::Label( this, "PortalColorsLabel", "Portal colors" );
		m_pPortalRandom = new vgui::CheckButton( this, "PortalColorRandom", "Random" );
		m_pPortalRandom->AddActionSignalTarget( this );
		for ( int p = 0; p < 2; ++p )
		{
			char name[32];
			Q_snprintf( name, sizeof( name ), "Portal%dLabel", p + 1 );
			m_pPortalLabel[p] = new vgui::Label( this, name, p ? "Portal 2" : "Portal 1" );
			for ( int c = 0; c < 3; ++c )
			{
				Q_snprintf( name, sizeof( name ), "Portal%d%s", p + 1, s_Names[c] );
				m_pPortalRGB[p][c] = new vgui::Slider( this, name );
				m_pPortalRGB[p][c]->SetRange( 0, 255 );
				m_pPortalRGB[p][c]->AddActionSignalTarget( this );
			}
			Q_snprintf( name, sizeof( name ), "Portal%dSwatch", p + 1 );
			m_pPortalSwatch[p] = new vgui::Panel( this, name );
			m_pPortalSwatch[p]->SetPaintBackgroundEnabled( true );
		}

		LayoutControls();
		OnResetData();
	}

	virtual void PerformLayout()
	{
		BaseClass::PerformLayout();
		// VGUI's later scheme pass can reset constructor-only control bounds.
		LayoutControls();
		UpdatePreview();
	}

	virtual void OnResetData()
	{
		ConVarRef custom( "hud_color_custom", true );
		m_pCustom->SetSelected( custom.IsValid() && custom.GetBool() );
		for ( int i = 0; i < 3; ++i )
		{
			ConVarRef var( s_ColorVars[i], true );
			m_pRGB[i]->SetValue( var.IsValid() ? ColorByte( var.GetInt() ) : s_Defaults[i], false );
		}
		for ( int p = 0; p < 2; ++p )
		{
			ConVarRef var( s_PortalVars[p], true );
			int rgb[3] = { s_PortalDefaults[p][0], s_PortalDefaults[p][1], s_PortalDefaults[p][2] };
			if ( var.IsValid() )
				sscanf( var.GetString(), "%d %d %d", &rgb[0], &rgb[1], &rgb[2] );
			for ( int c = 0; c < 3; ++c )
				m_pPortalRGB[p][c]->SetValue( ColorByte( rgb[c] ), false );
		}
		ConVarRef random( "cl_portal_color_random", true );
		m_pPortalRandom->SetSelected( random.IsValid() && random.GetBool() );
		UpdatePreview();
	}

	virtual void OnApplyChanges()
	{
		for ( int i = 0; i < 3; ++i )
		{
			ConVarRef var( s_ColorVars[i], true );
			if ( var.IsValid() )
				var.SetValue( ColorByte( m_pRGB[i]->GetValue() ) );
		}
		ConVarRef custom( "hud_color_custom", true );
		if ( custom.IsValid() )
			custom.SetValue( m_pCustom->IsSelected() ? 1 : 0 );

		for ( int p = 0; p < 2; ++p )
		{
			ConVarRef var( s_PortalVars[p], true );
			if ( var.IsValid() )
			{
				char value[32];
				Q_snprintf( value, sizeof( value ), "%d %d %d", ColorByte( m_pPortalRGB[p][0]->GetValue() ),
					ColorByte( m_pPortalRGB[p][1]->GetValue() ), ColorByte( m_pPortalRGB[p][2]->GetValue() ) );
				var.SetValue( value );
			}
		}
		ConVarRef random( "cl_portal_color_random", true );
		if ( random.IsValid() )
			random.SetValue( m_pPortalRandom->IsSelected() ? 1 : 0 );
	}

	virtual void OnCommand( const char *command )
	{
		if ( !Q_stricmp( command, "HUDDefaults" ) )
		{
			m_pCustom->SetSelected( false );
			for ( int i = 0; i < 3; ++i )
				m_pRGB[i]->SetValue( s_Defaults[i], false );
			m_pPortalRandom->SetSelected( false );
			for ( int p = 0; p < 2; ++p )
				for ( int c = 0; c < 3; ++c )
					m_pPortalRGB[p][c]->SetValue( s_PortalDefaults[p][c], false );
			OnColorChanged();
		}
		else
		{
			BaseClass::OnCommand( command );
		}
	}

private:
	MESSAGE_FUNC( OnColorChanged, "SliderMoved" );
	MESSAGE_FUNC( OnCheckButtonChecked, "CheckButtonChecked" ) { OnColorChanged(); }

	static int ColorByte( int value ) { return value < 0 ? 0 : ( value > 255 ? 255 : value ); }

	// Two columns: the HUD colour on the left, the portal colours on the right
	void LayoutControls()
	{
		m_pCustom->SetBounds( 12, 8, 220, 24 );
		for ( int i = 0; i < 3; ++i )
		{
			m_pLabels[i]->SetBounds( 12, 40 + i * 30, 44, 24 );
			m_pRGB[i]->SetBounds( 56, 40 + i * 30, 130, 28 );
			m_pValue[i]->SetBounds( 192, 40 + i * 30, 36, 24 );
		}
		m_pPreview->SetBounds( 12, 132, 220, 24 );
		m_pDefaults->SetBounds( 12, 164, 170, 26 );

		m_pPortalTitle->SetBounds( 250, 8, 100, 24 );
		m_pPortalRandom->SetBounds( 356, 8, 110, 24 );
		for ( int p = 0; p < 2; ++p )
		{
			int y = 36 + p * 90;
			m_pPortalLabel[p]->SetBounds( 250, y, 100, 20 );
			m_pPortalSwatch[p]->SetBounds( 420, y + 2, 44, 16 );
			for ( int c = 0; c < 3; ++c )
				m_pPortalRGB[p][c]->SetBounds( 250, y + 20 + c * 22, 214, 22 );
		}

		m_pHint->SetBounds( 12, 218, 460, 24 );
	}

	void UpdatePreview()
	{
		int rgb[3];
		for ( int i = 0; i < 3; ++i )
		{
			rgb[i] = ColorByte( m_pRGB[i]->GetValue() );
			char value[8];
			Q_snprintf( value, sizeof( value ), "%d", rgb[i] );
			m_pValue[i]->SetText( value );
			m_pRGB[i]->SetEnabled( m_pCustom->IsSelected() );
			if ( !m_pCustom->IsSelected() )
				rgb[i] = s_Defaults[i];
		}
		m_pPreview->SetFgColor( Color( rgb[0], rgb[1], rgb[2], 255 ) );

		bool bRandom = m_pPortalRandom->IsSelected();
		for ( int p = 0; p < 2; ++p )
		{
			for ( int c = 0; c < 3; ++c )
				m_pPortalRGB[p][c]->SetEnabled( !bRandom );
			m_pPortalSwatch[p]->SetBgColor( Color( ColorByte( m_pPortalRGB[p][0]->GetValue() ),
				ColorByte( m_pPortalRGB[p][1]->GetValue() ), ColorByte( m_pPortalRGB[p][2]->GetValue() ), 255 ) );
			m_pPortalSwatch[p]->SetVisible( !bRandom );
		}
	}

	static const char *s_Names[3];
	static const char *s_ColorVars[3];
	static const int s_Defaults[3];
	static const char *s_PortalVars[2];
	static const int s_PortalDefaults[2][3];

	vgui::CheckButton *m_pCustom;
	vgui::Slider *m_pRGB[3];
	vgui::Label *m_pPreview;
	vgui::Label *m_pValue[3];
	vgui::Label *m_pLabels[3];
	vgui::Button *m_pDefaults;
	vgui::Label *m_pHint;

	vgui::Label *m_pPortalTitle;
	vgui::CheckButton *m_pPortalRandom;
	vgui::Label *m_pPortalLabel[2];
	vgui::Slider *m_pPortalRGB[2][3];
	vgui::Panel *m_pPortalSwatch[2];
};

inline void COptionsSubHUD::OnColorChanged()
{
	UpdatePreview();
	PostActionSignal( new KeyValues( "ApplyButtonEnable" ) );
}

const char *COptionsSubHUD::s_Names[3] = { "Red", "Green", "Blue" };
// the 2002/2003 beta HUD amber
const char *COptionsSubHUD::s_ColorVars[3] = { "hud_color_r", "hud_color_g", "hud_color_b" };
const int COptionsSubHUD::s_Defaults[3] = { 247, 192, 84 };
// Portal's own blue and orange
const char *COptionsSubHUD::s_PortalVars[2] = { "cl_portal_color1", "cl_portal_color2" };
const int COptionsSubHUD::s_PortalDefaults[2][3] = { { 64, 160, 255 }, { 255, 160, 32 } };

#endif // OPTIONSSUBHUD_H
