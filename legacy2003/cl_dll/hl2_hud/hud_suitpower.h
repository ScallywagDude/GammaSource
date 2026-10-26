// Numeric Energy counter shares Health/HEV rendering and layout.
#ifndef HUD_SUITPOWER_H
#define HUD_SUITPOWER_H
#include "hudelement.h"
#include "hud_numericdisplay.h"
// HL2MODERN_ENERGY_COUNTER
class CHudSuitPower : public CHudElement, public CHudNumericDisplay
{
    DECLARE_CLASS_SIMPLE(CHudSuitPower,CHudNumericDisplay);
public:
    CHudSuitPower(const char *pElementName);
    virtual void Init();
    virtual void Reset();
    virtual void OnThink();
private:
    int m_iEnergy;
};
#endif
