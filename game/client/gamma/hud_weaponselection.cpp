//========= GammaSource: ported from the 2003 code (cl_dll/hl2_hud/hud_weaponselection.cpp) ============//
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "weapon_selection.h"
#include "iclientmode.h"
#include "weapon_hud_layout.h"
#include "hud_color_client.h"
#include "hl1_weapon_buckets.h"
#include "ammodef.h"

#include <KeyValues.h>
#include <vgui/IScheme.h>
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui_controls/AnimationController.h>
#include <vgui_controls/Panel.h>

#include <string.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

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
    // HL1 weapon selection layout drawn with 2002 beta HUD pieces: a row of
    // numbered bucket boxes, the open bucket expanded into a column of weapon
    // icons, HL1's corner-bracket selection frame, per-weapon ammo bars, and
    // the beta HUD's bitmap digits and amber palette.
    if(!ShouldDraw())return;
    C_BasePlayer *player=C_BasePlayer::GetLocalPlayer();
    if(!player)return;
    C_BaseCombatWeapon *selected=GetSelectedWeapon();
    if(m_hLastPickedUpWeapon)selected=m_hLastPickedUpWeapon;
    if(!selected)return;
    const int active=selected->GetSlot();
    if(active<0 || active>=MAX_WEAPON_SLOTS)return;

    // Five buckets as in HL1; later slots appear only when they hold something.
    int slots=MAX_WEAPON_SLOTS<5?MAX_WEAPON_SLOTS:5;
    for(int slot=5;slot<MAX_WEAPON_SLOTS;++slot)
    {
        if(slot==active)slots=slot+1;
        for(int pos=0;pos<MAX_WEAPON_POSITIONS;++pos)
            if(GetWeaponInSlot(slot,pos))slots=slot+1;
    }

    const int originX=16, originY=8;
    const int box=22, gap=4;              // bucket header boxes
    const int iconW=128, iconH=64;        // native beta w_icons cell
    const int stripH=6;                   // HL1 per-weapon strip in closed buckets

    float alpha=m_flSelectionAlphaOverride;
    if(alpha<0)alpha=0;
    if(alpha>255)alpha=255;
    Color ink=HL2ModernHudColor(Color(232,204,107,255));
    ink[3]=(int)alpha;
    Color dim=ink;
    dim[3]=(int)(alpha*0.45f);

    // Rows visible below the header, scrolled so the selection stays on screen.
    int count=0,selectedRow=0;
    for(int pos=0;pos<MAX_WEAPON_POSITIONS;++pos)
    {
        C_BaseCombatWeapon *weapon=GetWeaponInSlot(active,pos);
        if(!weapon)continue;
        if(weapon==selected)selectedRow=count;
        ++count;
    }
    // Leave room for the fixed health/HEV/ammo HUD along the bottom edge.
    int visible=(GetTall()-originY-box-gap-64)/(iconH+gap);
    if(visible<1)visible=1;
    int first=0;
    if(selectedRow>=visible)first=selectedRow-visible+1;

    CHudTexture *frame=gHUD.GetIcon("hl1_selection");
    int x=originX;
    for(int slot=0;slot<slots;++slot)
    {
        const bool open=(slot==active);
        const int wide=open?iconW:box;

        // Bucket box with the beta bitmap digit (font fallback).
        surface()->DrawSetColor(open?ink:dim);
        surface()->DrawOutlinedRect(x,originY,x+box,originY+box);
        char digitName[16];
        Q_snprintf(digitName,sizeof(digitName),"number_%d",(slot+1)%10);
        CHudTexture *digit=gHUD.GetIcon(digitName);
        if(digit && digit->Width()>0)
            digit->DrawSelf(x+6,originY+4,10,14,open?ink:dim);
        else
        {
            surface()->DrawSetTextFont(m_hNumberFont);
            surface()->DrawSetTextColor(open?ink:dim);
            surface()->DrawSetTextPos(x+6,originY+3);
            surface()->DrawUnicodeChar((wchar_t)('1'+slot));
        }

        if(open && count>visible)
        {
            // The bucket scrolls with the selection; show where its visible rows sit.
            wchar_t range[32];
            V_snwprintf(range,ARRAYSIZE(range),L"%d-%d/%d",first+1,MIN(first+visible,count),count);
            surface()->DrawSetTextFont(m_hTextFont);
            surface()->DrawSetTextColor(ink);
            surface()->DrawSetTextPos(x+box+gap,originY+5);
            surface()->DrawPrintText(range,wcslen(range));
        }
        int y=originY+box+gap;
        int row=0;
        for(int pos=0;pos<MAX_WEAPON_POSITIONS;++pos)
        {
            C_BaseCombatWeapon *weapon=GetWeaponInSlot(slot,pos);
            if(!weapon)continue;
            const bool hasAmmo=weapon->HasAnyAmmo();
            Color tint=hasAmmo?ink:gHUD.m_clrCaution;

            if(!open)
            {
                // HL1 shows each weapon of a closed bucket as a small strip.
                tint[3]=(int)(alpha*0.45f);
                surface()->DrawSetColor(tint);
                surface()->DrawFilledRect(x,y,x+box,y+stripH);
                y+=stripH+2;
                continue;
            }

            const int index=row++;
            if(index<first || index>=first+visible)continue;
            const int top=originY+box+gap+(index-first)*(iconH+gap);
            const bool current=(weapon==selected);
            tint[3]=(int)(alpha*(current?1.0f:0.55f));

            const CHudTexture *sprite=current?weapon->GetSpriteActive():weapon->GetSpriteInactive();
            if(!sprite)sprite=weapon->GetSpriteActive();
            if(!sprite)sprite=weapon->GetSpriteInactive();
            bool drawn=false;
            if(sprite && sprite->Width()>0 && sprite->Height()>0 &&
               (sprite->bRenderUsingFont ? sprite->hFont!=vgui::INVALID_FONT : sprite->textureId!=-1))
            {
                // Beta icons are drawn at their native 128x64; HL1 170x45 art is fitted.
                HL2ModernHud::Rect fitted;
                if(sprite->Width()==iconW && sprite->Height()==iconH)
                {
                    sprite->DrawSelf(x,top,iconW,iconH,tint);
                    drawn=true;
                }
                else if(HL2ModernHud::FitIcon(sprite->Width(),sprite->Height(),x,top,iconW,iconH,fitted))
                {
                    sprite->DrawSelf(fitted.x,fitted.y,fitted.w,fitted.h,tint);
                    drawn=true;
                }
            }
            if(!drawn)
            {
                const char *name=weapon->GetWpnData().szPrintName;
                if(!name || !name[0])name=weapon->GetClassname();
                wchar_t converted[128];
                const wchar_t *label=g_pVGuiLocalize->Find(name);
                if(!label)
                {
                    g_pVGuiLocalize->ConvertANSIToUnicode(name,converted,sizeof(converted));
                    converted[127]=0;
                    label=converted;
                }
                surface()->DrawSetTextFont(m_hTextFont);
                surface()->DrawSetTextColor(tint);
                surface()->DrawSetTextPos(x+6,top+iconH/2-6);
                int used=0;
                for(const wchar_t *ch=label;*ch;++ch)
                {
                    int glyph=surface()->GetCharacterWidth(m_hTextFont,*ch);
                    if(used+glyph>iconW-16)break;
                    surface()->DrawUnicodeChar(*ch);
                    used+=glyph;
                }
            }

            // HL1 ammo bar on the right edge: clip for clip weapons, reserve otherwise.
            const int type=weapon->GetPrimaryAmmoType();
            if(type>=0)
            {
                float fraction=0.0f;
                if(weapon->UsesClipsForAmmo1() && weapon->GetMaxClip1()>0)
                    fraction=(float)weapon->Clip1()/(float)weapon->GetMaxClip1();
                else
                {
                    const int carry=GetAmmoDef()->MaxCarry(type);
                    if(carry>0)fraction=(float)player->GetAmmoCount(type)/(float)carry;
                }
                if(fraction<0.0f)fraction=0.0f;
                if(fraction>1.0f)fraction=1.0f;
                const int barX=x+iconW-7, barTop=top+8, barBottom=top+iconH-8;
                Color empty=tint;
                empty[3]=tint[3]/3;
                surface()->DrawSetColor(empty);
                surface()->DrawFilledRect(barX,barTop,barX+4,barBottom);
                const int filled=(int)((barBottom-barTop)*fraction+0.5f);
                surface()->DrawSetColor(tint);
                surface()->DrawFilledRect(barX,barBottom-filled,barX+4,barBottom);
            }

            if(current)
            {
                if(frame && frame->Width()>0)
                    frame->DrawSelf(x-3,top-3,iconW+6,iconH+6,ink);
                else
                {
                    surface()->DrawSetColor(ink);
                    surface()->DrawOutlinedRect(x-3,top-3,x+iconW+3,top+iconH+3);
                }
            }
        }
        x+=wide+gap;
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
	// The 2003 scheme does not define the retail HudSelection* font aliases.
	if(m_hTextFont==vgui::INVALID_FONT)m_hTextFont=pScheme->GetFont("DefaultSmall",IsProportional());
	if(m_hNumberFont==vgui::INVALID_FONT)m_hNumberFont=pScheme->GetFont("DefaultSmall",IsProportional());

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
