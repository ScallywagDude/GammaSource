#!/usr/bin/env python3
"""Read-only verification of v36/v37 actor autolayers and fresh v48 cache metadata."""
from pathlib import Path
import argparse,json,struct

def model(path):
    data=path.read_bytes();I=lambda off:struct.unpack_from('<i',data,off)[0]
    text=lambda off:data[off:data.index(0,off)].decode('latin1') if 0<off<len(data) else ''
    version=I(4)
    if version==36:count,base,stride,name,lc,lo=I(188),I(192),4284,0,4240,4244
    elif version==37:count,base,stride,name,lc,lo=I(204),I(208),188,0,144,148
    elif version in (48,49):count,base,stride,name,lc,lo=I(188),I(192),212,4,148,152
    else:raise ValueError('Unsupported version '+str(version))
    sequences=[]
    for index in range(count):
        pos=base+index*stride;layers=[]
        for layer in range(I(pos+lc)):
            at=pos+I(pos+lo)+24*layer
            if version<48:
                target,flags=struct.unpack_from('<ii',data,at);pose=-1
            else:target,pose,flags=struct.unpack_from('<hhi',data,at)
            layers.append({'sequence':target,'pose':pose,'flags':flags,'ramp':struct.unpack_from('<4f',data,at+8)})
        sequences.append({'name':text(pos+I(pos+name)),'layers':layers})
    offset=16 if version==37 else 0
    controllers=[]
    for index in range(I(268+offset)):
        at=I(272+offset)+20*index
        controllers.append({'name':text(at+I(at+4)),'global':I(at+8)})
    return {'path':str(path),'version':version,'sequences':sequences,'controllers':controllers}

def compare(source,converted):
    errors=[]
    if len(source['sequences'])!=len(converted['sequences']):errors.append('sequence_count')
    for index,(old,new) in enumerate(zip(source['sequences'],converted['sequences'])):
        if old['name']!=new['name']:errors.append('sequence_label_'+str(index))
        if len(old['layers'])!=len(new['layers']):errors.append('layer_count_'+old['name']);continue
        for before,after in zip(old['layers'],new['layers']):
            flags=(before['flags']&0x10)|(0x40 if before['flags']&0x80 else 0)
            start,peak,tail,end=before['ramp']
            expected=(start,peak if before['flags']&0x40 else start,tail if before['flags']&0x40 else end,end)
            if after['sequence']!=before['sequence'] or after['pose']!=-1 or after['flags']!=flags or tuple(after['ramp'])!=expected:
                errors.append('layer_data_'+old['name'])
    if [c['name'] for c in source['controllers']]!=[c['name'] for c in converted['controllers']]:errors.append('flex_names')
    if any(c['global']!=-1 for c in converted['controllers']):errors.append('uninitialized_flex_mapping_sentinel')
    return errors

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,default=Path(r'C:\Users\doge6\Videos\GammaSource-main\gammasource_content\models'))
    parser.add_argument('--cache',type=Path,required=True,help='Use <testgame>/legacy_models_cache_v2/models')
    parser.add_argument('--models',default='Alyx.mdl,Kleiner.mdl,Eli.mdl');parser.add_argument('--out',type=Path)
    args=parser.parse_args();results=[]
    for name in args.models.split(','):
        before=model(args.source/name);path=args.cache/name
        row={'name':name,'source':str(args.source/name),'source_version':before['version'],
            'sequences':len(before['sequences']),'autolayers':sum(len(s['layers']) for s in before['sequences']),
            'layered_sequences':{s['name']:len(s['layers']) for s in before['sequences'] if s['layers']},
            'facial_controllers':len(before['controllers']),'cache':str(path)}
        if path.exists():
            after=model(path);row['errors']=compare(before,after)
            row['status']='passed' if not row['errors'] else 'failed'
        else:row.update(status='awaiting_runtime_conversion',errors=[])
        results.append(row)
    report={'models':results,'all_converted_and_valid':all(r['status']=='passed' for r in results),
        'note':'Files in the old unversioned cache are intentionally ignored by the fixed loader. Runtime actorinfo additionally verifies scene playback and actual facial mapping.'}
    output=json.dumps(report,indent=2)
    if args.out:args.out.parent.mkdir(parents=True,exist_ok=True);args.out.write_text(output+'\n')
    print(output)
    return 1 if any(r['errors'] for r in results) else 0
if __name__=='__main__':raise SystemExit(main())
