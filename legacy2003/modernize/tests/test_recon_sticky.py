"""Execute original recon sticky throw/fuse/glow code against failure stubs."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from modernize.tests.test_stickylauncher_spawn import method
ROOT=Path(__file__).resolve().parents[2]
HARNESS=r'''
#include <cassert>
#include <stddef.h>
#define max(a,b) ((a)>(b)?(a):(b))
struct Vector{float x,y,z;Vector(float a=0,float b=0,float c=0):x(a),y(b),z(c){}Vector operator+(const Vector&v)const{return Vector(x+v.x,y+v.y,z+v.z);}Vector operator*(float f)const{return Vector(x*f,y*f,z*f);}};
typedef Vector QAngle;const Vector vec3_origin;
void VectorAngles(const Vector&,QAngle&a){a=QAngle(0,90,0);}
struct Globals{float curtime;}globals;Globals*gpGlobals=&globals;
enum{MASK_SOLID=1,COLLISION_GROUP_NONE=0,ACT_VM_SECONDARYATTACK=3,IN_ATTACK=1,MOVETYPE_FLYGRAVITY=4,MOVECOLLIDE_FLY_BOUNCE=5,SOLID_BBOX=6,TFCOLLISION_GROUP_WEAPON=7,kRenderGlow=8,kRenderFxNoDissipation=9,MOVETYPE_NONE=0};
struct CBaseEntity {
 bool marked,bsp;static CBaseEntity*result;static int creates;static Vector spawn;
 CBaseEntity():marked(false),bsp(false){}virtual ~CBaseEntity(){}
 static CBaseEntity*Create(const char*,const Vector&p,const QAngle&,CBaseEntity*){++creates;spawn=p;return result;}
 bool IsMarkedForDeletion(){return marked;}bool IsBSPModel(){return bsp;}
};
CBaseEntity*CBaseEntity::result=0;int CBaseEntity::creates=0;Vector CBaseEntity::spawn;
void UTIL_Remove(CBaseEntity*p){p->marked=true;}
struct CBasePlayer:CBaseEntity{
 bool alive;int ammo,removes,m_nButtons;CBasePlayer():alive(true),ammo(2),removes(0),m_nButtons(0){}
 bool IsAlive(){return alive;}int GetAmmoCount(int){return ammo;}void RemoveAmmo(int n,int){ammo-=n;++removes;}int GetTeamNumber(){return 4;}
 void EyeVectors(Vector*f){*f=Vector(1,0,0);}Vector Weapon_ShootPosition(){return Vector(10,20,30);}
};
CBasePlayer*ToBasePlayer(CBaseEntity*p){return dynamic_cast<CBasePlayer*>(p);}
struct trace_t{bool startsolid,allsolid;Vector endpos;};bool blocked=false;
void UTIL_TraceHull(const Vector&,const Vector&end,const Vector&,const Vector&,int,CBasePlayer*,int,trace_t*t){t->startsolid=blocked;t->allsolid=false;t->endpos=end;}
void UTIL_SetSize(CBaseEntity*,const Vector&,const Vector&){}
template<class T>struct Handle{T*p;Handle():p(0){}Handle&operator=(T*v){p=v;return *this;}operator T*()const{return p;}T*operator->()const{return p;}};
struct CSprite:CBaseEntity{
 static CSprite*result;int attachments;CSprite():attachments(0){}
 static CSprite*SpriteCreate(const char*,const Vector&,bool){return result;}
 void SetTransparency(int,int,int,int,int,int){}void SetBrightness(int){}void SetScale(float){}void SetAttachment(CBaseEntity*,int){++attachments;}
};CSprite*CSprite::result=0;
struct CBaseGrenade:CBaseEntity{
 CBaseEntity*owner,*ownerEntity;Vector velocity;int team,move,bounces,explodes,removals,precaches;bool thinking;float next,damage,radius;
 CBaseGrenade():owner(0),ownerEntity(0),team(0),move(0),bounces(0),explodes(0),removals(0),precaches(0),thinking(false),next(0),damage(0),radius(0){}
 void SetOwner(CBaseEntity*p){owner=p;}void SetOwnerEntity(CBaseEntity*p){ownerEntity=p;}CBaseEntity*GetOwnerEntity(){return ownerEntity;}
 void SetAbsVelocity(const Vector&v){velocity=v;}void ChangeTeam(int t){team=t;}void SetMoveType(int m,int=0){move=m;}
 void SetSolid(int){}void SetGravity(float){}void SetFriction(float){}void SetModel(const char*){}void SetCollisionGroup(int){}
 template<class T>void SetTouch(T){}template<class T>void SetThink(T){thinking=true;}void SetNextThink(float t){next=t;}
 Vector GetLocalOrigin(){return Vector(1,2,3);}void SetDamage(float v){damage=v;}void SetDamageRadius(float v){radius=v;}
 void BounceSound(){++bounces;}void Detonate(){}void Explode(trace_t*,int){++explodes;}void UpdateOnRemove(){++removals;}void Precache(){++precaches;}
};
struct Engine{void PrecacheModel(const char*){}}engineObject;Engine*engine=&engineObject;
struct ConVar{float GetFloat(){return 100;}}grenade_stickybomb_damage,grenade_stickybomb_radius;
struct CGrenadeStickyBomb:CBaseGrenade{
 typedef CBaseGrenade BaseClass;typedef CGrenadeStickyBomb ThisClass;Handle<CSprite>m_pLiveSprite;
 void Precache(void);void Spawn(void);void SetTimer(float);void StickyTouch(CBaseEntity*);void Explode(trace_t*,int);void UpdateOnRemove(void);
};
struct AmmoDef{int Index(const char*){return 3;}}ammoDef;AmmoDef*GetAmmoDef(){return &ammoDef;}
int otherPrecaches=0;void UTIL_PrecacheOther(const char*){++otherPrecaches;}
struct WeaponBase:CBaseEntity{int frames,precaches;WeaponBase():frames(0),precaches(0){}void ItemPostFrame(){++frames;}void Precache(){++precaches;}};
struct CWeaponPistols:WeaponBase{
 typedef WeaponBase BaseClass;CBaseEntity*owner;bool emp;int m_iSecondaryAmmoType,animations;float m_flNextSecondaryAttack,m_flNextPrimaryAttack,m_flSoonestPrimaryAttack;
 CWeaponPistols():owner(0),emp(false),m_iSecondaryAmmoType(-1),animations(0),m_flNextSecondaryAttack(0),m_flNextPrimaryAttack(0),m_flSoonestPrimaryAttack(0){}
 CBaseEntity*GetOwner(){return owner;}bool IsOwnerEMPed(){return emp;}void PlayAttackAnimation(int){++animations;}
 void Precache(void);void SecondaryAttack(void);void ItemPostFrame(void);
};
'''
SCENARIOS=r'''
int main(){
 gpGlobals->curtime=10;CBasePlayer player;CWeaponPistols weapon;weapon.owner=&player;weapon.Precache();
 assert(weapon.m_iSecondaryAmmoType==3&&weapon.precaches==1&&otherPrecaches==1);
 weapon.SecondaryAttack();assert(CBaseEntity::creates==1&&player.ammo==2&&weapon.animations==0&&weapon.m_flNextSecondaryAttack>10);
 gpGlobals->curtime=11;CGrenadeStickyBomb grenade;CBaseEntity::result=&grenade;grenade.marked=true;
 weapon.SecondaryAttack();assert(player.ammo==2&&weapon.animations==0);grenade.marked=false;
 gpGlobals->curtime=12;blocked=true;weapon.SecondaryAttack();assert(CBaseEntity::creates==2&&player.ammo==2);blocked=false;
 gpGlobals->curtime=13;weapon.SecondaryAttack();assert(CBaseEntity::creates==3&&player.ammo==1&&player.removes==1&&weapon.animations==1);
 assert(grenade.owner==&player&&grenade.ownerEntity==&player&&grenade.team==4&&grenade.velocity.x==1200&&CBaseEntity::spawn.x==26);
 assert(weapon.m_flNextSecondaryAttack==13.5f&&weapon.m_flNextPrimaryAttack==13.5f);
 weapon.SecondaryAttack();assert(player.ammo==1&&CBaseEntity::creates==3);
 // Releasing primary must retain the secondary's shared firing lockout.
 weapon.ItemPostFrame();assert(weapon.m_flNextPrimaryAttack==13.5f&&weapon.frames==1);
 gpGlobals->curtime=14;weapon.emp=true;weapon.SecondaryAttack();weapon.emp=false;player.alive=false;weapon.SecondaryAttack();player.alive=true;
 player.ammo=0;weapon.SecondaryAttack();weapon.owner=0;weapon.SecondaryAttack();assert(CBaseEntity::creates==3);
 // Original red glow is optional; the two-second fuse survives visual allocation failure.
 grenade.Spawn();assert(grenade.thinking&&grenade.next==16&&grenade.damage==100&&grenade.radius==100&&!grenade.m_pLiveSprite);
 CSprite glow;CSprite::result=&glow;CGrenadeStickyBomb shown;shown.Spawn();assert(glow.attachments==1&&shown.m_pLiveSprite);
 CBaseEntity wall,actor;wall.bsp=true;shown.ownerEntity=&actor;shown.velocity=Vector(50,0,0);shown.StickyTouch(0);shown.StickyTouch(&actor);assert(shown.velocity.x==50);
 shown.StickyTouch(&wall);assert(shown.velocity.x==0&&shown.move==MOVETYPE_NONE&&shown.bounces==1);
 shown.UpdateOnRemove();assert(glow.marked&&!shown.m_pLiveSprite&&shown.removals==1);
 CSprite glow2;CSprite::result=&glow2;CGrenadeStickyBomb exploding;exploding.Spawn();trace_t trace;exploding.Explode(&trace,0);exploding.UpdateOnRemove();
 assert(glow2.marked&&!exploding.m_pLiveSprite&&exploding.explodes==1&&exploding.removals==1);
 return 0;
}
'''

class ReconStickyTests(unittest.TestCase):
    def test_actual_secondary_fuse_and_visual_lifecycle(self):
        cc=shutil.which('clang++') or shutil.which('g++')
        if not cc:self.skipTest('no C++ compiler')
        weapon=(ROOT/'game_shared/tf2/weapon_pistols.cpp').read_bytes().decode('latin1').replace('\r\n','\n')
        grenade=(ROOT/'game_shared/tf2/grenade_stickybomb.cpp').read_bytes().decode('latin1').replace('\r\n','\n')
        definitions=[method(weapon,s) for s in ['void CWeaponPistols::Precache(void)','void CWeaponPistols::SecondaryAttack( void )','void CWeaponPistols::ItemPostFrame( void )']]
        definitions += [method(grenade,s) for s in ['void CGrenadeStickyBomb::Precache( void )','void CGrenadeStickyBomb::Spawn( void )','void CGrenadeStickyBomb::SetTimer( float timer )','void CGrenadeStickyBomb::StickyTouch( CBaseEntity *pOther )','void CGrenadeStickyBomb::Explode( trace_t *pTrace, int bitsDamageType )','void CGrenadeStickyBomb::UpdateOnRemove(void)']]
        with tempfile.TemporaryDirectory() as td:
            cpp=Path(td)/'recon.cpp';exe=Path(td)/'recon.exe';cpp.write_text(HARNESS+'\n'.join(definitions)+SCENARIOS)
            result=subprocess.run([cc,'-std=c++98','-Wall','-Wextra','-Werror','-pedantic',str(cpp),'-o',str(exe)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            result=subprocess.run([str(exe)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)

    def test_replay_and_saved_handles(self):
        import sys
        sys.path.insert(0,str(ROOT/'modernize'))
        import restore_tf2_prototype_weapons as patch
        for file,change in [('weapon_pistols.cpp',patch.pistols),('grenade_stickybomb.cpp',patch.sticky_grenade)]:
            source=(ROOT/'game_shared/tf2'/file).read_bytes().decode('latin1').replace('\r\n','\n')
            self.assertEqual(change(source),source)
        self.assertIn('DEFINE_FIELD(CGrenadeStickyBomb,m_pLiveSprite,FIELD_EHANDLE)',source)

if __name__=='__main__':unittest.main()
