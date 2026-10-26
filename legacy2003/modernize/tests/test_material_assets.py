"""Numerical edge cases repaired in the legacy cheap-water material shader."""
from pathlib import Path
import math
import unittest


class WaterMaterialTests(unittest.TestCase):
    def test_near_eye_normalization_is_finite_in_both_permutations(self):
        for distance in (0.0,1e-9,1e-4,0.01,1.0,128.0):
            nv_distance_squared=max(distance*distance,0.0001)
            nv_normalized=distance/math.sqrt(nv_distance_squared)
            normal_distance=max(distance,0.01)
            normal_normalized=distance/normal_distance
            self.assertTrue(math.isfinite(nv_normalized))
            self.assertAlmostEqual(nv_normalized,normal_normalized)
            if distance>=0.01:self.assertAlmostEqual(normal_normalized,1.0)

    def test_quantized_normal_does_not_make_negative_reflections(self):
        for normal_dot in (-0.01,0.0,0.5,1.0,1.0001,1.5):
            fresnel=(1-min(1,max(0,normal_dot)))**5
            self.assertGreaterEqual(fresnel,0)
            self.assertLessEqual(fresnel,1)

    def test_existing_dx9_shader_has_both_guards(self):
        shader=(Path(__file__).resolve().parents[2]/'materialsystem/stdshaders/WaterCheap_ps20.fxc').read_bytes().decode('latin1')
        self.assertIn('max( dot( worldSpaceEye, worldSpaceEye ), 0.0001f )',shader)
        self.assertIn('max( length( worldSpaceEye ), 0.01f )',shader)
        self.assertIn('dotResult = saturate( dotResult );',shader)


if __name__=='__main__':unittest.main()
