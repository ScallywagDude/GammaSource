"""Compile actual hopwire methods against controlled entity/player substitutes."""
import importlib.util
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from modernize.tests.test_stickylauncher_spawn import method

ROOT = Path(__file__).resolve().parents[2]
PATCH = ROOT / "modernize/patch_hopwire_spawn.py"
WEAPON = ROOT / "dlls/hl2_dll/weapon_hopwire.cpp"
GRENADE = ROOT / "dlls/hl2_dll/grenade_hopwire.cpp"

HARNESS = r'''
#include <cassert>
#include <math.h>
#include <string.h>
#include <stddef.h>
struct Vector {
    float x,y,z;
    Vector(float a=0,float b=0,float c=0): x(a),y(b),z(c) {}
    Vector operator+(const Vector &v) const { return Vector(x+v.x,y+v.y,z+v.z); }
    Vector operator-(const Vector &v) const { return Vector(x-v.x,y-v.y,z-v.z); }
    Vector operator*(float v) const { return Vector(x*v,y*v,z*v); }
    Vector &operator+=(const Vector &v) { x+=v.x;y+=v.y;z+=v.z;return *this; }
    float &operator[](int n) { return n==0 ? x : (n==1 ? y : z); }
};
typedef Vector QAngle;
typedef Vector AngularImpulse;
const QAngle vec3_angle;
const float GRENADE_TIMER=2.0f;
enum { EVENT_WEAPON_THROW=1,EVENT_WEAPON_THROW2,EVENT_WEAPON_THROW3,
       MASK_PLAYERSOLID=4,COLLISION_GROUP_NONE=0 };
struct animevent_t { int event; };
struct CBaseEntity {
    static CBaseEntity *result;
    static int creates;
    static Vector position, angles;
    static CBaseEntity *creator;
    bool character,marked;
    CBaseEntity(): character(false),marked(false) {}
    bool IsMarkedForDeletion() { return marked; }
    static CBaseEntity *Create(const char *name,const Vector &p,const QAngle &a,CBaseEntity *owner) {
        assert(strcmp(name,"npc_grenade_hopwire")==0);
        ++creates;position=p;angles=a;creator=owner;return result;
    }
};
CBaseEntity *CBaseEntity::result=0;
int CBaseEntity::creates=0;
Vector CBaseEntity::position,CBaseEntity::angles;
CBaseEntity *CBaseEntity::creator=0;
struct CBaseCombatCharacter: CBaseEntity {
    int ammo,removals,drops;
    bool player;
    CBaseCombatCharacter(): ammo(3),removals(0),drops(0),player(false) { character=true; }
    int GetAmmoCount(int) { return ammo; }
    void RemoveAmmo(int n,int) { ammo-=n;++removals; }
    void Weapon_Drop(void *) { ++drops; }
};
struct CBasePlayer: CBaseCombatCharacter {
    CBasePlayer() { player=true; }
    Vector EyePosition() { return Vector(100,200,300); }
    void EyeVectors(Vector *f,Vector *r,Vector *) { *f=Vector(1,0,0);*r=Vector(0,1,0); }
    Vector GetAbsVelocity() { return Vector(10,20,30); }
    Vector WorldSpaceCenter() { return Vector(100,200,264); }
    Vector GetAbsMins() { return Vector(84,184,232); }
    Vector BodyDirection2D() { return Vector(1,0,0); }
    QAngle GetLocalAngles() { return QAngle(0,75,0); }
};
CBasePlayer *ToBasePlayer(CBaseCombatCharacter *owner) {
    return owner && owner->player ? static_cast<CBasePlayer *>(owner) : 0;
}
CBaseCombatCharacter *ToBaseCombatCharacter(CBaseEntity *owner) {
    return owner && owner->character ? static_cast<CBaseCombatCharacter *>(owner) : 0;
}
struct CBaseGrenade: CBaseEntity {};
struct CGrenadeHopWire: CBaseGrenade {
    int timers,velocities,owners;
    float timer;
    Vector velocity,rotation;
    CBaseCombatCharacter *owner;
    CGrenadeHopWire(): timers(0),velocities(0),owners(0),timer(0),owner(0) {}
    void SetTimer(float v) { ++timers;timer=v; }
    void SetVelocity(const Vector &v,const AngularImpulse &r) { ++velocities;velocity=v;rotation=r; }
    void SetOwner(CBaseCombatCharacter *v) { ++owners;owner=v; }
};
struct TestRandom { int RandomInt(int low,int high) { return (low+high)/2; } } fakeRandom;
#define random (&fakeRandom)
void VectorNormalize(Vector &v) { float n=(float)sqrt(v.x*v.x+v.y*v.y+v.z*v.z);v=v*(1/n); }
void CrossProduct(const Vector &a,const Vector &b,Vector &v) {
    v=Vector(a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x);
}
struct trace_t { float fraction;struct { Vector normal; } plane; };
float traceFraction=1.0f;
void UTIL_TraceLine(const Vector &,const Vector &,int,CBasePlayer *,int,trace_t *tr) {
    tr->fraction=traceFraction;tr->plane.normal=Vector(0,0,1);
}
struct WeaponBase {
    int forwarded;
    WeaponBase(): forwarded(0) {}
    void Operator_HandleAnimEvent(animevent_t *,CBaseCombatCharacter *) { ++forwarded; }
};
struct CWeaponHopWire: WeaponBase {
    typedef WeaponBase BaseClass;
    CBaseCombatCharacter *owner;
    int m_iPrimaryAmmoType,sounds,removed;
    bool m_bRedraw;
    CWeaponHopWire(): owner(0),m_iPrimaryAmmoType(1),sounds(0),removed(0),m_bRedraw(false) {}
    CBaseCombatCharacter *GetOwner() { return owner; }
    void EmitSound(const char *) { ++sounds; }
    void Operator_HandleAnimEvent(animevent_t *,CBaseCombatCharacter *);
    void DecrementAmmo(CBaseCombatCharacter *);
    bool ThrowGrenade(CBasePlayer *);
    bool LobGrenade(CBasePlayer *);
    bool RollGrenade(CBasePlayer *);
};
void UTIL_Remove(CWeaponHopWire *weapon) { ++weapon->removed; }
CBaseGrenade *HopWire_Create(const Vector &,const QAngle &,const Vector &,const AngularImpulse &,CBaseEntity *,float);
'''

SCENARIOS = r'''
int main() {
    for (int event=EVENT_WEAPON_THROW;event<=EVENT_WEAPON_THROW3;++event) {
        animevent_t animation={event};
        CBasePlayer player;
        CWeaponHopWire weapon;
        weapon.owner=&player;
        int before=CBaseEntity::creates;
        CBaseEntity::result=0;
        // Failed creation never dereferences the absent grenade, spends ammo,
        // drops/removes the weapon, requests redraw or plays a throw sound.
        weapon.Operator_HandleAnimEvent(&animation,&player);
        assert(CBaseEntity::creates==before+1);
        assert(player.ammo==3 && player.removals==0 && player.drops==0);
        assert(!weapon.m_bRedraw && weapon.sounds==0 && weapon.removed==0);

        CGrenadeHopWire grenade;
        CBaseEntity::result=&grenade;
        // Spawn can return an entity already rejected by its physics setup.
        grenade.marked=true;
        weapon.Operator_HandleAnimEvent(&animation,&player);
        assert(player.ammo==3 && player.removals==0 && player.drops==0);
        assert(!weapon.m_bRedraw && weapon.sounds==0 && grenade.timers==0 && grenade.velocities==0);
        grenade.marked=false;
        ++before;
        weapon.Operator_HandleAnimEvent(&animation,&player);
        assert(CBaseEntity::creates==before+2 && CBaseEntity::creator==&player);
        assert(player.ammo==2 && player.removals==1 && player.drops==0);
        assert(weapon.m_bRedraw && weapon.removed==0);
        assert(grenade.timers==1 && grenade.timer==2 && grenade.velocities==1);
        assert(grenade.owners==1 && grenade.owner==&player);
        if (event==EVENT_WEAPON_THROW) {
            assert(weapon.sounds==0 && grenade.velocity.x==1210 && grenade.velocity.z==150);
            assert(CBaseEntity::position.z==300 && grenade.rotation.x==600);
        } else if (event==EVENT_WEAPON_THROW2) {
            assert(weapon.sounds==1 && grenade.velocity.x==510 && grenade.velocity.z==30);
            assert(CBaseEntity::position.z==236 && CBaseEntity::angles.y==75 && CBaseEntity::angles.z==-90);
            assert(grenade.rotation.z==720);
            // Floor projection follows the same successful rolling path.
            traceFraction=0.5f;
            assert(weapon.RollGrenade(&player));
            assert(grenade.velocity.x==510 && grenade.velocity.z==30);
            traceFraction=1.0f;
        } else {
            assert(weapon.sounds==1 && grenade.velocity.x==360 && grenade.velocity.z==80);
            assert(CBaseEntity::position.z==292 && grenade.rotation.x==200);
        }

        // The final successful grenade preserves the original drop/removal behavior.
        player.ammo=1;
        weapon.Operator_HandleAnimEvent(&animation,&player);
        assert(player.ammo==0 && player.removals==2 && player.drops==1 && weapon.removed==1);
        before=CBaseEntity::creates;
        weapon.Operator_HandleAnimEvent(&animation,&player);
        player.ammo=-1;
        weapon.Operator_HandleAnimEvent(&animation,&player);
        assert(CBaseEntity::creates==before && player.removals==2);

        // Animation events can outlive ownership or arrive for an NPC.
        weapon.owner=0;
        weapon.Operator_HandleAnimEvent(&animation,&player);
        CBaseCombatCharacter npc;
        weapon.owner=&npc;
        weapon.Operator_HandleAnimEvent(&animation,&npc);
        assert(CBaseEntity::creates==before && npc.removals==0);
        animevent_t unrelated={999};
        weapon.Operator_HandleAnimEvent(&unrelated,&npc);
        assert(weapon.forwarded==1 && CBaseEntity::creates==before);
    }
    return 0;
}
'''


class HopwireSpawnTests(unittest.TestCase):
    def test_overlay_is_idempotent_and_preserves_source_bytes(self):
        spec = importlib.util.spec_from_file_location("hopwire_patch", PATCH)
        patch = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(patch)
        with tempfile.TemporaryDirectory() as td:
            patch.ROOT = Path(td)
            originals = {}
            for relative in (patch.WEAPON, patch.GRENADE):
                original = (ROOT / relative).read_bytes()
                target = patch.ROOT / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(original)
                originals[target] = original
            patch.main()
            patch.main()
            self.assertEqual(originals, {path: path.read_bytes() for path in originals})
        pipeline = (ROOT / "modernize/apply_modernization.py").read_text()
        self.assertIn("patch_hopwire_spawn.py", pipeline)

    def test_exact_animation_throw_roll_lob_and_factory_methods(self):
        compiler = shutil.which("clang++") or shutil.which("g++")
        if not compiler:
            self.skipTest("C++ compiler unavailable in PATH")
        weapon = WEAPON.read_bytes().decode("latin1").replace("\r\n", "\n")
        grenade = GRENADE.read_bytes().decode("latin1").replace("\r\n", "\n")
        methods = [method(weapon, signature) for signature in (
            "void CWeaponHopWire::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )",
            "void CWeaponHopWire::DecrementAmmo( CBaseCombatCharacter *pOwner )",
            "bool CWeaponHopWire::ThrowGrenade( CBasePlayer *pPlayer )",
            "bool CWeaponHopWire::RollGrenade( CBasePlayer *pPlayer )",
            "bool CWeaponHopWire::LobGrenade( CBasePlayer *pPlayer )",
        )]
        methods.append(method(grenade, "CBaseGrenade *HopWire_Create( const Vector &position, const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, CBaseEntity *pOwner, float timer )"))
        with tempfile.TemporaryDirectory() as td:
            cpp = Path(td) / "hopwire.cpp"
            executable = Path(td) / "hopwire.exe"
            cpp.write_text(HARNESS + "\n".join(methods) + SCENARIOS)
            built = subprocess.run([compiler, "-std=c++98", "-Wall", "-Wextra", "-Werror", "-pedantic",
                                    str(cpp), "-o", str(executable)], capture_output=True, text=True)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            ran = subprocess.run([str(executable)], capture_output=True, text=True)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)


if __name__ == "__main__":
    unittest.main()
