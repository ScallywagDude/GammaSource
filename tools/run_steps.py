"""Run selected steps of the repo's Round 9 plan with the same environment as the driver."""
import sys, subprocess
from pathlib import Path
root = Path(r'C:\Users\doge6\Videos\GammaSource-main')
sys.path.insert(0, str(root/'modernize'))
import os
import build_win32_round9 as b

config, logdir, wanted = sys.argv[1], Path(sys.argv[2]), [int(x) for x in sys.argv[3].split(',')]
logdir.mkdir(parents=True, exist_ok=True)
steps = [s for s in b.plan(config, root) if s['step'] in wanted]
env = b.normalized_environment(os.environ)
_, msbuild, devcmd = b.find_visual_studio(env)
env = b.build_environment(devcmd, env)
print(b.select_shader_tools(env), 'D3DX9=', env.get('HL2_MODERN_D3DX9'), flush=True)
failed = []
for s in steps:
    log = logdir/(f"{s['step']:02d}_"+Path(s['project']).stem+'.log')
    with log.open('w', encoding='utf-8', errors='replace') as fh:
        rc = subprocess.run(b.msbuild_command(msbuild, s), cwd=root, env=env, stdout=fh, stderr=subprocess.STDOUT).returncode
    ok = rc == 0 and b.clean_msbuild_summary(log.read_text(encoding='utf-8', errors='replace'))
    print(f"[{s['step']}] {s['project']} [{s['configuration']}] {'PASS' if ok else 'FAIL'}", flush=True)
    if not ok:
        failed.append(s['step'])
print('FAILED:', failed, flush=True)
sys.exit(1 if failed else 0)
