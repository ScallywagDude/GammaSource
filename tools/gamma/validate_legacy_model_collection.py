"""Inventory original models, audit actual runtime cache bytes, and prepare isolated native warmup CFGs."""
import argparse, collections, hashlib, json, shutil, struct
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
from audit_legacy_animation_samples import audit_pair, normalize_original


def read_i(data,offset):
    return struct.unpack_from('<i',data,offset)[0]


def inventory(root):
    rows=[]
    for path in sorted((root/'models').rglob('*.mdl')):
        row={'source':str(path.resolve()),'game_root':str(root.resolve()),'model':path.relative_to(root).as_posix(),'status':'unexamined'}
        try:
            data=path.read_bytes();row['signature']=data[:4].decode('ascii',errors='replace');row['version']=read_i(data,4)
            if data[:4]!=b'IDST' or row['version'] not in (36,37):
                row['status']='outside_legacy_studio_scope';rows.append(row);continue
            row.update(checksum=read_i(data,8),bones=read_i(data,156),animations=read_i(data,180),bytes=len(data))
            table=read_i(data,184)
            if not 0<=row['bones']<=128 or not 0<=row['animations']<=10000 or table<0 or table+92*row['animations']>len(data):
                raise ValueError('invalid animation/bone table bounds')
            flags=[(read_i(data,table+92*k+8),max(1,read_i(data,table+92*k+12))) for k in range(row['animations'])]
            row['delta_animations']=sum(bool(flag&4) for flag,_ in flags)
            row['moving_delta_animations']=sum(bool(flag&4) and frames>1 for flag,frames in flags)
            row['absolute_animations']=sum(not(flag&4) for flag,_ in flags)
            lower=row['model'].lower()
            row['category']='weapon' if '/weapons/' in lower else ('character' if row['bones']>15 and row['animations']>8 and lower.count('/')==1 else 'prop')
            row['status']='legacy_studio'
        except Exception as error:
            row['status']='invalid_or_unreadable';row['error']=str(error)
        rows.append(row)
    return rows


def compare_job(job):
    original,converted=job
    try:
        result=audit_pair(original,converted)
        result['status']='pass' if not result['mismatched_samples'] and not result['eyeballs']['mismatched_eyes'] else 'sample_mismatch'
        return result
    except Exception as error:
        return {'original':str(original),'converted':str(converted),'status':'audit_error','error':str(error)}


def cache_metadata(path,require_marker=False):
    """Read native cache headers; check v5 tags without changing cached bytes."""
    data=path.read_bytes()
    if data[:4]!=b'IDST' or len(data)<200 or read_i(data,4)!=48:
        raise ValueError('Expected native converted studio MDL v48')
    row={'checksum':read_i(data,8),'bytes':len(data),'marker_required':require_marker}
    if require_marker:
        for name,count_offset,index_offset,stride in [('animations',180,184,100),('sequences',188,192,212)]:
            count,table=read_i(data,count_offset),read_i(data,index_offset)
            if not 0<=count<=10000 or table<0 or table+stride*count>len(data):
                raise ValueError('Invalid converted '+name+' table bounds')
            missing=[n for n in range(count) if not read_i(data,table+stride*n+12)&0x40000000]
            row[name]=count;row[name+'_missing_legacy_marker']=missing
    return row


def requested_coverage(warmup,cache_rows,rows):
    expected={}
    for row in rows:
        if row['status']=='legacy_studio':expected.setdefault(row['model'].lower(),row)
    for alias in warmup['v37_private_aliases']:
        expected[alias['model'].lower()]=alias['original']
    requested=[]
    for model in warmup['cfg_models']:
        actual=cache_rows.get(model.lower());source=expected.get(model.lower())
        row={'model':model,'source':source['source'] if source else None}
        if not actual:row['status']='missing_cache_output'
        elif 'error' in actual:row.update(status='invalid_cache_output',error=actual['error'])
        elif not source or actual['checksum']!=source['checksum']:
            row.update(status='unexpected_checksum',expected_checksum=source.get('checksum') if source else None,actual_checksum=actual['checksum'])
        elif actual.get('animations_missing_legacy_marker') or actual.get('sequences_missing_legacy_marker'):
            row['status']='missing_legacy_marker'
        else:row['status']='cache_output_present'
        requested.append(row)
    counts=collections.Counter(row['status'] for row in requested)
    return {'requested_models':len(requested),'status_counts':dict(counts),'models':requested}


def alias_integrity(warmup,alias_game_root=None):
    """Confirm test aliases still contain the original, recorded model bytes."""
    results=[]
    for alias in warmup['v37_private_aliases']:
        for file in alias['files']:
            targets=[('original',Path(file['source'])),('prepared_alias',Path(file['output']))]
            if alias_game_root:
                alias_model=Path(alias['model']);original=Path(alias['original']['source'])
                suffix=Path(file['source']).name[len(original.stem):]
                targets.append(('mounted_alias',alias_game_root/alias_model.parent/(alias_model.stem+suffix)))
            for kind,path in targets:
                row={'model':alias['model'],'kind':kind,'path':str(path),'expected_sha256':file['sha256']}
                if path.is_file():
                    row['sha256']=hashlib.sha256(path.read_bytes()).hexdigest()
                    row['status']='pass' if row['sha256']==file['sha256'] else 'hash_mismatch'
                else:row['status']='missing'
                results.append(row)
    return {'files_checked':len(results),'failed_files':sum(row['status']!='pass' for row in results),'files':results}


def make_cfg(models,path,marker):
    lines=['// Isolated test only. Original model bytes are never modified.','sv_cheats 1','echo '+marker+'_BEGIN']
    # Keep only one temporary render prop at a time; disable physics/collision.
    for index,model in enumerate(models):
        at=.2+index*.4
        lines += ['gamma_delay %.2f echo GAMMA_MODEL_WARMUP %d %s'%(at,index,model),
                  'gamma_delay %.2f ent_create prop_dynamic_override targetname gamma_model_warmup model %s spawnflags 384 solid 0'%(at+.04,model),
                  'gamma_delay %.2f ent_fire gamma_model_warmup Kill'%(at+.25)]
    lines += ['gamma_delay %.2f echo %s_COMPLETE'%(.6+len(models)*.4,marker)]
    path.write_text('\n'.join(lines)+'\n',encoding='utf-8')


def prepare_warmups(rows,out):
    by_name={}
    for row in rows:
        if row['status']=='legacy_studio':by_name.setdefault(row['model'].lower(),row)
    requested=['alyx','kleiner','eli','barney','gman','breen','monk','mossman','citizen','zombie','headcrab','antlion','antlion_guard','strider','hydra','stalker','combine_soldier','combine_super_soldier','scanner','manhack','crow','pigeon','seagull','barnacle','dog','weapons/v_pistol','weapons/v_smg1','weapons/v_shotgun','weapons/v_ar2','weapons/v_irifle','weapons/v_crowbar','weapons/v_alyxgun','weapons/w_pistol','weapons/w_smg1','weapons/w_shotgun','weapons/w_crowbar','items/healthkit','items/battery','props_c17/oildrum001','props_junk/wood_crate001a','props_lab/citizenradio','airboat','buggy','apc']
    selected=[]
    for name in requested:
        row=by_name.get('models/'+name+'.mdl')
        if row and row not in selected:selected.append(row)
    # Ensure each category and delta/non-delta class is represented, even if names differ.
    for category in ['character','weapon','prop']:
        for delta in [True,False]:
            candidates=[r for r in by_name.values() if r['category']==category and bool(r['moving_delta_animations'])==delta and r not in selected]
            selected+=sorted(candidates,key=lambda r:(-r['moving_delta_animations'] if delta else -r['absolute_animations'],r['bytes']))[:3]
    selected=selected[:60]
    models=[r['model'] for r in selected]
    alias_rows=[]
    for category in ['character','weapon','prop']:
        candidates=[r for r in rows if r['status']=='legacy_studio' and r['version']==37 and r['category']==category and r['animations']>0]
        candidates.sort(key=lambda r:(not bool(r['moving_delta_animations']),r['bytes']))
        accepted=0
        for row in candidates:
            try:normalize_original(Path(row['source']).read_bytes())
            except Exception:continue
            source=Path(row['source']);relative=Path(row['model']).relative_to('models');alias=Path('models/gamma_validation_v37')/relative
            target=out/'private_warmup_overlay'/alias;target.parent.mkdir(parents=True,exist_ok=True)
            companions=[]
            for suffix in ['.mdl','.dx90.vtx','.dx80.vtx','.sw.vtx','.dx7_2bone.vtx','.phy']:
                original=source.with_suffix(suffix)
                if original.exists():
                    dest=target.with_suffix(suffix);shutil.copyfile(original,dest)
                    companions.append({'source':str(original),'output':str(dest),'sha256':hashlib.sha256(original.read_bytes()).hexdigest()})
            alias_rows.append({'model':alias.as_posix(),'original':row,'files':companions});models.append(alias.as_posix());accepted+=1
            if accepted==2:break
    make_cfg(models,out/'gamma_model_warmup_representative.cfg','GAMMA_MODEL_REPRESENTATIVE')
    # Full collection is split across map reloads to respect engine precache limits.
    all_models=[r['model'] for r in by_name.values()]
    batch_dir=out/'all_model_batches';batch_dir.mkdir(exist_ok=True)
    batches=[]
    for number,start in enumerate(range(0,len(all_models),150)):
        target=batch_dir/('gamma_model_warmup_all_%02d.cfg'%number)
        make_cfg(all_models[start:start+150],target,'GAMMA_MODEL_BATCH_%02d'%number)
        batches.append({'cfg':str(target),'models':len(all_models[start:start+150]),'reload_map_before_next_batch':True})
    return {'representative':selected,'v37_private_aliases':alias_rows,'cfg_models':models,'cfg_seconds':round(.6+len(models)*.4,2),'full_batches':batches,'note':'Warmup proves only conversion/load. Numerical checker validates actual cached animation bytes afterward. Test-only aliases contain byte-identical original model/VTX files; do not deliver them as production models.'}


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--source',type=Path,action='append',required=True,help='Original game root containing models; repeat in mount priority order.')
    ap.add_argument('--cache',type=Path,required=True,help='Actual legacy_models_cache_vN/models directory produced by the native engine.')
    ap.add_argument('--audit-only',action='store_true',help='Reuse warmup manifest without rewriting its test-only overlay/CFG.')
    ap.add_argument('--reuse-inventory',type=Path,help='Reuse prior metadata only; every audited original model is still read and checked against its cache bytes.')
    ap.add_argument('--warmup-manifest',type=Path,help='Reuse an existing warmup manifest while writing audit results into a separate directory.')
    ap.add_argument('--alias-game-root',type=Path,help='Also verify private aliases copied into this isolated game root; omit when mounting their prepared overlay directly.')
    ap.add_argument('--require-legacy-marker',action='store_true',help='Require the 2003 marker on every converted animation and sequence, as produced by cache v5.')
    ap.add_argument('--out',type=Path,required=True);ap.add_argument('--inventory-only',action='store_true');ap.add_argument('--workers',type=int,default=2)
    args=ap.parse_args();args.out.mkdir(parents=True,exist_ok=True)
    rows=json.loads(args.reuse_inventory.read_text())['models'] if args.reuse_inventory else [row for root in args.source for row in inventory(root)]
    (args.out/'model_inventory.json').write_text(json.dumps({'sources':[str(r) for r in args.source],'counts':dict(collections.Counter((r.get('signature','bad')+' v'+str(r.get('version','?'))) for r in rows)),'models':rows},indent=2)+'\n')
    manifest=args.warmup_manifest or args.out/'warmup_manifest.json'
    warmup=json.loads(manifest.read_text()) if args.audit_only or args.warmup_manifest else prepare_warmups(rows,args.out);(args.out/'warmup_manifest.json').write_text(json.dumps(warmup,indent=2)+'\n')
    print('Inventory %d files; representative %d models, %.1f seconds; %d exact v37 aliases.'%(len(rows),len(warmup['cfg_models']),warmup['cfg_seconds'],len(warmup['v37_private_aliases'])),flush=True)
    if args.inventory_only:return 0
    sources=collections.defaultdict(list)
    for row in rows:
        if row['status']=='legacy_studio':sources[(row['model'].lower(),row['checksum'])].append(row['source'])
    for alias in warmup['v37_private_aliases']:
        sources[(alias['model'].lower(),alias['original']['checksum'])].append(alias['original']['source'])
    jobs=[];unmatched=[];cache_rows={}
    for converted in sorted(args.cache.rglob('*.mdl')):
        model='models/'+converted.relative_to(args.cache).as_posix()
        try:
            metadata=cache_metadata(converted,args.require_legacy_marker);cache_rows[model.lower()]=metadata
            candidates=sources.get((model.lower(),metadata['checksum']),[])
            if not candidates:unmatched.append({'converted':str(converted),'reason':'No original path+checksum match in supplied roots'});continue
            jobs.extend((source,converted) for source in candidates)
        except Exception as error:
            cache_rows[model.lower()]={'error':str(error)};unmatched.append({'converted':str(converted),'error':str(error)})
    results=[]
    with ThreadPoolExecutor(max_workers=max(1,args.workers)) as pool:
        for result in pool.map(compare_job,jobs):
            results.append(result)
            if len(results)%25==0:print('Audited %d/%d original/cache pairs'%(len(results),len(jobs)),flush=True)
    counts=collections.Counter(r['status'] for r in results)
    summary={'cache_exists':args.cache.is_dir(),'pair_counts':dict(counts),'unique_cached_models_audited':len(set(r['converted'] for r in results)),'samples':sum(sum(g['samples'] for g in r.get('summary',{}).values()) for r in results),'mismatched_samples':sum(r.get('mismatched_samples',0) for r in results),'eyeballs_checked':sum(r.get('eyeballs',{}).get('eyes_checked',0) for r in results),'mismatched_eyeballs':sum(r.get('eyeballs',{}).get('mismatched_eyes',0) for r in results),'delta_mismatched_samples':sum(r.get('mismatch_counts',{}).get('delta',0) for r in results),'native_float16_quantized_samples':sum(r.get('native_float16_quantization',{}).get('samples_above_0_05u_within_component_ulp_bounds',0) for r in results),'unmatched_cache_models':unmatched,'results':results,'coverage_limit':'Only existing outputs in the requested cache were numerically audited. Remaining original models await optional warmup batches.'}
    summary.update(cache_directory=str(args.cache.resolve()),actual_cache_outputs=len(cache_rows),requested_coverage=requested_coverage(warmup,cache_rows,rows),private_alias_integrity=alias_integrity(warmup,args.alias_game_root),cache_metadata=cache_rows)
    summary['missing_marker_models']=[name for name,row in cache_rows.items() if row.get('animations_missing_legacy_marker') or row.get('sequences_missing_legacy_marker')]
    (args.out/'bulk_animation_audit.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps({k:v for k,v in summary.items() if k not in ['results','unmatched_cache_models','cache_metadata','private_alias_integrity','requested_coverage']},indent=2),flush=True)
    print('Requested cache coverage:',summary['requested_coverage']['status_counts'],'Alias integrity failures:',summary['private_alias_integrity']['failed_files'],flush=True)
    if not args.cache.is_dir() or not results:return 2
    incomplete=any(row['status']!='cache_output_present' for row in summary['requested_coverage']['models'])
    return 1 if counts['sample_mismatch'] or counts['audit_error'] or unmatched or incomplete or summary['private_alias_integrity']['failed_files'] or summary['missing_marker_models'] else 0

if __name__=='__main__':raise SystemExit(main())
