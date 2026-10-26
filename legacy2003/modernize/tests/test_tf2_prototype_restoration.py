"""Run the restored TF2 plasma/guidance/mortar methods with controlled inputs."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from modernize.tests.test_stickylauncher_spawn import method

ROOT=Path(__file__).resolve().parents[2]
HARNESS=r'''
#include <cassert>
#include <math.h>
#include <string.h>
extern "C" double strtod(const char*,char**);
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#define MAX_CHARGED_TIME 2.0f
#define MAX_CHARGED_POWER 3.0f
struct Vector{
 float x,y,z;Vector(float a=0,float b=0,float c=0):x(a),y(b),z(c){}
 Vector operator+(const Vector&v)const{return Vector(x+v.x,y+v.y,z+v.z);}
 Vector operator-(const Vector&v)const{return Vector(x-v.x,y-v.y,z-v.z);}
 Vector operator*(float f)const{return Vector(x*f,y*f,z*f);}
};
typedef Vector QAngle;const Vector vec3_origin;
float VectorNormalize(Vector&v){float n=(float)sqrt(v.x*v.x+v.y*v.y+v.z*v.z);if(n>0)v=v*(1/n);return n;}
void VectorTransform(const Vector&local,const Vector&origin,Vector&out){out=local+origin;}
void AngleVectors(const QAngle&,Vector*f,Vector*r,Vector*){if(f)*f=Vector(1,0,0);if(r)*r=Vector(0,1,0);}
float AngleNormalize(float v){while(v>180)v-=360;while(v< -180)v+=360;return v;}
float RemapVal(float v,float a,float b,float c,float d){return c+(v-a)*(d-c)/(b-a);}
struct Globals{float curtime,frametime;}globals;Globals*gpGlobals=&globals;
template<class T>struct Handle{T*p;Handle():p(0){}Handle&operator=(T*v){p=v;return *this;}operator T*()const{return p;}T*Get()const{return p;}};
struct CBaseEntity{
 bool alive,sameTeam;Vector origin,velocity;CBaseEntity():alive(true),sameTeam(false){}
 virtual ~CBaseEntity(){}bool IsAlive(){return alive;}bool InSameTeam(CBaseEntity*){return sameTeam;}
 Vector GetAbsOrigin(){return origin;}Vector GetAbsVelocity(){return velocity;}Vector EntityToWorldTransform(){return origin;}
};
struct CBaseCombatWeapon:CBaseEntity{};
struct CBaseTFPlayer:CBaseEntity{
 CBaseCombatWeapon*active;struct{QAngle m_vecPunchAngle;}m_Local;
 CBaseTFPlayer():active(0){}CBaseCombatWeapon*GetActiveWeapon(){return active;}
 Vector Weapon_ShootPosition(){return vec3_origin;}void EyeVectors(Vector*f){*f=Vector(1,0,0);}QAngle EyeAngles(){return vec3_origin;}
};
CBaseTFPlayer*ToBaseTFPlayer(CBaseTFPlayer*p){return p;}
struct SharedFlight{
 float expiry,spawn;Vector direction;int inits;
 SharedFlight():expiry(123),spawn(0),inits(0){}
 float GetDeathTime(){return expiry;}void SetDeathTime(float v){expiry=v;}void SetSpawnTime(float v){spawn=v;}
 void Init(const Vector&,const Vector&d,float){direction=d;expiry=0;++inits;}
};
struct PoweredProjectile:CBaseEntity{
 SharedFlight m_Shared;CBaseEntity*owner;int steps;float power,damage;
 PoweredProjectile():owner(0),steps(0),power(1),damage(10){}
 CBaseEntity*GetOwnerEntity(){return owner;}void SetPower(float v){power=v;}void SetDamage(float v){damage=v;}
 void PerformCustomPhysics(Vector*,Vector*,QAngle*,QAngle*){++steps;}
};
struct CVar{float GetFloat(){return 10;}}weapon_combat_chargeableplasma_damage;
struct CGuidedPlasma:PoweredProjectile{
 typedef PoweredProjectile BaseClass;Handle<CBaseEntity>m_hGuidanceTarget;Vector m_vecGuidanceOffset;
 static CGuidedPlasma*result;static int creates;
 static CGuidedPlasma*Create(const Vector&,const Vector&,CBaseEntity*,const Vector&,CBaseEntity*){++creates;return result;}
 void SetPowerLevel(float);void PerformCustomPhysics(Vector*,Vector*,QAngle*,QAngle*);
};
CGuidedPlasma*CGuidedPlasma::result=0;int CGuidedPlasma::creates=0;
struct WeaponBase:CBaseCombatWeapon{
 bool holster;WeaponBase():holster(true){}bool Holster(CBaseCombatWeapon*){return holster;}
};
enum{SINGLE=1};
struct CWeaponCombat_ChargeablePlasma:WeaponBase{
 typedef WeaponBase BaseClass;CBaseTFPlayer*owner;Handle<CBaseEntity>m_hLockTarget;CBaseEntity*target;
 bool emp,m_bHasCharge,m_bCharging;int m_iClip1,sounds,animations,m_iBurstShotsRemaining;
 float m_flLockedAt,m_flChargeStartTime,m_flPower,m_flNextPrimaryAttack;Vector m_vecTargetOffset;
 CWeaponCombat_ChargeablePlasma():owner(0),target(0),emp(false),m_bHasCharge(true),m_bCharging(false),m_iClip1(3),sounds(0),animations(0),m_iBurstShotsRemaining(2),m_flLockedAt(0),m_flChargeStartTime(0),m_flPower(0),m_flNextPrimaryAttack(0){}
 CBaseTFPlayer*GetOwner(){return owner;}bool IsOwnerEMPed(){return emp;}void WeaponSound(int){++sounds;}
 void PlayAttackAnimation(int){++animations;}int GetPrimaryAttackActivity(){return 0;}
 CBaseEntity*GetLockTarget(){return target;}float GetFireRate(){return .5f;}
 void PrimaryAttack(void);bool Holster(CBaseCombatWeapon*);
};
struct CObjectMortar{
 CBaseTFPlayer*builder;bool reloading;CObjectMortar():builder(0),reloading(false){}
 CBaseTFPlayer*GetBuilder(){return builder;}bool IsReloading(){return reloading;}
};
struct CWeaponMortar:CBaseCombatWeapon{
 CBaseTFPlayer*owner;CObjectMortar*m_hDeployedMortar;float m_flNextPrimaryAttack,yaw,power,accuracy;int fires;
 CWeaponMortar():owner(0),m_hDeployedMortar(0),m_flNextPrimaryAttack(0),yaw(0),power(0),accuracy(0),fires(0){}
 CBaseTFPlayer*GetOwner(){return owner;}void SetYaw(float v){yaw=v;}
 void Fire(float p,float a){power=p;accuracy=a;++fires;m_flNextPrimaryAttack=gpGlobals->curtime+.5f;}
 bool FireControlled(float,float,float);
};
struct Engine{
 int argc;const char*argv[4];int Cmd_Argc(){return argc;}const char*Cmd_Argv(int i){return argv[i];}
}engineObject;
Engine*engine=&engineObject;
int Q_stricmp(const char*a,const char*b){return strcmp(a,b);}
'''

SCENARIOS=r'''
int main(){
 gpGlobals->curtime=10;gpGlobals->frametime=.1f;
 CBaseTFPlayer player,enemy;CWeaponCombat_ChargeablePlasma weapon;weapon.owner=&player;
 weapon.PrimaryAttack();assert(weapon.m_iClip1==3&&weapon.sounds==0&&weapon.animations==0&&CGuidedPlasma::creates==1);
 CGuidedPlasma plasma;CGuidedPlasma::result=&plasma;weapon.PrimaryAttack();
 assert(weapon.m_iClip1==2&&weapon.sounds==1&&weapon.animations==1&&plasma.damage==30&&plasma.power==3);
 weapon.emp=true;weapon.PrimaryAttack();assert(weapon.m_iClip1==2);weapon.emp=false;
 weapon.m_iClip1=0;weapon.PrimaryAttack();assert(weapon.m_iClip1==0);
 weapon.m_bCharging=true;weapon.holster=false;assert(!weapon.Holster(0)&&weapon.m_bCharging);
 weapon.holster=true;assert(weapon.Holster(0)&&!weapon.m_bCharging&&weapon.m_iBurstShotsRemaining==0&&!weapon.m_hLockTarget);
 plasma.owner=&player;plasma.velocity=Vector(1000,0,0);enemy.origin=Vector(100,100,0);plasma.m_hGuidanceTarget=&enemy;
 Vector position,velocity;QAngle angles,angular;plasma.PerformCustomPhysics(&position,&velocity,&angles,&angular);
 assert(plasma.steps==1&&plasma.m_Shared.inits==1&&plasma.m_Shared.direction.y>0);
 assert(plasma.m_Shared.expiry==123&&plasma.m_Shared.spawn==10);
 enemy.sameTeam=true;plasma.PerformCustomPhysics(&position,&velocity,&angles,&angular);assert(plasma.m_Shared.inits==1&&plasma.steps==2);
 enemy.sameTeam=false;enemy.alive=false;plasma.PerformCustomPhysics(&position,&velocity,&angles,&angular);assert(plasma.m_Shared.inits==1);
 plasma.m_hGuidanceTarget=0;plasma.PerformCustomPhysics(&position,&velocity,&angles,&angular);assert(plasma.steps==4);

 CWeaponMortar mortar;CObjectMortar object;mortar.owner=&player;mortar.m_hDeployedMortar=&object;object.builder=&player;player.active=&mortar;
 assert(!mortar.FireControlled(2,0,0)&&!mortar.FireControlled(.5f,.3f,0)&&!mortar.FireControlled(.5f,0,40000));
 object.reloading=true;assert(!mortar.FireControlled(.5f,0,0));object.reloading=false;
 object.builder=&enemy;assert(!mortar.FireControlled(.5f,0,0));object.builder=&player;
 assert(mortar.FireControlled(.5f,.1f,450)&&mortar.fires==1&&mortar.yaw==90);
 assert(!mortar.FireControlled(.5f,0,0)&&mortar.fires==1);
 gpGlobals->curtime=11;engine->argc=4;engine->argv[0]="mortar";engine->argv[1]="0.7";engine->argv[2]="-0.2";engine->argv[3]="-90";
 assert(!HL2ModernMortarClientCommand(&player,"other"));
 assert(HL2ModernMortarClientCommand(&player,"mortar")&&mortar.fires==2&&mortar.power==.7f&&mortar.yaw==-90);
 gpGlobals->curtime=12;engine->argv[1]="nan";assert(HL2ModernMortarClientCommand(&player,"mortar")&&mortar.fires==2);
 engine->argv[1]="0.5junk";assert(HL2ModernMortarClientCommand(&player,"mortar")&&mortar.fires==2);
 engine->argc=3;assert(HL2ModernMortarClientCommand(&player,"mortar")&&mortar.fires==2);
 return 0;
}
'''


class TF2PrototypeRestorationTests(unittest.TestCase):
    def test_actual_plasma_guidance_and_mortar_commands(self):
        compiler=shutil.which('clang++') or shutil.which('g++')
        if not compiler:self.skipTest('C++ compiler unavailable in PATH')
        sources={name:(ROOT/'game_shared/tf2'/name).read_bytes().decode('latin1').replace('\r\n','\n') for name in ('weapon_combat_chargeableplasma.cpp','tf_guidedplasma.cpp','weapon_mortar.cpp')}
        signatures=[
            ('weapon_combat_chargeableplasma.cpp','void CWeaponCombat_ChargeablePlasma::PrimaryAttack( void )'),
            ('weapon_combat_chargeableplasma.cpp','bool CWeaponCombat_ChargeablePlasma::Holster( CBaseCombatWeapon *pSwitchingTo )'),
            ('tf_guidedplasma.cpp','void CGuidedPlasma::SetPowerLevel(float power)'),
            ('tf_guidedplasma.cpp','void CGuidedPlasma::PerformCustomPhysics(Vector *position,Vector *velocity,QAngle *angles,QAngle *angularVelocity)'),
            ('weapon_mortar.cpp','bool CWeaponMortar::FireControlled(float power,float accuracy,float yaw)'),
            ('weapon_mortar.cpp','bool HL2ModernMortarClientCommand(CBaseTFPlayer *player,const char *command)'),
        ]
        with tempfile.TemporaryDirectory() as td:
            cpp=Path(td)/'prototypes.cpp';exe=Path(td)/'prototypes.exe'
            cpp.write_text(HARNESS+'\n'.join(method(sources[file],sig) for file,sig in signatures)+SCENARIOS)
            result=subprocess.run([compiler,'-std=c++98','-Wall','-Wextra','-Werror','-pedantic',str(cpp),'-o',str(exe)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            result=subprocess.run([str(exe)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)

    def test_tf2_project_isolation_and_replay(self):
        import sys
        sys.path.insert(0,str(ROOT/'modernize'))
        from weapon_build_audit import compiled_sources,windows_path
        for config in ('Debug','Release'):
            for name in ('weapon_combat_chargeableplasma','weapon_infiltrator','weapon_laserdesignator','weapon_laserrifle','weapon_mortar','weapon_pistols','tf_guidedplasma'):
                source=windows_path(ROOT/'game_shared/tf2'/(name+'.cpp'))
                self.assertIn(source,compiled_sources(ROOT/'dlls/hl.vcxproj',config+' TF2'))
                for branch in ('HL2','HL1','CounterStrike'):
                    self.assertNotIn(source,compiled_sources(ROOT/'dlls/hl.vcxproj',config+' '+branch))
        self.assertIn('HL2ModernMortarClientCommand(this,cmd)',(ROOT/'dlls/tf2_dll/tf_player.cpp').read_bytes().decode('latin1'))
        self.assertIn('restore_tf2_prototype_weapons.py',(ROOT/'modernize/apply_modernization.py').read_text())

    def test_designator_reapplication_and_shared_range_definition(self):
        import sys
        sys.path.insert(0,str(ROOT/'modernize'))
        import restore_tf2_prototype_weapons as patch
        source=(ROOT/'game_shared/tf2/weapon_laserdesignator.cpp').read_bytes().decode('latin1').replace('\r\n','\n')
        self.assertEqual(patch.designator(source),source)
        self.assertIn('extern ConVar weapon_laserdesignator_range;',source)
        self.assertEqual(source.count('if (!m_pBeam) { m_bDesignating=false; return; }'),1)
        self.assertIn('void Drop(const Vector &velocity) { StopDesignating();',source)


if __name__=='__main__':unittest.main()
