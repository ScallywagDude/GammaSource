#include "cbase.h"
#include "hud.h"
#include "hud_suitpower.h"
#include "hud_macros.h"
#include "c_basehlplayer.h"
#include "hud_palette.h"
#include "tier0/memdbgon.h"
// HL2MODERN_ENERGY_COUNTER: networked sprint power, never a separate stamina value.
DECLARE_HUDELEMENT(CHudSuitPower);
CHudSuitPower::CHudSuitPower(const char *name)
    : CHudElement(name), CHudNumericDisplay(NULL,"HudSuitPower")
{
    SetHiddenBits(HIDEHUD_PLAYERDEAD | HIDEHUD_NEEDSUIT);
    Init();
}
// Replaces the aux power bar: drawn as a third beta counter (ENERGY) beside HEALTH and HEV.
// Sprint power starts full, so show 100 until the first networked value arrives.
void CHudSuitPower::Init() { m_iEnergy=-1; SetLabelText(L"ENERGY"); SetDisplayValue(100); }
void CHudSuitPower::Reset() { Init(); }
void CHudSuitPower::OnThink()
{
    C_BaseHLPlayer *local=(C_BaseHLPlayer*)C_BasePlayer::GetLocalPlayer();
    int value=local?HL2ModernHud::EnergyValue(local->m_HL2Local.m_flSuitPower):0;
    if(value!=m_iEnergy) { m_iEnergy=value; SetDisplayValue(value); }
}
