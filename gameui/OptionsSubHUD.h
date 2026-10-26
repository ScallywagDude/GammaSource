//========= GammaSource: the 2003 build's HUD color options tab ============//
//
// HUD options share the client's archived console variables (hud_color_custom,
// hud_color_r/g/b). Changes apply on OK/Apply.
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
		m_pCustom->SetBounds( 20, 18, 350, 24 );
		m_pCustom->AddActionSignalTarget( this );

		static const char *s_Names[] = { "Red", "Green", "Blue" };
		for ( int i = 0; i < 3; ++i )
		{
			char labelName[32]; Q_snprintf( labelName, sizeof( labelName ), "%sLabel", s_Names[i] );
			m_pLabels[i] = new vgui::Label( this, labelName, s_Names[i] );
			vgui::Label *label = m_pLabels[i];
			label->SetBounds( 20, 58 + i * 40, 60, 24 );
			m_pRGB[i] = new vgui::Slider( this, s_Names[i] );
			m_pRGB[i]->SetBounds( 84, 58 + i * 40, 280, 28 );
			m_pRGB[i]->SetRange( 0, 255 );
			m_pRGB[i]->SetTickCaptions( "0", "255" );
			m_pRGB[i]->AddActionSignalTarget( this );
			Q_snprintf( labelName, sizeof( labelName ), "%sValue", s_Names[i] );
			m_pValue[i] = new vgui::Label( this, labelName, "" );
			m_pValue[i]->SetBounds( 374, 58 + i * 40, 60, 24 );
		}

		m_pPreview = new vgui::Label( this, "Preview", "HEALTH 100    HEV 100    ENERGY 100" );
		m_pPreview->SetBounds( 20, 190, 440, 32 );

		m_pDefaults = new vgui::Button( this, "Defaults", "Restore 2002 color" );
		vgui::Button *reset = m_pDefaults;
		reset->SetCommand( "HUDDefaults" );
		reset->AddActionSignalTarget( this );
		reset->SetBounds( 20, 236, 190, 26 );

		m_pHint = new vgui::Label( this, "Hint", "HUD changes are saved when you press Apply or OK." );
		vgui::Label *hint = m_pHint;
		hint->SetBounds( 20, 270, 440, 24 );

		OnResetData();
	}

	virtual void PerformLayout()
	{
		BaseClass::PerformLayout();
		// VGUI's later scheme pass can reset constructor-only control bounds.
		m_pCustom->SetBounds(20,18,350,24);
		for(int i=0;i<3;++i)
		{
			m_pLabels[i]->SetBounds(20,58+i*40,60,24);
			m_pRGB[i]->SetBounds(84,58+i*40,280,28);
			m_pValue[i]->SetBounds(374,58+i*40,60,24);
		}
		m_pPreview->SetBounds(20,190,440,32);
		m_pDefaults->SetBounds(20,236,190,26);
		m_pHint->SetBounds(20,270,440,24);
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
	}

	virtual void OnCommand( const char *command )
	{
		if ( !Q_stricmp( command, "HUDDefaults" ) )
		{
			m_pCustom->SetSelected( false );
			for ( int i = 0; i < 3; ++i )
				m_pRGB[i]->SetValue( s_Defaults[i], false );
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
	}

	static const char *s_ColorVars[3];
	static const int s_Defaults[3];

	vgui::CheckButton *m_pCustom;
	vgui::Slider *m_pRGB[3];
	vgui::Label *m_pPreview;
	vgui::Label *m_pValue[3];
	vgui::Label *m_pLabels[3];
	vgui::Button *m_pDefaults;
	vgui::Label *m_pHint;
};

inline void COptionsSubHUD::OnColorChanged()
{
	UpdatePreview();
	PostActionSignal( new KeyValues( "ApplyButtonEnable" ) );
}

// the 2002/2003 beta HUD amber
const char *COptionsSubHUD::s_ColorVars[3] = { "hud_color_r", "hud_color_g", "hud_color_b" };
const int COptionsSubHUD::s_Defaults[3] = { 247, 192, 84 };

#endif // OPTIONSSUBHUD_H
