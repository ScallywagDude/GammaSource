"""Execute the actual repair-gun target method against controlled vehicles.

This verifies target selection in isolation, not networking or a linked game.
"""
import importlib.util
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "game_shared/tf2/weapon_repairgun.cpp"
PATCH = ROOT / "modernize/patch_repairgun_targets.py"


def method(source):
    signature = "CBaseEntity *CWeaponRepairGun::CheckVehicleTargets( CBaseEntity *pCurHealing )"
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
#include <stddef.h>
struct CBaseEntity {
    int health, maximum;
    CBaseEntity(): health(100), maximum(100) {}
    virtual ~CBaseEntity() {}
    virtual bool IsPlayer() const { return false; }
    int GetHealth() const { return health; }
    int GetMaxHealth() const { return maximum; }
};
struct VehicleSeat {
    CBaseEntity *entity;
    explicit VehicleSeat(CBaseEntity *value): entity(value) {}
    CBaseEntity *GetVehicleEnt() { return entity; }
};
struct CBaseTFPlayer: CBaseEntity {
    VehicleSeat *seat;
    CBaseTFPlayer(): seat(0) {}
    virtual bool IsPlayer() const { return true; }
    bool IsInAVehicle() const { return seat!=0; }
    VehicleSeat *GetVehicle() { return seat; }
};
struct CBaseTFVehicle: CBaseEntity {
    CBaseTFPlayer *passengers[4];
    int count;
    CBaseTFVehicle(): count(4) {
        for (int i=0;i<4;++i) passengers[i]=0;
    }
    int GetMaxPassengerCount() const { return count; }
    CBaseEntity *GetPassenger(int index) { return passengers[index]; }
};
struct CWeaponRepairGun {
    bool targetsPlayers;
    CWeaponRepairGun(): targetsPlayers(true) {}
    bool TargetsPlayers() const { return targetsPlayers; }
    CBaseEntity *CheckVehicleTargets(CBaseEntity *);
};
'''

SCENARIOS = r'''
int main() {
    CWeaponRepairGun weapon;
    CBaseTFVehicle vehicle;
    CBaseTFPlayer healthy, injured;
    injured.health=40;
    VehicleSeat seat(&vehicle);
    healthy.seat=&seat;
    injured.seat=&seat;

    // An injured passenger wins in either ordering, including empty seats.
    vehicle.passengers[0]=&healthy;
    vehicle.passengers[2]=&injured;
    assert(weapon.CheckVehicleTargets(&vehicle)==&injured);
    assert(weapon.CheckVehicleTargets(&healthy)==&injured);
    vehicle.passengers[0]=&injured;
    vehicle.passengers[2]=&healthy;
    assert(weapon.CheckVehicleTargets(&vehicle)==&injured);
    assert(weapon.CheckVehicleTargets(&healthy)==&injured);

    // A damaged vehicle retains priority when the current passenger is well.
    vehicle.health=30;
    assert(weapon.CheckVehicleTargets(&vehicle)==&vehicle);
    assert(weapon.CheckVehicleTargets(&healthy)==&vehicle);
    // An already injured passenger remains the current target.
    assert(weapon.CheckVehicleTargets(&injured)==&injured);

    vehicle.health=100;
    vehicle.passengers[0]=0;
    assert(weapon.CheckVehicleTargets(&vehicle)==&healthy);
    vehicle.passengers[2]=0;
    assert(weapon.CheckVehicleTargets(&vehicle)==&vehicle);
    vehicle.count=0;
    assert(weapon.CheckVehicleTargets(&vehicle)==&vehicle);

    CBaseTFPlayer pedestrian;
    assert(weapon.CheckVehicleTargets(&pedestrian)==&pedestrian);
    pedestrian.health=20;
    assert(weapon.CheckVehicleTargets(&pedestrian)==&pedestrian);
    CBaseEntity ordinary;
    assert(weapon.CheckVehicleTargets(&ordinary)==&ordinary);

    // A weapon that cannot target players must not change its target.
    vehicle.count=4;
    vehicle.passengers[0]=&injured;
    weapon.targetsPlayers=false;
    assert(weapon.CheckVehicleTargets(&vehicle)==&vehicle);
    assert(weapon.CheckVehicleTargets(&healthy)==&healthy);
    return 0;
}
'''


class RepairGunTargetTests(unittest.TestCase):
    def test_patch_repairs_original_and_preserves_encoding_and_line_endings(self):
        spec = importlib.util.spec_from_file_location("repairgun_patch", PATCH)
        patch = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(patch)
        source = SOURCE.read_bytes().decode("latin1").replace("\r\n", "\n")
        self.assertEqual(source.count(patch.AFTER), 1)
        original = source.replace(patch.AFTER, patch.BEFORE, 1)
        self.assertEqual(patch.restore(original), source)
        self.assertEqual(patch.restore(source), source)
        with tempfile.TemporaryDirectory() as td:
            patch.ROOT = Path(td)
            target = patch.ROOT / patch.SOURCE
            target.parent.mkdir(parents=True)
            for newline in ("\n", "\r\n"):
                sentinel = "\n// synthetic encoding sentinel: \xff\n"
                target.write_bytes((original + sentinel).replace("\n", newline).encode("latin1"))
                expected = (source + sentinel).replace("\n", newline).encode("latin1")
                patch.main()
                self.assertEqual(target.read_bytes(), expected)
                patch.main()
                self.assertEqual(target.read_bytes(), expected)
        pipeline = (ROOT / "modernize/apply_modernization.py").read_text()
        self.assertIn("patch_repairgun_targets.py", pipeline)

    def test_exact_source_passenger_and_vehicle_selection(self):
        compiler = shutil.which("clang++") or shutil.which("g++")
        if not compiler:
            self.skipTest("C++ compiler unavailable in PATH")
        source = SOURCE.read_bytes().decode("latin1").replace("\r\n", "\n")
        with tempfile.TemporaryDirectory() as td:
            cpp = Path(td) / "repairgun.cpp"
            executable = Path(td) / "repairgun.exe"
            cpp.write_text(HARNESS + method(source) + SCENARIOS)
            built = subprocess.run([compiler, "-std=c++98", "-Wall", "-Wextra", "-Werror", "-pedantic",
                                    str(cpp), "-o", str(executable)], capture_output=True, text=True)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            ran = subprocess.run([str(executable)], capture_output=True, text=True)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)


if __name__ == "__main__":
    unittest.main()
