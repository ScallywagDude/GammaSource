"""Static checks for the opt-in capability probe (not a renderer test)."""
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[2]
class GpuProbeTests(unittest.TestCase):
    def test_api_diagnostics_are_not_renderer_claims(self):
        probe=(ROOT/'modernize/gpu_api_probe.cpp').read_text()
        for api in ('D3D11CreateDevice','D3D12CreateDevice','vkEnumerateInstanceVersion',
                    'wglCreateContextAttribsARB','WGL_CONTEXT_CORE_PROFILE_BIT_ARB',
                    'wglMakeCurrent(0,0)','wglDeleteContext(modern)'):
            self.assertIn(api,probe)
        self.assertIn('This only probes system APIs',probe)
        self.assertIn('OpenGL 3.3+ core context: yes',probe)
        self.assertIn('OpenGL 3.3+ core context: unsupported',probe)
        self.assertIn('wglMakeCurrent(dc,modern)',probe)
        self.assertIn('IsRealWglExtension',probe)
        # No dynamic SDK download/build dependencies are introduced.
        self.assertNotIn('vulkan/vulkan.h',probe)
