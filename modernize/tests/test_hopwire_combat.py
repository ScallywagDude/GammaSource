"""Execute the restored hopwire's actual stage and cleanup methods in isolation."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from modernize.tests.test_stickylauncher_spawn import method

ROOT=Path(__file__).resolve().parents[2]
SOURCE=ROOT/'dlls/hl2_dll/grenade_hopwire.cpp'

HARNESS=r'''
#include <cassert>
#include <stddef.h>
struct Vector {
    float x,y,z;
    Vector(float a=0,float b=0,float c=0):x(a),y(b),z(c){}
    Vector operator*(float f)const{return Vector(x*f,y*f,z*f);}
    float LengthSqr()const{return x*x+y*y+z*z;}
    void Negate(){x=-x;y=-y;z=-z;}
};
typedef Vector QAngle;
typedef Vector AngularImpulse;
const Vector vec3_origin;
Vector RandomVector(float,float){return Vector(1,0,0);}
Vector RandomAngularImpulse(float,float){return vec3_origin;}
void VectorNormalize(Vector &v){v=Vector(1,0,0);}
template<class T> struct CHandle {
    T *p;
    CHandle():p(0){}
    CHandle &operator=(T *v){p=v;return *this;}
    operator T*()const{return p;}
    T*operator->()const{return p;}
    T*Get()const{return p;}
};
struct Globals{float curtime;} globals;
Globals *gpGlobals=&globals;
enum{SOUND_DANGER=1,kRenderTransAdd,kRenderFxNone,MOVETYPE_NONE};
struct CBaseEntity {
    bool marked,solid;
    int updates;
    CBaseEntity():marked(false),solid(true),updates(0){}
    virtual ~CBaseEntity(){}
    virtual void UpdateOnRemove(){marked=true;++updates;}
    bool IsMarkedForDeletion(){return marked;}
    bool IsSolid(){return solid;}
    Vector GetAbsOrigin(){return vec3_origin;}
    QAngle GetAbsAngles(){return vec3_origin;}
    int entindex(){return 1;}
};
void UTIL_Remove(CBaseEntity *e){if(e&&!e->marked)e->UpdateOnRemove();}
struct CSprite:CBaseEntity {
    static CSprite *result;
    static CSprite *SpriteCreate(const char*,const Vector&,bool){return result;}
    void SetParent(CBaseEntity*){}
    void SetTransparency(int,int,int,int,int,int){}
    void SetBrightness(int,float){}
    void SetScale(float,float){}
};
CSprite *CSprite::result=0;
struct CBeam:CBaseEntity {
    static CBeam *result;
    static CBeam *BeamCreate(const char*,float){return result;}
    void EntsInit(CBaseEntity*,CBaseEntity*){}
    void SetNoise(float){}
    void SetColor(int,int,int){}
    void SetScrollRate(int){}
    void SetBrightness(int){}
    void SetWidth(float){}
    void SetEndWidth(float){}
};
CBeam *CBeam::result=0;
struct CRopeKeyframe:CBaseEntity{};
struct IPhysicsObject{} physics;
IPhysicsObject *g_PhysWorldObject=&physics;
struct IPhysicsSpring{} springObject;
struct springparams_t {
    float constant,damping,naturalLength,relativeDamping;
    Vector startPosition,endPosition;
    bool useLocalPositions;
};
struct PhysicsEnvironment {
    bool fail;
    int creates,destroys;
    PhysicsEnvironment():fail(false),creates(0),destroys(0){}
    IPhysicsSpring *CreateSpring(IPhysicsObject*,IPhysicsObject*,springparams_t*){++creates;return fail?0:&springObject;}
    void DestroySpring(IPhysicsSpring*){++destroys;}
} environment;
PhysicsEnvironment *physenv=&environment;
struct CGrenadeHopWire;
struct CTetherHook:CBaseEntity {
    typedef CBaseEntity BaseClass;
    static bool fail;
    static int created;
    CHandle<CGrenadeHopWire> m_hTetheredOwner;
    IPhysicsSpring *m_pSpring;
    CHandle<CRopeKeyframe> m_pRope;
    CHandle<CBeam> m_pBeam;
    CHandle<CSprite> m_pGlow;
    bool m_bAttached;
    float next;
    int physicsDestroyed,ropes,velocities;
    Vector velocity;
    CTetherHook():m_pSpring(0),m_bAttached(false),next(0),physicsDestroyed(0),ropes(0),velocities(0){}
    static CTetherHook *Create(const Vector&,const QAngle&,CGrenadeHopWire *owner){
        if(fail)return 0;
        ++created;CTetherHook *h=new CTetherHook;h->m_hTetheredOwner=owner;return h;
    }
    void CreateRope(){++ropes;}
    void SetVelocity(const Vector &v,const AngularImpulse&){velocity=v;++velocities;}
    void SetMoveType(int){}
    void VPhysicsDestroyObject(){++physicsDestroyed;}
    void EmitSound(const char*){}
    void StopSound(int,const char*){}
    void SetThink(void(CTetherHook::*)()){}
    void SetNextThink(float value){next=value;}
    void StartTouch(CBaseEntity*);
    void HookThink();
    void UpdateOnRemove();
};
bool CTetherHook::fail=false;
int CTetherHook::created=0;
struct GrenadeBase:CBaseEntity {
    int explosions;
    GrenadeBase():explosions(0){}
    void Detonate(){++explosions;}
};
struct CGrenadeHopWire:GrenadeBase {
    typedef GrenadeBase BaseClass;
    int m_nHooksShot,velocities;
    float m_flTetherDeadline,m_flCombatDeadline,next;
    Vector m_vecHookVelocity;
    CHandle<CTetherHook> m_hHooks[8];
    CHandle<CSprite> m_pGlow;
    void(CGrenadeHopWire::*think)();
    bool hasPhysics;
    CGrenadeHopWire():m_nHooksShot(0),velocities(0),m_flTetherDeadline(0),m_flCombatDeadline(0),next(0),think(0),hasPhysics(true){}
    IPhysicsObject *VPhysicsGetObject(){return hasPhysics?&physics:0;}
    Vector WorldSpaceCenter(){return vec3_origin;}
    void SetVelocity(const Vector&,const AngularImpulse&){++velocities;}
    void SetThink(void(CGrenadeHopWire::*value)()){think=value;}
    void SetNextThink(float value){next=value;}
    void Detonate();void TetherThink();void CombatThink();void RemoveTethers();void UpdateOnRemove();
};
struct CSoundEnt{static void InsertSound(int,const Vector&,int,float){}};
'''

SCENARIOS=r'''
int main(){
    gpGlobals->curtime=10;
    CGrenadeHopWire charge;
    charge.Detonate();
    assert(charge.velocities==1&&charge.m_flTetherDeadline==13&&charge.next>10);
    charge.Detonate();assert(charge.velocities==1);
    for(int i=0;i<8;++i){charge.TetherThink();assert(charge.m_nHooksShot==i+1);}
    assert(CTetherHook::created==8&&charge.m_hHooks[0]->velocity.x==1500);
    assert(charge.m_hHooks[1]->velocity.x==-1500);
    charge.TetherThink();
    assert(charge.think==&CGrenadeHopWire::CombatThink&&charge.m_flCombatDeadline==10.75f);
    charge.CombatThink();assert(charge.explosions==0&&charge.next>10);
    // Saving/restoring the stage deadlines resumes the original remaining fuse.
    CGrenadeHopWire restored=charge;
    gpGlobals->curtime=10.75f;
    restored.CombatThink();assert(restored.explosions==1);
    for(int i=0;i<8;++i){assert(!restored.m_hHooks[i]);assert(charge.m_hHooks[i]->marked);delete charge.m_hHooks[i].Get();charge.m_hHooks[i]=0;}

    // Allocation pressure cannot leave a live inert grenade behind.
    CTetherHook::fail=true;gpGlobals->curtime=20;
    CGrenadeHopWire exhausted;exhausted.Detonate();exhausted.TetherThink();
    assert(exhausted.m_nHooksShot==0&&exhausted.next>20);
    gpGlobals->curtime=23;exhausted.TetherThink();
    assert(exhausted.m_flCombatDeadline==23.75f);
    gpGlobals->curtime=23.75f;exhausted.CombatThink();assert(exhausted.explosions==1);

    // Missing owner/physics, failed springs and failed visual allocation are safe.
    CTetherHook orphan;orphan.HookThink();assert(orphan.marked);
    CBaseEntity wall;
    CTetherHook missing;missing.m_hTetheredOwner=&charge;charge.hasPhysics=false;
    missing.StartTouch(&wall);assert(missing.marked&&!missing.m_bAttached);
    charge.hasPhysics=true;
    CTetherHook springFailure;springFailure.m_hTetheredOwner=&charge;
    environment.fail=true;springFailure.StartTouch(&wall);assert(springFailure.marked);
    environment.fail=false;
    CTetherHook hook;hook.m_hTetheredOwner=&charge;hook.StartTouch(&wall);
    assert(hook.m_bAttached&&hook.physicsDestroyed==1&&hook.m_pSpring);
    int springs=environment.creates;hook.StartTouch(&wall);assert(environment.creates==springs);
    hook.HookThink();assert(!hook.m_pBeam&&!hook.m_pGlow&&hook.next>0);
    CBeam beam;CSprite glow;CRopeKeyframe rope;
    CBeam::result=&beam;CSprite::result=&glow;hook.m_pRope=&rope;hook.HookThink();
    assert(hook.m_pBeam&&hook.m_pGlow);
    int destroyed=environment.destroys;hook.UpdateOnRemove();
    assert(environment.destroys==destroyed+1&&beam.marked&&glow.marked&&rope.marked);
    assert(!hook.m_pSpring&&!hook.m_pBeam&&!hook.m_pGlow&&!hook.m_pRope&&hook.updates==1);
    hook.UpdateOnRemove();assert(environment.destroys==destroyed+1);
    return 0;
}
'''


class HopwireCombatTests(unittest.TestCase):
    def test_actual_finite_stages_and_hook_cleanup(self):
        compiler=shutil.which('clang++') or shutil.which('g++')
        if not compiler:self.skipTest('C++ compiler unavailable in PATH')
        source=SOURCE.read_bytes().decode('latin1').replace('\r\n','\n')
        signatures=[
            'void CTetherHook::StartTouch( CBaseEntity *pOther )',
            'void CTetherHook::HookThink( void )',
            'void CTetherHook::UpdateOnRemove( void )',
            'void CGrenadeHopWire::Detonate( void )',
            'void CGrenadeHopWire::TetherThink( void )',
            'void CGrenadeHopWire::CombatThink( void )',
            'void CGrenadeHopWire::RemoveTethers( void )',
            'void CGrenadeHopWire::UpdateOnRemove( void )',
        ]
        with tempfile.TemporaryDirectory() as td:
            cpp=Path(td)/'hopwire.cpp';exe=Path(td)/'hopwire.exe'
            cpp.write_text(HARNESS+'\n'.join(method(source,sig) for sig in signatures)+SCENARIOS)
            result=subprocess.run([compiler,'-std=c++98','-Wall','-Wextra','-Werror','-pedantic',str(cpp),'-o',str(exe)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            result=subprocess.run([str(exe)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)

    def test_saved_handles_and_deadlines_and_overlay_replay(self):
        import sys
        sys.path.insert(0,str(ROOT/'modernize'))
        import restore_hopwire_combat as patch
        for relative,change in (('dlls/hl2_dll/grenade_hopwire.h',patch.header),('dlls/hl2_dll/grenade_hopwire.cpp',patch.restore)):
            source=(ROOT/relative).read_bytes().decode('latin1').replace('\r\n','\n')
            self.assertEqual(change(source),source)
        source=SOURCE.read_bytes().decode('latin1')
        for field in ('m_flTetherDeadline','m_flCombatDeadline'):
            self.assertIn('CGrenadeHopWire, '+field+', FIELD_TIME',source)
        self.assertIn('CGrenadeHopWire, m_hHooks, FIELD_EHANDLE, 8',source)
        self.assertNotIn('TODO: Go boom',source)
        self.assertIn('restore_hopwire_combat.py',(ROOT/'modernize/apply_modernization.py').read_text())


if __name__=='__main__':unittest.main()
