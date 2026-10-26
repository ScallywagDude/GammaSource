"""Exercise actual restored remote-manhack input, camera and cleanup methods."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from modernize.tests.test_stickylauncher_spawn import method

ROOT=Path(__file__).resolve().parents[2]
ENTITY=ROOT/'dlls/hl2_dll/player_manhack.cpp'
WEAPON=ROOT/'dlls/hl2_dll/weapon_manhack.cpp'

HARNESS=r'''
#include <cassert>
#include <math.h>
#include <stddef.h>
#include <limits.h>
template<class T>T min(T a,T b){return a<b?a:b;}
template<class T>T max(T a,T b){return a>b?a:b;}
struct Vector{
 float x,y,z;Vector(float a=0,float b=0,float c=0):x(a),y(b),z(c){}
 Vector operator+(const Vector &v)const{return Vector(x+v.x,y+v.y,z+v.z);}
 Vector operator-(const Vector &v)const{return Vector(x-v.x,y-v.y,z-v.z);}
 Vector operator*(float f)const{return Vector(x*f,y*f,z*f);}
 Vector &operator*=(float f){x*=f;y*=f;z*=f;return *this;}
 float Length()const{return (float)sqrt(x*x+y*y+z*z);}
 float &operator[](int n){return n==0?x:(n==1?y:z);}
};
typedef Vector QAngle;
const Vector vec3_origin;const QAngle vec3_angle;
void VectorNormalize(Vector &v){float n=v.Length();if(n>0)v*=1/n;}
void AngleVectors(const QAngle&,Vector *f,Vector *r=0,Vector *u=0){if(f)*f=Vector(1,0,0);if(r)*r=Vector(0,1,0);if(u)*u=Vector(0,0,1);}
enum{IN_ATTACK=1,IN_ATTACK2=2,IN_JUMP=4,IN_DUCK=8,EF_NODRAW=32,MOVETYPE_NONE=0,MOVETYPE_WALK=1,MOVECOLLIDE_DEFAULT=0,
 LIFE_ALIVE=0,LIFE_DEAD=1,DAMAGE_NO=0,DAMAGE_YES=1,DMG_SLASH=4,MASK_SHOT=1,MASK_PLAYERSOLID=2,COLLISION_GROUP_NONE=0};
typedef int MoveType_t;typedef int MoveCollide_t;
struct Globals{float curtime;}globals;Globals *gpGlobals=&globals;
template<class T>struct CHandle{
 T*p;CHandle():p(0){}CHandle&operator=(T*v){p=v;return *this;}
 operator T*()const{return p;}T*operator->()const{return p;}T*Get()const{return p;}
};
struct CTakeDamageInfo;
struct CBaseEntity{
 bool marked;Vector origin;int m_takedamage,damages,updates,m_lifeState;
 CBaseEntity():marked(false),m_takedamage(DAMAGE_YES),damages(0),updates(0),m_lifeState(LIFE_ALIVE){}
 virtual ~CBaseEntity(){}
 static CBaseEntity *created;
 static int creates;
 static CBaseEntity*Create(const char*,const Vector&,const QAngle&,CBaseEntity*){++creates;return created;}
 bool IsMarkedForDeletion(){return marked;}
 virtual void UpdateOnRemove(){marked=true;++updates;}
 virtual void OnRestore(){}
 virtual bool IsAlive(){return m_lifeState==LIFE_ALIVE;}
 Vector GetAbsOrigin(){return origin;}
 CBaseEntity *edict(){return this;}
 void TakeDamage(const CTakeDamageInfo&){++damages;}
};
CBaseEntity*CBaseEntity::created=0;int CBaseEntity::creates=0;
void UTIL_Remove(CBaseEntity*e){if(e&&!e->marked)e->UpdateOnRemove();}
struct CBaseCombatCharacter:CBaseEntity{};
struct CBaseViewModel{int m_fEffects;CBaseViewModel():m_fEffects(0){}};
struct CBasePlayer;
struct CBaseCombatWeapon:CBaseEntity{
 CBasePlayer*owner;CBaseCombatWeapon():owner(0){}CBasePlayer*GetOwner(){return owner;}
};
struct CBasePlayer:CBaseCombatCharacter{
 CBaseCombatWeapon *active;int fov,move,collide;bool visible;Vector velocity;QAngle angles;CBaseViewModel viewmodel;
 CBasePlayer():active(0),fov(90),move(MOVETYPE_WALK),collide(0),visible(true){}
 CBaseCombatWeapon*GetActiveWeapon(){return active;}
 int GetFOV(){return fov;}void SetFOV(int v){fov=v;}
 int GetMoveType(){return move;}int GetMoveCollide(){return collide;}
 void SetMoveType(int v,int c=0){move=v;collide=c;}
 CBaseViewModel*GetViewModel(){return &viewmodel;}
 QAngle EyeAngles(){return angles;}void SnapEyeAngles(const QAngle&v){angles=v;}
 void SetAbsVelocity(const Vector &v){velocity=v;}
 void ShowViewModel(bool v){visible=v;}
 void EyeVectors(Vector *f){*f=Vector(1,0,0);}
 Vector EyePosition(){return origin+Vector(0,0,64);}
};
CBasePlayer*ToBasePlayer(CBasePlayer*p){return p;}
CBaseCombatCharacter*ToBaseCombatCharacter(CBaseEntity*p){return dynamic_cast<CBaseCombatCharacter*>(p);}
struct Engine{CBaseEntity *client,*view;int changes;Engine():client(0),view(0),changes(0){}
 void SetView(CBaseEntity*c,CBaseEntity*v){client=c;view=v;++changes;}}engineObject;
Engine*engine=&engineObject;
struct IPhysicsObject{
 Vector velocity,position;int additions;IPhysicsObject():additions(0){}
 void GetVelocity(Vector*v,Vector*){*v=velocity;}void AddVelocity(const Vector*v,Vector*){velocity=velocity+*v;++additions;}
 void GetPosition(Vector*v,QAngle*){*v=position;}void SetPosition(const Vector&p,const QAngle&,bool){position=p;}
};
struct CUserCmd{float forwardmove,sidemove,upmove;int buttons;QAngle viewangles;
 CUserCmd():forwardmove(0),sidemove(0),upmove(0),buttons(0){}
};
struct trace_t{float fraction;bool startsolid,world;CBaseEntity*m_pEnt;Vector endpos;
 trace_t():fraction(1),startsolid(false),world(false),m_pEnt(0){}
 bool DidHitWorld(){return world;}}fakeTrace;
void UTIL_TraceHull(const Vector&,const Vector&,const Vector&,const Vector&,int,CBaseEntity*,int,trace_t*t){*t=fakeTrace;}
void UTIL_DecalTrace(trace_t*,const char*){}
struct CTakeDamageInfo{CBaseEntity*attacker;float damage;CTakeDamageInfo(CBaseEntity*,CBaseEntity*a,float d,int):attacker(a),damage(d){}};
void CalculateMeleeDamageForce(CTakeDamageInfo*,const Vector&,const Vector&){}
struct CPlayer_Manhack:CBaseCombatCharacter{
 typedef CBaseCombatCharacter BaseClass;
 CHandle<CBasePlayer>m_hController;CHandle<CBaseCombatWeapon>m_hControlWeapon;
 bool m_bActive,m_bRestoreView,m_bSavedViewModelVisible;int m_iSavedFOV,m_iSavedMoveType,m_iSavedMoveCollide;
 float m_flExpireTime,m_flLastCommand,m_flNextBladeDamage,next;
 Vector m_vecDesiredVelocity;QAngle m_angCommandView,m_angSavedPlayerView;
 IPhysicsObject physics;bool hasPhysics;int sounds,stops;
 CPlayer_Manhack():m_bActive(false),m_bRestoreView(false),m_bSavedViewModelVisible(false),m_iSavedFOV(0),m_iSavedMoveType(0),m_iSavedMoveCollide(0),m_flExpireTime(0),m_flLastCommand(0),m_flNextBladeDamage(0),next(0),hasPhysics(true),sounds(0),stops(0){}
 IPhysicsObject*VPhysicsGetObject(){return hasPhysics?&physics:0;}
 void EmitSound(const char*){++sounds;}void StopSound(const char*){++stops;}
 void SetThink(void(CPlayer_Manhack::*)()){}void SetNextThink(float f){next=f;}
 static CPlayer_Manhack*FindForPlayer(CBasePlayer*);
 bool IsControlledBy(CBasePlayer*)const;CBaseCombatWeapon*GetControlWeapon(void)const;
 bool StartControl(CBasePlayer*,CBaseCombatWeapon*);void StopControl(void);void PlayerCommand(CUserCmd*);
 void FlyThink(void);void CheckBladeTrace(void);void OnRestore(void);void UpdateOnRemove(void);
 void Event_Killed(const CTakeDamageInfo&);
};
struct EntityList{
 CBaseEntity*entries[8];int count;EntityList():count(0){}
 CBaseEntity*FindEntityByClassname(CBaseEntity*previous,const char*){
  int start=0;if(previous){for(int i=0;i<count;++i)if(entries[i]==previous){start=i+1;break;}}
  return start<count?entries[start]:0;
 }
}gEntList;
struct WeaponBase:CBaseCombatWeapon{
 bool holster;int frames,drops;WeaponBase():holster(true),frames(0),drops(0){}
 void ItemPostFrame(){++frames;}bool Holster(CBaseCombatWeapon*){return holster;}
 void Drop(const Vector&){++drops;}
};
struct CWeapon_Manhack:WeaponBase{
 typedef WeaponBase BaseClass;
 CHandle<CPlayer_Manhack>m_hRemote;float m_flNextPrimaryAttack,m_flNextSecondaryAttack;
 CWeapon_Manhack():m_flNextPrimaryAttack(0),m_flNextSecondaryAttack(0){}
 void PrimaryAttack(void);void SecondaryAttack(void);void StopRemote(void);void ItemPostFrame(void);
 bool Holster(CBaseCombatWeapon*);void Drop(const Vector&);void UpdateOnRemove(void);
};
'''

SCENARIOS=r'''
int main(){
 gpGlobals->curtime=10;
 CBasePlayer player,other;CWeapon_Manhack weapon;weapon.owner=&player;player.active=&weapon;
 CPlayer_Manhack control;gEntList.entries[gEntList.count++]=&control;
 const Vector body=player.origin;
 assert(!control.StartControl(&other,&weapon));
 assert(control.StartControl(&player,&weapon));weapon.m_hRemote=&control;
 assert(player.move==MOVETYPE_NONE&&player.fov==132&&engine->view==&control);
 assert(player.origin.x==body.x&&player.origin.y==body.y&&player.origin.z==body.z);
 assert(!control.StartControl(&player,&weapon));
 CUserCmd command;command.forwardmove=400;command.sidemove=400;command.buttons=IN_ATTACK|IN_ATTACK2|IN_JUMP;
 control.PlayerCommand(&command);
 assert(command.forwardmove==0&&command.sidemove==0&&command.upmove==0);
 assert(command.buttons==(IN_ATTACK|IN_ATTACK2)&&control.m_vecDesiredVelocity.Length()<=400.001f);
 assert(control.m_vecDesiredVelocity.z>0);
 CUserCmd invalid;invalid.forwardmove=100000;invalid.viewangles.x=100000;invalid.buttons=IN_ATTACK|IN_DUCK;
 control.PlayerCommand(&invalid);assert(invalid.forwardmove==0&&invalid.buttons==IN_ATTACK);
 control.physics.velocity=Vector(1000,0,0);control.FlyThink();assert(control.physics.velocity.Length()<=400.001f);
 gpGlobals->curtime=10.5f;control.FlyThink();assert(control.m_vecDesiredVelocity.Length()==0);
 // The original 1-damage blade action is clocked at 0.1 seconds, with owner attribution.
 CBaseCombatCharacter target;fakeTrace.fraction=0.5f;fakeTrace.m_pEnt=&target;
 gpGlobals->curtime=11;control.FlyThink();assert(target.damages==1);
 control.FlyThink();assert(target.damages==1);
 gpGlobals->curtime=11.11f;control.FlyThink();assert(target.damages==2);
 fakeTrace.m_pEnt=&player;control.CheckBladeTrace();assert(player.damages==0);
 control.OnRestore();assert(control.m_bRestoreView);control.FlyThink();assert(!control.m_bRestoreView&&engine->view==&control);
 assert(weapon.Holster(0));assert(!control.m_bActive&&control.marked&&player.move==MOVETYPE_WALK&&player.fov==90&&engine->view==&player);
 int views=engine->changes;control.StopControl();assert(engine->changes==views);

 // Failure/abort gates never leave camera or movement captured.
 gEntList.count=0;
 CPlayer_Manhack dead;gEntList.entries[gEntList.count++]=&dead;
 assert(dead.StartControl(&player,&weapon));
 player.m_lifeState=LIFE_DEAD;player.move=99;dead.FlyThink();
 assert(dead.marked&&player.move==99&&player.fov==0&&engine->view==&player);
 player.m_lifeState=LIFE_ALIVE;player.move=MOVETYPE_WALK;player.fov=90;
 gEntList.count=0;
 CPlayer_Manhack lost;gEntList.entries[gEntList.count++]=&lost;
 assert(lost.StartControl(&player,&weapon));weapon.owner=&other;lost.FlyThink();
 assert(lost.marked&&player.move==MOVETYPE_WALK&&engine->view==&player);weapon.owner=&player;
 gEntList.count=0;
 CPlayer_Manhack distant;gEntList.entries[gEntList.count++]=&distant;
 assert(distant.StartControl(&player,&weapon));distant.origin=Vector(1601,0,0);distant.FlyThink();assert(distant.marked);
 gEntList.count=0;
 CPlayer_Manhack timed;gEntList.entries[gEntList.count++]=&timed;
 assert(timed.StartControl(&player,&weapon));gpGlobals->curtime=timed.m_flExpireTime;timed.FlyThink();assert(timed.marked);

 // Primary spawn failure, blocked clearance, and control failure are reversible.
 gEntList.count=0;gpGlobals->curtime=100;fakeTrace=trace_t();weapon.m_hRemote=0;
 CBaseEntity::created=0;int created=CBaseEntity::creates;weapon.PrimaryAttack();assert(CBaseEntity::creates==created+1);
 fakeTrace.startsolid=true;weapon.PrimaryAttack();assert(CBaseEntity::creates==created+1);fakeTrace.startsolid=false;
 CPlayer_Manhack spawned;CBaseEntity::created=&spawned;gEntList.entries[gEntList.count++]=&spawned;
 weapon.PrimaryAttack();assert(weapon.m_hRemote&&spawned.m_bActive);
 weapon.SecondaryAttack();assert(spawned.marked&&!weapon.m_hRemote&&player.move==MOVETYPE_WALK);
 return 0;
}
'''


class ManhackControlTests(unittest.TestCase):
    def test_actual_owner_input_camera_blades_and_cleanup(self):
        compiler=shutil.which('clang++') or shutil.which('g++')
        if not compiler:self.skipTest('C++ compiler unavailable in PATH')
        entity=ENTITY.read_bytes().decode('latin1').replace('\r\n','\n')
        weapon=WEAPON.read_bytes().decode('latin1').replace('\r\n','\n')
        entity_signatures=[
            'CPlayer_Manhack *CPlayer_Manhack::FindForPlayer(CBasePlayer *player)',
            'bool CPlayer_Manhack::IsControlledBy(CBasePlayer *player) const',
            'CBaseCombatWeapon *CPlayer_Manhack::GetControlWeapon(void) const',
            'bool CPlayer_Manhack::StartControl(CBasePlayer *player,CBaseCombatWeapon *weapon)',
            'void CPlayer_Manhack::StopControl(void)',
            'void CPlayer_Manhack::PlayerCommand(CUserCmd *command)',
            'void CPlayer_Manhack::FlyThink(void)',
            'void CPlayer_Manhack::CheckBladeTrace(void)',
            'void CPlayer_Manhack::OnRestore(void)',
            'void CPlayer_Manhack::UpdateOnRemove(void)',
            'void CPlayer_Manhack::Event_Killed(const CTakeDamageInfo &info)',
        ]
        weapon_signatures=[
            'void CWeapon_Manhack::PrimaryAttack(void)',
            'void CWeapon_Manhack::SecondaryAttack(void)',
            'void CWeapon_Manhack::StopRemote(void)',
            'void CWeapon_Manhack::ItemPostFrame(void)',
            'bool CWeapon_Manhack::Holster(CBaseCombatWeapon *switchingTo)',
            'void CWeapon_Manhack::Drop(const Vector &velocity)',
            'void CWeapon_Manhack::UpdateOnRemove(void)',
        ]
        code=HARNESS+'\n'.join(method(entity,sig) for sig in entity_signatures)+'\n'+'\n'.join(method(weapon,sig) for sig in weapon_signatures)+SCENARIOS
        with tempfile.TemporaryDirectory() as td:
            cpp=Path(td)/'control.cpp';exe=Path(td)/'control.exe';cpp.write_text(code)
            result=subprocess.run([compiler,'-std=c++98','-Wall','-Wextra','-Werror','-pedantic',str(cpp),'-o',str(exe)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            result=subprocess.run([str(exe)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)

    def test_save_network_and_runtime_overlay_contract(self):
        source=ENTITY.read_bytes().decode('latin1')
        for field in ('m_hController','m_hControlWeapon'):
            self.assertIn('CPlayer_Manhack,'+field+',FIELD_EHANDLE',source)
        for field in ('m_flExpireTime','m_flLastCommand','m_flNextBladeDamage'):
            self.assertIn('CPlayer_Manhack,'+field+',FIELD_TIME',source)
        self.assertNotIn('UTIL_PlayerByIndex',source)
        self.assertNotIn('#if 0',source)
        self.assertIn('DT_Player_Manhack',source)
        self.assertIn('STUB_WEAPON_CLASS( player_manhack, Player_Manhack, C_BaseCombatCharacter );',(ROOT/'cl_dll/hl2_hud/c_weapon__stubs_hl2.cpp').read_text())
        self.assertIn('restore_manhack_control.py',(ROOT/'modernize/apply_modernization.py').read_text())
        self.assertIn('HL2ModernManhackPlayerCommand(this, ucmd);',(ROOT/'dlls/player.cpp').read_bytes().decode('latin1'))


if __name__=='__main__':unittest.main()
