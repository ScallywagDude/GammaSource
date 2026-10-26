#!/usr/bin/env python3
"""GammaSource: compile the small set of GammaSource HLSL shaders with fxc.exe and
write them as VCS v6 files (uncompressed blocks), plus the combo .inc headers the
C++ shaders include. Stand-in for the Valve shadercompile/vmpi pipeline, which is
not part of this tree.

usage: gamma_build_shaders.py <fxc.exe> <output shaders dir> [shader names...]
"""
import itertools, os, re, struct, subprocess, sys, tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
SHADERS = [  # (fxc source, output name, shader model, stock)
	('gamma_predator_vs20.fxc', 'gamma_predator_vs20', 'vs_2_0', False),
	('gamma_predator_ps20.fxc', 'gamma_predator_ps20', 'ps_2_0', False),
	('gamma_predator_ps20.fxc', 'gamma_predator_ps20b', 'ps_2_b', False),
	# (fxc source, name, model, True) entries rebuild a stock shader and keep the tree's .inc;
	# note GMod's stock .vcs files already match this engine, rebuilding them is not needed
]


def parse_combos(src, target):
	# target is 'vs20', 'ps20' or 'ps20b'; [XBOX] lines and lines for another target are skipped
	statics, dynamics = [], []
	for m in re.finditer(r'//\s*(STATIC|DYNAMIC):\s*"(\w+)"\s*"(\d+)\.\.(\d+)"([^\n]*)', src):
		tags = re.findall(r'\[(\w+)\]', m.group(5))
		if 'XBOX' in tags or 'X360' in tags:
			continue
		targets = [x for x in tags if x in ('vs20', 'vs11', 'ps20', 'ps20b', 'ps11', 'ps30', 'vs30')]
		if targets and target not in targets:
			continue
		(statics if m.group(1) == 'STATIC' else dynamics).append((m.group(2), int(m.group(3)), int(m.group(4))))
	return statics, dynamics


def write_inc(path, name, statics, dynamics):
	ndyn = 1
	for _, lo, hi in dynamics:
		ndyn *= hi - lo + 1
	out = ['#include "shaderlib/cshader.h"']
	for kind, combos, base in (('Static', statics, ndyn), ('Dynamic', dynamics, 1)):
		out.append('class %s_%s_Index\n{' % (name, kind))
		for var, lo, hi in combos:
			out.append('private:\n\tint m_n%s;\npublic:\n\tvoid Set%s( int i ) { Assert( i >= %d && i <= %d ); m_n%s = i; }\n'
					   '\tvoid Set%s( bool i ) { m_n%s = i ? 1 : 0; }' % (var, var, lo, hi, var, var, var))
		ctor = ' '.join('m_n%s = 0;' % v for v, _, _ in combos)
		out.append('public:\n\t%s_%s_Index() { %s }' % (name, kind, ctor))
		terms, mul = [], base
		for var, lo, hi in combos:
			terms.append('( %d * m_n%s )' % (mul, var))
			mul *= hi - lo + 1
		out.append('\tint GetIndex() { return %s; }\n};' % (' + '.join(terms + ['0'])))
		test = ' + '.join(['%s_forgot_to_set_%s_%s' % ('psh' if '_ps' in name else 'vsh', kind.lower(), v) for v, _, _ in combos] + ['0'])
		out.append('#define shader%sTest_%s %s' % (kind, name, test))
	Path(path).write_text('\n'.join(out) + '\n')


def compile_combo(fxc, src, model, defines, tmp):
	obj = Path(tmp) / 'out.o'
	cmd = [fxc, '/nologo', '/T', model, '/E', 'main', '/O3', '/I', str(HERE), '/Fo', str(obj)]
	for k, v in defines.items():
		cmd += ['/D', '%s=%s' % (k, v)]
	cmd.append(str(HERE / src))
	r = subprocess.run(cmd, capture_output=True, text=True)
	if r.returncode:
		return None	# an invalid (SKIP) combination: left out of the .vcs
	return obj.read_bytes()


def build(fxc, outdir):
	outdir = Path(outdir) / 'fxc'
	outdir.mkdir(parents=True, exist_ok=True)
	incdir = HERE / 'fxctmp9'
	only = set(sys.argv[3:])
	for src, name, model, stock in SHADERS:
		if only and name not in only:
			continue
		text = (HERE / src).read_text()
		statics, dynamics = parse_combos(text, {'vs_2_0': 'vs20', 'ps_2_0': 'ps20', 'ps_2_b': 'ps20b'}[model])
		ndyn = 1
		for _, lo, hi in dynamics:
			ndyn *= hi - lo + 1
		smdef = {'vs_2_0': 'SHADER_MODEL_VS_2_0', 'ps_2_0': 'SHADER_MODEL_PS_2_0', 'ps_2_b': 'SHADER_MODEL_PS_2_B'}[model]
		static_sets = list(itertools.product(*[range(lo, hi + 1) for _, lo, hi in statics])) or [()]
		dyn_sets = list(itertools.product(*[range(lo, hi + 1) for _, lo, hi in dynamics])) or [()]

		def index(values, combos):
			idx, mul = 0, 1
			for (var, lo, hi), val in zip(combos, values):
				idx += (val - lo) * mul
				mul *= hi - lo + 1
			return idx

		sections = []
		with tempfile.TemporaryDirectory() as tmp:
			for sv in static_sets:
				block = bytearray()
				for dv in dyn_sets:
					defs = {smdef: 1}
					defs.update({v: x for (v, _, _), x in zip(statics, sv)})
					defs.update({v: x for (v, _, _), x in zip(dynamics, dv)})
					code = compile_combo(fxc, src, model, defs, tmp)
					if code is None:
						continue
					block += struct.pack('<II', index(dv, dynamics), len(code)) + code
				if not block:
					continue
				sections.append((index(sv, statics), bytes(block)))
		nstat = len(sections)
		header_size = 28 + 8 * (nstat + 1) + 4
		data = bytearray()
		records = []
		for sid, block in sections:
			records.append((sid, header_size + len(data)))
			data += struct.pack('<I', 0x80000000 | len(block)) + block + struct.pack('<I', 0xffffffff)
		records.append((0xffffffff, header_size + len(data)))
		hdr = struct.pack('<iiiIIII', 6, len(static_sets) * ndyn, ndyn, 0, 0, nstat + 1, 0)
		vcs = hdr + b''.join(struct.pack('<II', a, b) for a, b in records) + struct.pack('<i', 0) + data
		(outdir / (name + '.vcs')).write_bytes(vcs)
		if not stock:
			write_inc(incdir / (name + '.inc'), name, statics, dynamics)
		print('%s: %d static x %d dynamic combos' % (name, nstat, ndyn))


if __name__ == '__main__':
	build(sys.argv[1], sys.argv[2])
