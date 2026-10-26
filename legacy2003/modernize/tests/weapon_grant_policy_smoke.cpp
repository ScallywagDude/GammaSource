#include "../weapon_grant_policy.h"
#include <cassert>
#include <climits>
int main() {
    using namespace HL2ModernWeapons;
    assert(WeaponClass("weapon_physgun"));
    assert(WeaponClass("weapon_357"));
    assert(!WeaponClass("weapon_"));
    assert(!WeaponClass("npc_manhack"));
    assert(!WeaponClass("weapon_pistol;quit"));
    assert(!WeaponClass("weapon_../../escape"));
    assert(!WeaponClass(0));
    assert(ModelPath("models/Weapons/v_irifle.mdl"));
    assert(ModelPath("MODELS/weapons/w_gun.MDL"));
    assert(!ModelPath("models/.mdl"));
    assert(!ModelPath("models/weapons/.mdl"));
    assert(!ModelPath("models/../escape.mdl"));
    assert(!ModelPath("models/weapons/../../escape.mdl"));
    assert(!ModelPath("models//escape.mdl"));
    assert(!ModelPath("models/./escape.mdl"));
    assert(!ModelPath("C:/models/gun.mdl"));
    assert(!ModelPath("models\\gun.mdl"));
    assert(!ModelPath("models/gun.mdl?x"));
    assert(!ModelPath("models/gun.vtf"));
    assert(!ModelPath(0));
    assert(AmmoNeeded(250,0)==250);
    assert(AmmoNeeded(250,100)==150);
    assert(AmmoNeeded(250,250)==0);
    assert(AmmoNeeded(250,300)==0);
    assert(AmmoNeeded(-2,0)==0);
    assert(AmmoNeeded(250,-1)==0);
    assert(AmmoNeeded(INT_MAX,0)==INT_MAX);
    unsigned char header[80]={0};
    header[0]='I';header[1]='D';header[2]='S';header[3]='T';header[4]=36;header[76]=80;
    assert(StudioHeader(header,80,80,36));
    assert(!StudioHeader(header,80,80,36,128));
    header[4]=35;assert(StudioHeader(header,80,80,36));
    header[4]=37;assert(!StudioHeader(header,80,80,36));
    header[4]=36;assert(!StudioHeader(header,79,80,36));
    header[76]=81;assert(!StudioHeader(header,80,80,36));
    header[76]=79;assert(!StudioHeader(header,80,80,36));
    header[0]='X';assert(!StudioHeader(header,80,80,36));
    return 0;
}
