"""Execute the actual sticky launcher methods with a controlled entity factory.

This verifies source behavior in isolation, not a linked game or engine physics.
"""
import importlib.util
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "dlls/hl2_dll/weapon_stickylauncher.cpp"
PATCH = ROOT / "modernize/patch_stickylauncher_spawn.py"


def method(source, signature):
    start = source.index(signature + "\n{")
    opening = source.index("{", start)
    depth = 1
    position = opening + 1
    while depth:
        depth += (source[position] == "{") - (source[position] == "}")
        position += 1
    return source[start:position]


HARNESS = r'''
#include <cassert>
struct Vector {
    float x, y, z;
    Vector(float a=0, float b=0, float c=0): x(a), y(b), z(c) {}
    Vector operator*(float value) const { return Vector(x*value,y*value,z*value); }
};
typedef Vector QAngle;
const Vector vec3_origin;
const QAngle vec3_angle;
enum { SINGLE, SINGLE_NPC, EF_MUZZLEFLASH=4, PLAYER_ATTACK1=5,
       AUTOAIM_5DEGREES=6, EVENT_WEAPON_PISTOL_FIRE=7 };
struct Globals { float curtime; } globals;
Globals *gpGlobals = &globals;
struct animevent_t { int event; };
struct CAI_BaseNPC {
    Vector GetActualShootTrajectory(const Vector &) { return Vector(0,1,0); }
};
struct CBaseEntity {
    static CBaseEntity *result;
    static int creates;
    static CBaseEntity *Create(const char *,const Vector &,const QAngle &,CBaseEntity *) {
        ++creates;
        return result;
    }
};
CBaseEntity *CBaseEntity::result = 0;
int CBaseEntity::creates = 0;
struct CBaseCombatCharacter: CBaseEntity {
    int m_fEffects;
    bool player;
    CAI_BaseNPC *npc;
    CBaseCombatCharacter(): m_fEffects(0), player(false), npc(0) {}
    Vector Weapon_ShootPosition() { return Vector(1,2,3); }
    CAI_BaseNPC *MyNPCPointer() { return npc; }
    QAngle GetLocalAngles() { return vec3_angle; }
};
struct CBasePlayer: CBaseCombatCharacter {
    int animations;
    CBasePlayer(): animations(0) { player=true; }
    Vector GetAutoaimVector(int) { return Vector(1,0,0); }
    void SetAnimation(int) { ++animations; }
};
CBasePlayer *ToBasePlayer(CBaseCombatCharacter *owner) {
    return owner && owner->player ? static_cast<CBasePlayer *>(owner) : 0;
}
void AngleVectors(const QAngle &,Vector *direction) { *direction=Vector(0,0,1); }
struct CStickyBomb: CBaseEntity {
    int velocities;
    Vector velocity;
    CStickyBomb(): velocities(0) {}
    void SetVelocity(const Vector &value,const Vector &) { ++velocities; velocity=value; }
};
struct WeaponBase {
    int forwarded;
    WeaponBase(): forwarded(0) {}
    void Operator_HandleAnimEvent(animevent_t *,CBaseCombatCharacter *) { ++forwarded; }
};
struct CWeaponStickyLauncher: WeaponBase {
    typedef WeaponBase BaseClass;
    CBaseCombatCharacter *owner;
    int m_iClip1, sounds, animations, kicks;
    float m_flNextPrimaryAttack;
    CWeaponStickyLauncher(): owner(0),m_iClip1(3),sounds(0),animations(0),kicks(0),m_flNextPrimaryAttack(0) {}
    CBaseCombatCharacter *GetOwner() { return owner; }
    float GetFireRate() { return 1.0f; }
    void WeaponSound(int) { ++sounds; }
    void SendWeaponAnim(int) { ++animations; }
    int GetPrimaryAttackActivity() { return 0; }
    void AddViewKick() { ++kicks; }
    void PrimaryAttack();
    void Operator_HandleAnimEvent(animevent_t *,CBaseCombatCharacter *);
};
static bool LaunchStickyBomb(CBaseCombatCharacter *,const Vector &,const Vector &);
'''

SCENARIOS = r'''
int main() {
    gpGlobals->curtime=10;
    CBasePlayer player;
    CWeaponStickyLauncher weapon;
    weapon.owner=&player;
    // Exhausted entity factory: no dereference, round, firing effects or recoil.
    weapon.PrimaryAttack();
    assert(CBaseEntity::creates==1);
    assert(weapon.m_iClip1==3 && weapon.sounds==0 && weapon.animations==0 && weapon.kicks==0);
    assert(player.m_fEffects==0 && player.animations==0);
    assert(weapon.m_flNextPrimaryAttack==11);

    CStickyBomb bomb;
    CBaseEntity::result=&bomb;
    weapon.PrimaryAttack();
    assert(CBaseEntity::creates==2 && bomb.velocities==1 && bomb.velocity.x==1200);
    assert(weapon.m_iClip1==2 && weapon.sounds==1 && weapon.animations==1 && weapon.kicks==1);
    assert(player.m_fEffects==EF_MUZZLEFLASH && player.animations==1);

    // Dropped/non-player weapons retain the original player-only attack gate.
    weapon.owner=0;
    weapon.PrimaryAttack();
    CBaseCombatCharacter actor;
    weapon.owner=&actor;
    weapon.PrimaryAttack();
    assert(CBaseEntity::creates==2);

    // NPC animation events must also preserve the clip and firing effects.
    CWeaponStickyLauncher npcWeapon;
    CAI_BaseNPC npc;
    actor.npc=&npc;
    animevent_t fire={EVENT_WEAPON_PISTOL_FIRE};
    CBaseEntity::result=0;
    npcWeapon.Operator_HandleAnimEvent(&fire,&actor);
    assert(CBaseEntity::creates==3 && npcWeapon.m_iClip1==3);
    assert(npcWeapon.sounds==0 && actor.m_fEffects==0);
    CBaseEntity::result=&bomb;
    npcWeapon.Operator_HandleAnimEvent(&fire,&actor);
    assert(CBaseEntity::creates==4 && bomb.velocities==2 && bomb.velocity.y==1200);
    assert(npcWeapon.m_iClip1==2 && npcWeapon.sounds==1 && actor.m_fEffects==EF_MUZZLEFLASH);
    // Non-NPC operators use the original angle-based trajectory.
    actor.npc=0;
    npcWeapon.Operator_HandleAnimEvent(&fire,&actor);
    assert(CBaseEntity::creates==5 && bomb.velocities==3 && bomb.velocity.z==1200);
    assert(npcWeapon.m_iClip1==1);
    animevent_t other={999};
    npcWeapon.Operator_HandleAnimEvent(&other,&actor);
    assert(npcWeapon.forwarded==1 && CBaseEntity::creates==5);
    return 0;
}
'''


class StickyLauncherSpawnTests(unittest.TestCase):
    def test_patch_reapplication_preserves_encoding_and_newlines(self):
        spec = importlib.util.spec_from_file_location("sticky_patch", PATCH)
        patch = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(patch)
        original = SOURCE.read_bytes()
        source = original.decode("latin1").replace("\r\n", "\n")
        self.assertEqual(patch.restore(source), source)
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            copy = root / patch.SOURCE
            copy.parent.mkdir(parents=True)
            copy.write_bytes(original)
            patch.ROOT = root
            patch.main()
            self.assertEqual(copy.read_bytes(), original)
        pipeline = (ROOT / "modernize/apply_modernization.py").read_text()
        self.assertIn("patch_stickylauncher_spawn.py", pipeline)

    def test_actual_player_npc_and_projectile_factory_behavior(self):
        compiler = shutil.which("clang++") or shutil.which("g++")
        if not compiler:
            self.skipTest("C++ compiler unavailable in PATH")
        source = SOURCE.read_bytes().decode("latin1").replace("\r\n", "\n")
        signatures = (
            "void CWeaponStickyLauncher::PrimaryAttack( void )",
            "void CWeaponStickyLauncher::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )",
            "bool LaunchStickyBomb( CBaseCombatCharacter *pOwner, const Vector &origin, const Vector &direction )",
        )
        with tempfile.TemporaryDirectory() as td:
            cpp = Path(td) / "sticky.cpp"
            executable = Path(td) / "sticky.exe"
            cpp.write_text(HARNESS + "\n".join(method(source, sig) for sig in signatures) + SCENARIOS)
            built = subprocess.run([compiler, "-std=c++98", "-Wall", "-Wextra", "-Werror", "-pedantic",
                                    str(cpp), "-o", str(executable)], capture_output=True, text=True)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            ran = subprocess.run([str(executable)], capture_output=True, text=True)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)


if __name__ == "__main__":
    unittest.main()
