"""Exercise the registered brickbat's actual typed pickup and throw methods."""
from pathlib import Path
import json
import shutil
import subprocess
import tempfile
import unittest
from modernize.tests.test_stickylauncher_spawn import method
ROOT=Path(__file__).resolve().parents[2]
HARNESS=r'''
#include <cassert>
#include <cstdarg>
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
struct Vector { float x,y,z; Vector(float a=0,float b=0,float c=0):x(a),y(b),z(c){} Vector operator+(const Vector&v)const{return Vector(x+v.x,y+v.y,z+v.z);} Vector operator*(float f)const{return Vector(x*f,y*f,z*f);} };
typedef Vector QAngle;typedef Vector AngularImpulse;const QAngle vec3_angle;
void AngularImpulseToQAngle(const AngularImpulse&a,QAngle&v){v=a;}
struct Globals {float curtime;}globals;Globals*gpGlobals=&globals;
enum {NUM_BRICKBAT_AMMO_TYPES=3,BRICKBAT_ROCK=0,ACT_VM_PULLBACK=1,ACT_VM_THROW=2,AUTOAIM_5DEGREES=0,IN_ATTACK=1,IN_ATTACK2=2};
struct Ammo {const char*m_sClassName;int m_nMaxCarry;}BrickBatAmmoArray[3]={{"rock",5},{"bottle",3},{"head",1}};
struct CWeaponBrickbat;struct CBaseCombatCharacter;
struct CBaseEntity {
 bool removed;static CBaseEntity*result;static int creates;CBaseEntity():removed(false){}virtual ~CBaseEntity(){}
 static CBaseEntity*Create(const char*,const Vector&,const Vector&,CBaseCombatCharacter*){++creates;return result;}
 bool IsMarkedForDeletion(){return removed;}
};
CBaseEntity*CBaseEntity::result=0;int CBaseEntity::creates=0;
void UTIL_Remove(CBaseEntity*entity){entity->removed=true;}
void Msg(const char*,...){}
struct CBaseCombatWeapon:CBaseEntity{};
struct CBaseCombatCharacter:CBaseEntity {
 int ammo;CBaseCombatWeapon*weapon;bool alive;CBaseCombatCharacter():ammo(0),weapon(0),alive(true){}
 int GetAmmoCount(int){return ammo;}void GiveAmmo(int n,int){ammo+=n;}void RemoveAmmo(int n,int){ammo-=min(n,ammo);}
 bool IsAlive(){return alive;}CBaseCombatWeapon*Weapon_OwnsThisType(const char*){return weapon;}
};
struct CBasePlayer:CBaseCombatCharacter {
 int m_nButtons;float m_flNextAttack;bool canUse;CBasePlayer():m_nButtons(0),m_flNextAttack(0),canUse(true){}
 Vector WorldSpaceCenter(){return Vector(10,20,30);}Vector BodyDirection3D(){return Vector(1,0,0);}Vector GetAutoaimVector(int){return Vector(1,0,0);}
 bool Weapon_CanUse(CBaseCombatWeapon*){return canUse;}
};
CBasePlayer*ToBasePlayer(CBaseEntity*entity){return dynamic_cast<CBasePlayer*>(entity);}
struct Rules {bool allow;Rules():allow(true){}bool CanHavePlayerItem(CBasePlayer*,CBaseCombatWeapon*){return allow;}}rules;Rules*g_pGameRules=&rules;
struct Physics {int launches;Vector velocity;Physics():launches(0){}void AddVelocity(Vector*v,AngularImpulse*){++launches;velocity=*v;}};
struct CGrenade_Brickbat:CBaseEntity {
 Physics*physics;Vector velocity;float damage;CBaseCombatCharacter*owner;CGrenade_Brickbat():physics(0),damage(0),owner(0){}
 Physics*VPhysicsGetObject(){return physics;}void SetAbsVelocity(const Vector&v){velocity=v;}void SetLocalAngularVelocity(const QAngle&){}
 void SetOwner(CBaseCombatCharacter*p){owner=p;}void SetOwnerEntity(CBaseEntity*){}void SetDamage(float v){damage=v;}
};
typedef Physics IPhysicsObject;
struct Random {float RandomFloat(float a,float b){assert(a<=b);return a;}}randomObject;Random*random=&randomObject;
struct ConVar{float GetFloat(){return 10;}}sk_plr_dmg_brickbat;
struct WeaponBase:CBaseCombatWeapon {
 bool holster;int defaults,drops;WeaponBase():holster(true),defaults(0),drops(0){}
 bool Holster(CBaseCombatWeapon*){return holster;}void Drop(const Vector&){++drops;}void DefaultTouch(CBaseEntity*){++defaults;}
 void Equip(CBaseCombatCharacter*){}void Spawn(){}
};
struct CWeaponBrickbat:WeaponBase {
 typedef WeaponBase BaseClass;CBaseCombatCharacter*owner;int m_iCurrentAmmoType,m_nAmmoCount[3],m_iPrimaryAmmoType;
 bool m_bNeedThrow,m_bNeedDraw,blocked;float m_flNextPrimaryAttack,m_flNextSecondaryAttack;int animations,draws,idles;
 CWeaponBrickbat():owner(0),m_iCurrentAmmoType(0),m_iPrimaryAmmoType(0),m_bNeedThrow(false),m_bNeedDraw(false),blocked(false),m_flNextPrimaryAttack(0),m_flNextSecondaryAttack(0),animations(0),draws(0),idles(0){m_nAmmoCount[0]=m_nAmmoCount[1]=m_nAmmoCount[2]=0;}
 int ValidAmmoType()const{return m_iCurrentAmmoType>=0&&m_iCurrentAmmoType<3?m_iCurrentAmmoType:0;}
 CBaseCombatCharacter*GetOwner(){return owner;}const char*GetClassname(){return "weapon_brickbat";}
 void SendWeaponAnim(int){++animations;}float SequenceDuration(){return .5f;}bool ObjectInWay(){return blocked;}
 void DrawAmmo(){++draws;m_bNeedDraw=false;}void WeaponIdle(){++idles;}
 void Spawn(void);bool HasPrimaryAmmo(void);void SynchronizeAmmo(void);void BrickbatTouch(CBaseEntity*);
 bool ThrowBrickbat(Vector,Vector,float);void PrimaryAttack(void);void Throw(void);void SecondaryAttack(void);void ItemPostFrame(void);
 bool Holster(CBaseCombatWeapon*);void Drop(const Vector&);
};
'''
SCENARIOS=r'''
int main(){
 gpGlobals->curtime=10;
 CBasePlayer player;CWeaponBrickbat weapon;weapon.m_iCurrentAmmoType=999;weapon.Spawn();
 assert(weapon.m_iCurrentAmmoType==0&&weapon.m_nAmmoCount[0]==1&&weapon.HasPrimaryAmmo());
 weapon.owner=&player;weapon.m_bNeedDraw=false;weapon.SynchronizeAmmo();assert(player.ammo==1);
 // Pullback completes before the single projectile is created.
 weapon.PrimaryAttack();assert(weapon.m_bNeedThrow&&weapon.animations==1&&CBaseEntity::creates==0);
 gpGlobals->curtime=10.25f;weapon.ItemPostFrame();assert(CBaseEntity::creates==0&&player.ammo==1);
 // Allocation failure consumes neither typed ammo nor aggregate HUD reserve.
 gpGlobals->curtime=10.5f;weapon.ItemPostFrame();
 assert(CBaseEntity::creates==1&&weapon.m_nAmmoCount[0]==1&&player.ammo==1&&!weapon.m_bNeedThrow&&weapon.m_bNeedDraw);
 CGrenade_Brickbat projectile;CBaseEntity::result=&projectile;
 gpGlobals->curtime=11;weapon.m_bNeedDraw=false;weapon.PrimaryAttack();
 gpGlobals->curtime=11.5f;weapon.ItemPostFrame();
 assert(weapon.m_nAmmoCount[0]==0&&player.ammo==0&&projectile.velocity.x==800&&projectile.damage==10&&projectile.owner==&player);
 assert(!weapon.m_bNeedThrow&&weapon.m_bNeedDraw&&!weapon.HasPrimaryAmmo());
 // The same method uses physical launch velocity when a body exists.
 weapon.m_nAmmoCount[2]=1;weapon.m_iCurrentAmmoType=2;Physics physics;projectile.physics=&physics;
 assert(weapon.ThrowBrickbat(Vector(),Vector(3,4,5),7)&&physics.launches==1&&physics.velocity.z==5&&projectile.damage==7);
 assert(!weapon.ThrowBrickbat(Vector(),Vector(),7));
 projectile.removed=true;weapon.m_nAmmoCount[2]=1;
 assert(!weapon.ThrowBrickbat(Vector(),Vector(),7)&&weapon.m_nAmmoCount[2]==1);projectile.removed=false;
 // Duplicate pickups transfer each type only up to the original 5/3/1 limits.
 weapon.m_nAmmoCount[0]=4;weapon.m_nAmmoCount[1]=3;player.weapon=&weapon;
 CWeaponBrickbat pickup;pickup.m_nAmmoCount[0]=2;pickup.m_nAmmoCount[1]=1;pickup.m_nAmmoCount[2]=1;
 pickup.BrickbatTouch(&player);assert(weapon.m_nAmmoCount[0]==5&&weapon.m_nAmmoCount[1]==3&&weapon.m_nAmmoCount[2]==1);
 assert(pickup.m_nAmmoCount[0]==1&&pickup.m_nAmmoCount[1]==1&&pickup.m_nAmmoCount[2]==1&&!pickup.removed&&player.ammo==9);
 weapon.m_nAmmoCount[0]=weapon.m_nAmmoCount[1]=weapon.m_nAmmoCount[2]=0;
 pickup.BrickbatTouch(&player);assert(pickup.removed&&weapon.m_nAmmoCount[0]==1&&weapon.m_nAmmoCount[1]==1&&weapon.m_nAmmoCount[2]==1&&player.ammo==3);
 CWeaponBrickbat denied;denied.m_nAmmoCount[0]=1;player.canUse=false;denied.BrickbatTouch(&player);assert(denied.m_nAmmoCount[0]==1&&!denied.removed);
 player.canUse=true;player.weapon=0;denied.BrickbatTouch(&player);assert(denied.defaults==1);
 // Holstering/drop cancels queued throws; failed holster leaves it intact.
 weapon.m_bNeedThrow=true;weapon.holster=false;assert(!weapon.Holster(0)&&weapon.m_bNeedThrow);
 weapon.holster=true;assert(weapon.Holster(0)&&!weapon.m_bNeedThrow);
 weapon.m_bNeedThrow=true;weapon.Drop(Vector());assert(!weapon.m_bNeedThrow&&weapon.drops==1);
 weapon.m_bNeedThrow=true;player.alive=false;weapon.ItemPostFrame();assert(!weapon.m_bNeedThrow);
 weapon.owner=0;weapon.PrimaryAttack();assert(!weapon.m_bNeedThrow);
 return 0;
}
'''

class BrickbatRestorationTests(unittest.TestCase):
    def test_actual_typed_pickup_throw_and_timing(self):
        cc=shutil.which('clang++') or shutil.which('g++')
        if not cc:self.skipTest('no C++ compiler')
        source=(ROOT/'dlls/hl2_dll/weapon_brickbat.cpp').read_bytes().decode('latin1').replace('\r\n','\n')
        signatures=['void CWeaponBrickbat::Spawn( void )','bool CWeaponBrickbat::HasPrimaryAmmo(void)','void CWeaponBrickbat::SynchronizeAmmo(void)','void CWeaponBrickbat::BrickbatTouch( CBaseEntity *pOther )','bool CWeaponBrickbat::ThrowBrickbat( Vector vecSrc, Vector vecVelocity, float damage)','void CWeaponBrickbat::PrimaryAttack( void )','void CWeaponBrickbat::Throw( void )','void CWeaponBrickbat::SecondaryAttack( void )','void CWeaponBrickbat::ItemPostFrame( void )','bool CWeaponBrickbat::Holster(CBaseCombatWeapon *switchingTo)','void CWeaponBrickbat::Drop(const Vector &velocity)']
        with tempfile.TemporaryDirectory() as td:
            cpp=Path(td)/'brickbat.cpp';exe=Path(td)/'brickbat.exe'
            cpp.write_text(HARNESS+'\n'.join(method(source,s) for s in signatures)+SCENARIOS)
            result=subprocess.run([cc,'-std=c++98','-Wall','-Wextra','-Werror','-pedantic',str(cpp),'-o',str(exe)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            result=subprocess.run([str(exe)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)

    def test_registered_built_and_replay_is_idempotent(self):
        import sys
        sys.path.insert(0,str(ROOT/'modernize'))
        import restore_brickbat as patch
        for filename,change in [('weapon_brickbat.cpp',patch.weapon),('weapon_brickbat.h',patch.header),('grenade_brickbat.cpp',patch.grenade),('grenade_brickbat.h',patch.grenade_header)]:
            source=(ROOT/'dlls/hl2_dll'/filename).read_bytes().decode('latin1').replace('\r\n','\n')
            self.assertEqual(change(source),source,filename)
        manifest=json.loads((ROOT/'modernize/weapon_impulse101_manifest.json').read_text())
        hl2=next(row for row in manifest if row['branch']=='hl2')
        self.assertEqual(hl2['compiled_impulse101_extra'].count('weapon_brickbat'),1)
        self.assertNotIn('weapon_brickbat',hl2['excluded_not_compiled'])
        self.assertIn('restore_brickbat.py',(ROOT/'modernize/apply_modernization.py').read_text())

if __name__=='__main__':unittest.main()
