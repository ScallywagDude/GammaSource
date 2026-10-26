#!/usr/bin/env python3
"""Generate/review a real-engine weapon exercise; never launches the game."""
from pathlib import Path
import argparse, json, math, re

EXTRAS = ['weapon_flaregun', 'weapon_stunstick']
INSPECT = {'weapon_citizenpackage', 'weapon_citizensuitcase', 'weapon_cubemap'}
WORLD = {'weapon_physgun', 'weapon_physcannon', 'weapon_rollerwand', 'weapon_thumper', 'weapon_tripwire', 'weapon_portalgun', 'weapon_binoculars', 'weapon_immolator'}
LATE = {'weapon_cguard': 1.35, 'weapon_frag': 1.6, 'weapon_hopwire': 1.5,
        'weapon_vortexhopwire': 1.5, 'weapon_bugbait': 1.3, 'weapon_brickbat': 1.5, 'weapon_slam': 1.8}
TAIL = {'weapon_thumper', 'weapon_tripwire', 'weapon_slam', 'weapon_portalgun', 'weapon_manhack'}

def fields(line):
    return dict(re.findall(r'(\w+)=([^ ]*)', line.strip()))

def generate(args):
    game = args.game_dir.resolve()
    if not (game/'gameinfo.txt').exists() or not game.name.startswith('t_'):
        raise SystemExit('--game-dir must name an existing isolated t_* test game, never the player install')
    roster = {}
    for line in args.roster_log.read_text(errors='replace').splitlines():
        if line.startswith('GAMMA_WEAPON name='):
            data = fields(line); roster[data['name']] = data
    if not roster:
        raise SystemExit('No GAMMA_WEAPON roster entries found')
    for name in EXTRAS:
        roster.setdefault(name, {'name':name, 'clip':'1/1' if name.endswith('flaregun') else '-1/-1',
            'ammo':'FlareRound' if name.endswith('flaregun') else '', 'interval':'1.0' if name.endswith('flaregun') else '0.7'})
    if args.only:
        wanted = set(args.only.split(','))
        unknown = wanted-set(roster)
        if unknown: raise SystemExit('Not in roster: '+', '.join(sorted(unknown)))
        roster = {name:data for name,data in roster.items() if name in wanted}
    names = sorted(roster, key=lambda name:(name in TAIL, name=='weapon_manhack', name))
    folder = game/'cfg'/'gamma_weapon_e2e'
    folder.mkdir(parents=True,exist_ok=True)
    def cfg(name, lines):
        (folder/(name+'.cfg')).write_text('\n'.join(line.replace(';', ',') for line in lines)+'\n',encoding='ascii')
    def later(at, command): return f'gamma_delay {at:.3f} {command}'
    main = ['// Disposable isolated test game only. Physical bindings are disabled in this process.',
        'sv_cheats 1', 'developer 1', 'hud_fastswitch 1', 'unbindall', 'joystick 0', 'touch_enable 0',
        'sensitivity 0', 'in_usekeyboardsampletime 0', '-attack', '-attack2', '-reload',
        '-forward', '-back', '-moveleft', '-moveright', '-jump', '-duck', 'god', 'notarget', 'impulse 101',
        'echo GAMMA_E2E_START', later(0.7,'gamma_weapon_audit')]
    cursor, manifest = 2.0, []
    for index, name in enumerate(names,1):
        data=roster[name]; short=f'{index:02d}_{name}'
        inspect = name in INSPECT or (name=='weapon_manhack' and not args.allow_remote)
        reason = ('Non-firing carried/debug object; inspect deploy only' if name in INSPECT else
            'Remote possession can redirect commands; exercise separately with --only weapon_manhack --allow-remote' if inspect else '')
        mode = 'inspect' if inspect else 'world' if name in WORLD else 'fire'
        primary = bool(data.get('ammo')) and data['ammo'].lower()!='none'
        clipped = int(data.get('clip','-1/-1').split('/')[1])>0 and primary
        press, release = 4.2, 4.7
        after = press + LATE.get(name,0.65)
        reload_at = max(after+0.10, press+float(data.get('interval',0))+0.25)
        reload_settle = 4.1 if name in ('weapon_smg2','weapon_sniperrifle') else 3.1
        finish = reload_at+reload_settle if clipped and not inspect else after+0.20
        phases={'before':4.08,'during':4.43,'after':after,'reload':finish-0.05}
        for phase,at in phases.items():
            lines=[f'echo GAMMA_E2E_SNAPSHOT_BEGIN name={name} phase={phase} index={index}',
                'gamma_weaponinfo','gamma_viewmodelinfo','report_entities','gamma_playerinfo',
                f'echo GAMMA_E2E_SNAPSHOT_END name={name} phase={phase}']
            if name=='weapon_brickbat': lines.insert(2,'gamma_brickbat_info')
            if args.screenshots and phase=='during' and not inspect:
                lines.insert(-1,f'screenshot gamma_e2e_{short}')
            cfg(short+'_'+phase,lines)
        lines=[f'echo GAMMA_E2E_CASE name={name} mode={mode} index={index}',
            '-attack','-attack2','-reload','impulse 101',later(0.10,'use '+name)]
        for phase,at in phases.items(): lines.append(later(at,'exec gamma_weapon_e2e/'+short+'_'+phase+'.cfg'))
        if inspect:
            lines.append(f'echo GAMMA_E2E_SKIP_ACTION name={name} reason={reason}')
        else:
            lines += [later(press,'+attack'),later(release,'-attack')]
            if name=='weapon_manhack':
                lines += [later(after+0.06,'+attack2'),later(after+0.18,'-attack2')]
                finish=max(finish,after+0.30)
            lines += [later(reload_at,'+reload'),later(reload_at+0.12,'-reload')]
        if name=='weapon_rpg':
            lines.append(later(finish+0.08,'ent_remove_all rpg_missile'))
        cfg(short,lines)
        main.append(later(cursor,'exec gamma_weapon_e2e/'+short+'.cfg'))
        manifest.append({'index':index,'name':name,'mode':mode,'reason':reason,'start':round(cursor,3),
            'duration':round(finish+0.20,3),'hold':0.5,'primary_ammo':data.get('ammo',''),
            'interval':float(data.get('interval',0)), 'reload_expected':clipped,
            'phase_offsets':phases,'notes':'Entity report deltas are evidence; transient objects may expire between snapshots. World tools need appropriate targets/geometry.'})
        cursor+=finish+0.20
    main += [later(cursor+0.10,'-attack'),later(cursor+0.13,'-attack2'),later(cursor+0.16,'-reload'),
        later(cursor+0.30,'gamma_weapon_audit'),later(cursor+0.55,'echo GAMMA_E2E_COMPLETE')]
    cfg('start',main)
    result={'roster_log':str(args.roster_log.resolve()),'game_dir':str(game),'weapons':len(manifest),
        'duration_seconds':round(cursor+0.55,2),'launch_cfg':'gamma_weapon_e2e/start.cfg','cases':manifest}
    (folder/'manifest.json').write_text(json.dumps(result,indent=2)+'\n')
    (folder/'README.txt').write_text('Launch only the isolated test game, load a map with a clear firing lane, then exec gamma_weapon_e2e/start.cfg.\n'
        'No launch, shutdown, focus, or process management is performed by these files. Test changes only the isolated game configuration.\n'
        'Use -condebug for the log. Boot with player invulnerability enabled if firing explosives at close walls.\n'
        'The cfg waits at least four seconds after each selection. Entity snapshots and active ammo include secondary-only consumers such as Molotov.\n'
        'Carried/debug objects inspect only. Remote manhack possession inspects only by default; use a separate --only weapon_manhack --allow-remote run.\n'
        'Analyze with make_weapon_e2e.py analyze --manifest <this-folder>/manifest.json --log <console.log>.\n')
    print(json.dumps({k:v for k,v in result.items() if k!='cases'},indent=2))

def analyze(args):
    manifest=json.loads(args.manifest.read_text()); captured={}; current=None; completed=False
    for line in args.log.read_text(errors='replace').splitlines():
        if line.startswith('GAMMA_E2E_SNAPSHOT_BEGIN '):
            tag=fields(line);current={'entities':{},'views':[]};captured[(tag['name'],tag['phase'])]=current
        elif line.startswith('GAMMA_E2E_SNAPSHOT_END '):current=None
        elif line.startswith('GAMMA_E2E_COMPLETE'):completed=True
        elif current is not None:
            if line.startswith('GAMMA_ACTIVE '):current['active']=fields(line)
            elif line.startswith('GAMMA_BRICKBAT_VARIANTS '):current['variants']=fields(line)
            elif line.startswith('gamma_viewmodelinfo '):current['views'].append(line)
            else:
                match=re.match(r'Class: (\S+) \((\d+)\)',line)
                if match:current['entities'][match[1]]=int(match[2])
    results=[]
    for case in manifest['cases']:
        name=case['name'];snapshots={phase:captured.get((name,phase),{}) for phase in ('before','during','after','reload')}
        before=snapshots['before'].get('active',{});after=snapshots['after'].get('active',{})
        issues=[];delta=None;created={};status='tested'
        for phase in ('during','after'):
            for kind,count in snapshots[phase].get('entities',{}).items():
                increase=count-snapshots['before'].get('entities',{}).get(kind,0)
                if increase>0:created[kind]=max(created.get(kind,0),increase)
        if not before or not after:
            if completed:issues.append('missing_snapshot')
            else:status='pending'
        elif before.get('name')!=name:issues.append('selection_failed')
        elif after.get('name')!=name:
            # A final throwable can correctly remove itself and select another gun.
            throwable=name in ('weapon_brickbat','weapon_frag','weapon_hopwire','weapon_vortexhopwire','weapon_thumper','weapon_molotov')
            exhausted=int(before.get('reserve',0))<=1 and int(before.get('secondary',0))<=1
            projectile=any(kind.startswith(('grenade_','npc_grenade','portable_thumper')) for kind in created)
            if throwable and exhausted and projectile:
                status='depleted_and_autoswitched';delta=1
            else:issues.append('selection_failed')
        else:
            def ammo(state):
                return (max(0,int(state.get('clip',-1)))+int(state.get('reserve',0))) if case['primary_ammo'] else int(state.get('secondary',0))
            delta=ammo(before)-ammo(after)
            if case['mode']=='fire' and case['primary_ammo'] and delta<=0:issues.append('no_ammo_consumption')
            if case['mode']=='fire' and case['interval']>0 and delta is not None and delta>math.ceil(case['hold']/case['interval'])+2:
                issues.append('excess_shots_for_interval')
            if float(before.get('next_primary',0))>0.02:issues.append('deploy_not_ready')
        variants={phase:snapshots[phase].get('variants',{}) for phase in ('before','after')}
        for phase,state in variants.items():
            if state:
                total=sum(int(state[key].split('/')[0]) for key in ('rock','bottle','head'))
                if total!=int(state['reserve']):issues.append('private_ammo_mismatch_'+phase)
        results.append({'name':name,'mode':case['mode'],'status':status,'issues':issues,'ammo_spent':delta,
            'entity_increases':created,'before':before,'after':after,'reload':snapshots['reload'].get('active',{}),
            'private_ammo':variants,'viewmodel_before':snapshots['before'].get('views',[]),
            'viewmodel_after':snapshots['after'].get('views',[]),'reason':case['reason']})
    result={'completed':completed,'weapons':len(results),'tested':sum(item['status']!='pending' for item in results),
        'issues':sum(bool(item['issues']) for item in results),'results':results,
        'note':'No-ammo tools, placement and carried objects need visual/context review; unchanged ammo alone is not failure for them. Positive reload cooldown means the captured reload was still underway.'}
    output=args.out or args.log.with_suffix('.e2e.json');output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({'completed':completed,'weapons':len(results),'tested':result['tested'],'issues':result['issues'],'report':str(output)},indent=2))

def main():
    parser=argparse.ArgumentParser(description=__doc__);subs=parser.add_subparsers(dest='mode',required=True)
    gen=subs.add_parser('generate');gen.add_argument('--roster-log',type=Path,required=True);gen.add_argument('--game-dir',type=Path,required=True)
    gen.add_argument('--only');gen.add_argument('--screenshots',action='store_true');gen.add_argument('--allow-remote',action='store_true')
    ana=subs.add_parser('analyze');ana.add_argument('--log',type=Path,required=True);ana.add_argument('--manifest',type=Path,required=True);ana.add_argument('--out',type=Path)
    args=parser.parse_args();generate(args) if args.mode=='generate' else analyze(args)
if __name__=='__main__':main()
