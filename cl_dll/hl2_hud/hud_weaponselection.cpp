//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "weapon_selection.h"
#include "iclientmode.h"
#include "../../modernize/weapon_hud_layout.h"
#include "../../modernize/hl1_weapon_buckets.h"

#include <KeyValues.h>
#include <vgui/IScheme.h>
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui_controls/AnimationController.h>
#include <vgui_controls/Panel.h>

#include <string.h>

//-----------------------------------------------------------------------------
// Purpose: hl2 weapon selection hud element
//-----------------------------------------------------------------------------
class CHudWeaponSelection : public CBaseHudWeaponSelection, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudWeaponSelection, vgui::Panel );

public:
	CHudWeaponSelection(const char *pElementName );

	virtual bool ShouldDraw();
	virtual void OnWeaponPickup( C_BaseCombatWeapon *pWeapon );

	virtual void CycleToNextWeapon( void );
	virtual void CycleToPrevWeapon( void );

	virtual C_BaseCombatWeapon *GetWeaponInSlot( int iSlot, int iSlotPos );
	virtual void SelectWeaponSlot( int iSlot );

	virtual C_BaseCombatWeapon	*GetSelectedWeapon( void )
	{ 
		return m_hSelectedWeapon;
	}

	virtual void OpenSelection( void );
	virtual void HideSelection( void );

protected:
	virtual void OnThink();
	virtual void Paint();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);

private:
	C_BaseCombatWeapon *FindNextWeaponInWeaponSelection(int iCurrentSlot, int iCurrentPosition);
	C_BaseCombatWeapon *FindPrevWeaponInWeaponSelection(int iCurrentSlot, int iCurrentPosition);

	virtual	void SetSelectedWeapon( C_BaseCombatWeapon *pWeapon ) 
	{ 
		m_hSelectedWeapon = pWeapon;
	}

	void DrawBox(int x, int y, int wide, int tall, Color color, float normalizedAlpha, int number);

	CPanelAnimationVar( vgui::HFont, m_hNumberFont, "NumberFont", "HudSelectionNumbers" );
	CPanelAnimationVar( vgui::HFont, m_hTextFont, "TextFont", "HudSelectionText" );

	CPanelAnimationVarAliasType( float, m_flSmallBoxSize, "SmallBoxSize", "32", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flLargeBoxWide, "LargeBoxWide", "108", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flLargeBoxTall, "LargeBoxTall", "72", "proportional_float" );

	CPanelAnimationVarAliasType( float, m_flBoxGap, "BoxGap", "12", "proportional_float" );

	CPanelAnimationVarAliasType( float, m_flSelectionNumberXPos, "SelectionNumberXPos", "4", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flSelectionNumberYPos, "SelectionNumberYPos", "4", "proportional_float" );

	CPanelAnimationVarAliasType( float, m_flIconXPos, "IconXPos", "16", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flIconYPos, "IconYPos", "8", "proportional_float" );

	CPanelAnimationVarAliasType( float, m_flTextYPos, "TextYPos", "54", "proportional_float" );

	CPanelAnimationVar( float, m_flAlphaOverride, "Alpha", "255" );
	CPanelAnimationVar( float, m_flSelectionAlphaOverride, "SelectionAlpha", "255" );


	CPanelAnimationVar( Color, m_TextColor, "TextColor", "SelectionTextFg" );
	CPanelAnimationVar( Color, m_NumberColor, "NumberColor", "SelectionNumberFg" );
	CPanelAnimationVar( Color, m_EmptyBoxColor, "EmptyBoxColor", "SelectionEmptyBoxBg" );
	CPanelAnimationVar( Color, m_BoxColor, "BoxColor", "SelectionBoxBg" );
	CPanelAnimationVar( Color, m_SelectedBoxColor, "SelectedBoxClor", "SelectionSelectedBoxBg" );

	CPanelAnimationVar( float, m_flWeaponPickupGrowTime, "SelectionGrowTime", "0.1" );

	CPanelAnimationVar( float, m_flTextScan, "TextScan", "1.0" );

	CHandle<C_BaseCombatWeapon> m_hLastPickedUpWeapon;
	float m_flPickupStartTime;
	float m_flWeaponPickupDisplayTime;
};

DECLARE_HUDELEMENT( CHudWeaponSelection );

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudWeaponSelection::CHudWeaponSelection( const char *pElementName ) : CBaseHudWeaponSelection(pElementName), BaseClass(NULL, "HudWeaponSelection")
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	// get the sequence time from the animation file
	m_flWeaponPickupDisplayTime = g_pClientMode->GetViewportAnimationController()->GetAnimationSequenceLength("WeaponPickup");;
}

//-----------------------------------------------------------------------------
// Purpose: sets up display for showing weapon pickup
//-----------------------------------------------------------------------------
void CHudWeaponSelection::OnWeaponPickup( C_BaseCombatWeapon *pWeapon )
{
	if (!IsInSelectionMode())
	{
		// show the weapon pickup animation, but only if we're not in weapon selection mode
		m_hLastPickedUpWeapon = pWeapon;
		m_flPickupStartTime = gpGlobals->curtime;

		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("WeaponPickup");
	}
}

//-----------------------------------------------------------------------------
// Purpose: updates animation status
//-----------------------------------------------------------------------------
void CHudWeaponSelection::OnThink()
{
	if (m_hLastPickedUpWeapon)
	{
		// check to see if we should clear out the weapon display
		if (m_flPickupStartTime + m_flWeaponPickupDisplayTime < gpGlobals->curtime)
		{
			m_hLastPickedUpWeapon = NULL;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the panel should draw
//-----------------------------------------------------------------------------
bool CHudWeaponSelection::ShouldDraw()
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
	{
		if ( IsInSelectionMode() )
		{
			HideSelection();
		}
		return false;
	}

	if (m_hLastPickedUpWeapon)
	{
		// Always draw weapon pickups, except when we don't have a suit
		if ( !pPlayer->IsSuitEquipped() )
			return false;

		return true;
	}
	
	bool bret = CBaseHudWeaponSelection::ShouldDraw();
	if ( !bret )
		return false;

	return ( m_bSelectionVisible ) ? true : false;
}

//-------------------------------------------------------------------------
// Purpose: draws the selection area
//-------------------------------------------------------------------------
void CHudWeaponSelection::Paint()
{
    // HL2MODERN_HL1_BUCKETS: original HL1 arrangement, existing 2002 HUD palette.
    // Reference behavior: ValveSoftware/halflife cl_dll/ammo.cpp DrawWList.
    if (!ShouldDraw()) return;
    C_BasePlayer *player=C_BasePlayer::GetLocalPlayer();
    if (!player) return;
    C_BaseCombatWeapon *selected=GetSelectedWeapon();
    if (m_hLastPickedUpWeapon) selected=m_hLastPickedUpWeapon;
    if (!selected) return;
    const int active=selected->GetSlot();
    if (active<0 || active>=MAX_WEAPON_SLOTS) return;
    int count=0,selectedRow=0;
    for (int pos=0;pos<MAX_WEAPON_POSITIONS;++pos)
    {
        C_BaseCombatWeapon *weapon=GetWeaponInSlot(active,pos);
        if (!weapon) continue;
        if (weapon==selected) selectedRow=count;
        ++count;
    }
    HL2ModernHud::BucketLayout layout=HL2ModernHud::WeaponBuckets(GetWide(),GetTall(),MAX_WEAPON_SLOTS,count,selectedRow);
    Color normal=gHUD.m_clrNormal;
    float alpha=m_flSelectionAlphaOverride;
    if (alpha<0) alpha=0;
    if (alpha>255) alpha=255;
    for (int slot=0;slot<MAX_WEAPON_SLOTS;++slot)
    {
        const int x=HL2ModernHud::WeaponBucketX(layout,slot,active);
        const int wide=slot==active?layout.expanded:layout.compact;
        Color header=normal;
        header[3]=(int)(alpha*(slot==active?0.32f:0.12f));
        DrawBox(x,layout.y,wide,layout.header,header,255,slot+1);
        surface()->DrawSetColor(normal[0],normal[1],normal[2],(int)alpha);
        surface()->DrawFilledRect(x,layout.y+layout.header-2,x+wide,layout.y+layout.header);
        int ordinal=0;
        for (int pos=0;pos<MAX_WEAPON_POSITIONS;++pos)
        {
            C_BaseCombatWeapon *pWeapon=GetWeaponInSlot(slot,pos);
            if (!pWeapon) continue;
            Color col=pWeapon->HasAnyAmmo()?normal:gHUD.m_clrCaution;
            const bool isSelected=pWeapon==selected;
            col[3]=(int)(alpha*(isSelected?1.0f:0.7f));
            if (slot!=active)
            {
                const int y=layout.y+layout.header+layout.gap+ordinal*10;
                if (y+6<GetTall())
                {
                    surface()->DrawSetColor(col);
                    surface()->DrawFilledRect(x,y,x+wide,y+6);
                }
                ++ordinal;
                continue;
            }
            const int row=ordinal++;
            if (row<layout.first || row>=layout.first+layout.visible) continue;
            const int y=layout.y+layout.header+layout.gap+(row-layout.first)*(layout.row+layout.gap);
            DrawBox(x,y,wide,layout.row,Color(0,0,0,isSelected?128:64),alpha,-1);
            if (isSelected)
            {
                surface()->DrawSetColor(col);
                surface()->DrawOutlinedRect(x,y,x+wide,y+layout.row);
                surface()->DrawFilledRect(x,y,x+3,y+layout.row);
            }
            // HL2MODERN_WEAPON_HUD_FIT: every original script icon uses the same bounded canvas.
            const CHudTexture *sprite=isSelected?pWeapon->GetSpriteActive():pWeapon->GetSpriteInactive();
            if (!sprite) sprite=pWeapon->GetSpriteActive();
            if (!sprite) sprite=pWeapon->GetSpriteInactive();
            bool bIconDrawn = false; // HL2MODERN_MISSING_WEAPON_ICON
            if (sprite)
            {
                HL2ModernHud::Rect fitted;
                if (HL2ModernHud::FitIcon(sprite->Width(),sprite->Height(),x+6,y+3,wide-12,layout.row-20,fitted))
                {
                    sprite->DrawSelf(fitted.x,fitted.y,fitted.w,fitted.h,col);
                    bIconDrawn = true;
                }
            }
            if (!bIconDrawn)
            {
                surface()->DrawSetTextColor(col);
                surface()->DrawSetTextFont(m_hTextFont);
                surface()->DrawSetTextPos(x+8,y+5);
                const char *fallback = "WPN";
                for (const char *c=fallback;*c;++c) surface()->DrawUnicodeChar((wchar_t)*c);
            }
            surface()->DrawSetTextColor(col);
            surface()->DrawSetTextFont(m_hTextFont);
            surface()->DrawSetTextPos(x+6,y+layout.row-15);
            // HL2MODERN_BUCKET_LOCALIZATION: retain localized names and bounded fallback.
            const char *name=pWeapon->GetWpnData().szPrintName;
            wchar_t converted[128];
            const wchar_t *label=vgui::localize()->Find(name);
            if (!label)
            {
                vgui::localize()->ConvertANSIToUnicode(name,converted,sizeof(converted));
                converted[127]=0;
                label=converted;
            }
            int textWidth=0;
            for (const wchar_t *c=label;*c;++c)
            {
                int characterWidth=surface()->GetCharacterWidth(m_hTextFont,*c);
                if (textWidth+characterWidth>wide-12) break;
                surface()->DrawUnicodeChar(*c);
                textWidth+=characterWidth;
            }
        }
    }
}

//-----------------------------------------------------------------------------
// Purpose: draws a selection box
//-----------------------------------------------------------------------------
void CHudWeaponSelection::DrawBox(int x, int y, int wide, int tall, Color color, float normalizedAlpha, int number)
{
	BaseClass::DrawBox( x, y, wide, tall, color, normalizedAlpha / 255.0f );

	// draw the number
	if (number >= 0)
	{
		Color numberColor = gHUD.m_clrNormal;
		numberColor[3] *= normalizedAlpha / 255.0f;
		surface()->DrawSetTextColor(numberColor);
		surface()->DrawSetTextFont(m_hNumberFont);
		wchar_t wch = '0' + number;
		surface()->DrawSetTextPos(x + m_flSelectionNumberXPos, y + m_flSelectionNumberYPos);
		surface()->DrawUnicodeChar(wch);
	}
}

//-----------------------------------------------------------------------------
// Purpose: hud scheme settings
//-----------------------------------------------------------------------------
void CHudWeaponSelection::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetPaintBackgroundEnabled(false);

	// set our size
	int screenWide, screenTall;
	int x, y;
	GetPos(x, y);
	surface()->GetScreenSize(screenWide, screenTall);
	SetBounds(0, y, screenWide, screenTall - y);
}

//-----------------------------------------------------------------------------
// Purpose: Opens weapon selection control
//-----------------------------------------------------------------------------
void CHudWeaponSelection::OpenSelection( void )
{
	Assert(!IsInSelectionMode());

	m_hLastPickedUpWeapon = NULL;
	CBaseHudWeaponSelection::OpenSelection();
	g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("OpenWeaponSelectionMenu");
}

//-----------------------------------------------------------------------------
// Purpose: Closes weapon selection control
//-----------------------------------------------------------------------------
void CHudWeaponSelection::HideSelection( void )
{
	CBaseHudWeaponSelection::HideSelection();
	g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("CloseWeaponSelectionMenu");
}

//-----------------------------------------------------------------------------
// Purpose: Returns the next available weapon item in the weapon selection
//-----------------------------------------------------------------------------
C_BaseCombatWeapon *CHudWeaponSelection::FindNextWeaponInWeaponSelection(int iCurrentSlot, int iCurrentPosition)
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return NULL;

	C_BaseCombatWeapon *pNextWeapon = NULL;

	// search all the weapons looking for the closest next
	int iLowestNextSlot = MAX_WEAPON_SLOTS;
	int iLowestNextPosition = MAX_WEAPON_POSITIONS;
	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		C_BaseCombatWeapon *pWeapon = pPlayer->GetWeapon(i);
		if ( !pWeapon )
			continue;

		if ( pWeapon->CanBeSelected() )
		{
			int weaponSlot = pWeapon->GetSlot(), weaponPosition = pWeapon->GetPosition();

			// see if this weapon is further ahead in the selection list
			if ( weaponSlot > iCurrentSlot || (weaponSlot == iCurrentSlot && weaponPosition > iCurrentPosition) )
			{
				// see if this weapon is closer than the current lowest
				if ( weaponSlot < iLowestNextSlot || (weaponSlot == iLowestNextSlot && weaponPosition < iLowestNextPosition) )
				{
					iLowestNextSlot = weaponSlot;
					iLowestNextPosition = weaponPosition;
					pNextWeapon = pWeapon;
				}
			}
		}
	}

	return pNextWeapon;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the prior available weapon item in the weapon selection
//-----------------------------------------------------------------------------
C_BaseCombatWeapon *CHudWeaponSelection::FindPrevWeaponInWeaponSelection(int iCurrentSlot, int iCurrentPosition)
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return NULL;

	C_BaseCombatWeapon *pPrevWeapon = NULL;

	// search all the weapons looking for the closest next
	int iLowestPrevSlot = -1;
	int iLowestPrevPosition = -1;
	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		C_BaseCombatWeapon *pWeapon = pPlayer->GetWeapon(i);
		if ( !pWeapon )
			continue;

		if ( pWeapon->CanBeSelected() )
		{
			int weaponSlot = pWeapon->GetSlot(), weaponPosition = pWeapon->GetPosition();

			// see if this weapon is further ahead in the selection list
			if ( weaponSlot < iCurrentSlot || (weaponSlot == iCurrentSlot && weaponPosition < iCurrentPosition) )
			{
				// see if this weapon is closer than the current lowest
				if ( weaponSlot > iLowestPrevSlot || (weaponSlot == iLowestPrevSlot && weaponPosition > iLowestPrevPosition) )
				{
					iLowestPrevSlot = weaponSlot;
					iLowestPrevPosition = weaponPosition;
					pPrevWeapon = pWeapon;
				}
			}
		}
	}

	return pPrevWeapon;
}

//-----------------------------------------------------------------------------
// Purpose: Moves the selection to the next item in the menu
//-----------------------------------------------------------------------------
void CHudWeaponSelection::CycleToNextWeapon( void )
{
	// Get the local player.
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	C_BaseCombatWeapon *pNextWeapon = NULL;
	if ( IsInSelectionMode() )
	{
		// find the next selection spot
		C_BaseCombatWeapon *pWeapon = GetSelectedWeapon();
		if ( !pWeapon )
			return;

		pNextWeapon = FindNextWeaponInWeaponSelection( pWeapon->GetSlot(), pWeapon->GetPosition() );
	}
	else
	{
		// open selection at the current place
		pNextWeapon = pPlayer->GetActiveWeapon();
		if ( pNextWeapon )
		{
			pNextWeapon = FindNextWeaponInWeaponSelection( pNextWeapon->GetSlot(), pNextWeapon->GetPosition() );
		}
	}

	if ( !pNextWeapon )
	{
		// wrap around back to start
		pNextWeapon = FindNextWeaponInWeaponSelection(-1, -1);
	}

	if ( pNextWeapon )
	{
		SetSelectedWeapon( pNextWeapon );

		if ( !IsInSelectionMode() )
		{
			OpenSelection();
		}

		// Play the "cycle to next weapon" sound
		pPlayer->EmitSound( "Player.WeaponSelectionMoveSlot" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Moves the selection to the previous item in the menu
//-----------------------------------------------------------------------------
void CHudWeaponSelection::CycleToPrevWeapon( void )
{
	// Get the local player.
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	C_BaseCombatWeapon *pNextWeapon = NULL;
	if ( IsInSelectionMode() )
	{
		// find the next selection spot
		C_BaseCombatWeapon *pWeapon = GetSelectedWeapon();
		if ( !pWeapon )
			return;

		pNextWeapon = FindPrevWeaponInWeaponSelection( pWeapon->GetSlot(), pWeapon->GetPosition() );
	}
	else
	{
		// open selection at the current place
		pNextWeapon = pPlayer->GetActiveWeapon();
		if ( pNextWeapon )
		{
			pNextWeapon = FindPrevWeaponInWeaponSelection( pNextWeapon->GetSlot(), pNextWeapon->GetPosition() );
		}
	}

	if ( !pNextWeapon )
	{
		// wrap around back to end of weapon list
		pNextWeapon = FindPrevWeaponInWeaponSelection(MAX_WEAPON_SLOTS, MAX_WEAPON_POSITIONS);
	}

	if ( pNextWeapon )
	{
		SetSelectedWeapon( pNextWeapon );

		if ( !IsInSelectionMode() )
		{
			OpenSelection();
		}

		// Play the "cycle to next weapon" sound
		pPlayer->EmitSound( "Player.WeaponSelectionMoveSlot" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns the weapon in the specified slot
//-----------------------------------------------------------------------------
C_BaseCombatWeapon *CHudWeaponSelection::GetWeaponInSlot( int iSlot, int iSlotPos )
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if ( !player )
		return NULL;

	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		C_BaseCombatWeapon *pWeapon = player->GetWeapon(i);
		
		if ( pWeapon == NULL )
			continue;

		if ( pWeapon->GetSlot() == iSlot && pWeapon->GetPosition() == iSlotPos )
			return pWeapon;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Moves selection to the specified slot
//-----------------------------------------------------------------------------
void CHudWeaponSelection::SelectWeaponSlot( int iSlot )
{
	// iSlot is one higher than it should be, since it's the number key, not the 0-based index into the weapons
	--iSlot;

	// Get the local player.
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	// Don't try and read past our possible number of slots
	if ( iSlot<0 || iSlot>=MAX_WEAPON_SLOTS )
		return;
	
	// Make sure the player's allowed to switch weapons
	if ( pPlayer->IsAllowedToSwitchWeapons() == false )
		return;

	int slotPos = 0;
	C_BaseCombatWeapon *pActiveWeapon = GetSelectedWeapon();

	// start later in the list
	if ( IsInSelectionMode() && pActiveWeapon && pActiveWeapon->GetSlot() == iSlot )
	{
		slotPos = pActiveWeapon->GetPosition() + 1;
	}

	// find the weapon in this slot
	pActiveWeapon = GetNextActivePos( iSlot, slotPos );
	if ( !pActiveWeapon )
	{
		pActiveWeapon = GetNextActivePos( iSlot, 0 );
	}
	
	if ( pActiveWeapon != NULL )
	{
		if ( !IsInSelectionMode() )
		{
			// open the weapon selection
			OpenSelection();
		}

		// Mark the change
		SetSelectedWeapon( pActiveWeapon );
	}

	pPlayer->EmitSound( "Player.WeaponSelectionMoveSlot" );
}
