"""Compare actual v36/v37 animation samples with converted v48 byte streams.

The v37 reader uses the existing validated normalizer; original files are never written.
"""
import argparse, importlib.util, json, math, struct
from pathlib import Path

def i(d,o): return struct.unpack_from('<i',d,o)[0]
def f(d,o): return struct.unpack_from('<f',d,o)[0]
def h(d,o): return struct.unpack_from('<h',d,o)[0]
def vec(d,o,n=3): return struct.unpack_from('<'+'f'*n,d,o)
def name(d,o): return d[o:d.index(0,o)].decode('latin1')
def qnorm(q):
    length=math.sqrt(sum(x*x for x in q))
    return tuple(x/length for x in q) if length else (0,0,0,1)
def aq(v):
    sx,sy,sz=[math.sin(x*.5) for x in v];cx,cy,cz=[math.cos(x*.5) for x in v]
    return (sx*cy*cz-cx*sy*sz,cx*sy*cz+sx*cy*sz,cx*cy*sz-sx*sy*cz,cx*cy*cz+sx*sy*sz)
def rle(d,o,frame):
    while True:
        valid,total=d[o:o+2]
        if not total or not 1 <= valid <= total: raise ValueError('Invalid animation RLE run')
        if frame<total: return h(d,o+2*(min(frame,valid-1)+1))
        frame-=total;o+=2*(valid+1)
def old_sample(d,b,a,frame):
    flags=i(d,a)
    p=tuple(f(d,b+32+4*j)+(rle(d,a+i(d,a+4+4*j),frame)*f(d,b+56+4*j) if i(d,a+4+4*j) else 0) for j in range(3)) if flags&1 else vec(d,a+4)
    q=aq(tuple(f(d,b+44+4*j)+(rle(d,a+i(d,a+16+4*j),frame)*f(d,b+68+4*j) if i(d,a+16+4*j) else 0) for j in range(3))) if flags&2 else vec(d,a+16,4)
    return p,qnorm(q)
def new_sample(d,b,a,frame):
    flags=d[a+1];offset=a+4
    if flags&0x20:
        bits=struct.unpack_from('<Q',d,offset)[0]
        xyz=tuple((((bits>>(j*21))&2097151)-1048576)/1048576.5 for j in range(3))
        q=xyz+(math.sqrt(max(0,1-sum(v*v for v in xyz)))*(-1 if bits>>63 else 1),)
    elif flags&8:
        q=aq(tuple((0 if flags&16 else f(d,b+60+4*j))+(rle(d,offset+h(d,offset+2*j),frame)*f(d,b+84+4*j) if h(d,offset+2*j) else 0) for j in range(3)))
    else:q=(0,0,0,1) if flags&16 else vec(d,b+44,4)
    if flags&1:
        p=struct.unpack_from('<eee',d,offset+(8 if flags&32 else 0))
    elif flags&4:
        po=offset+(6 if flags&8 else 0)
        p=tuple((0 if flags&16 else f(d,b+32+4*j))+(rle(d,po+h(d,po+2*j),frame)*f(d,b+72+4*j) if h(d,po+2*j) else 0) for j in range(3))
    else:p=(0,0,0) if flags&16 else vec(d,b+32)
    return p,qnorm(q)

def normalize_original(data):
    version=i(data,4)
    if version==37:
        helper=Path(__file__).resolve().parents[2]/'legacy2003/modernize/model_conversion/model37.py'
        spec=importlib.util.spec_from_file_location('gamma_model37_normalizer',helper)
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        data,_=module.convert(data)
    elif version!=36:
        raise ValueError('Expected MDL v36/v37, got '+str(version))
    return data,version

def audit_eyeballs(old,new):
    rows=[];issues=[]
    old_parts,new_parts=i(old,236),i(new,232)
    if old_parts!=new_parts:raise ValueError('Different bodypart counts for eyeball audit')
    for part in range(old_parts):
        op=i(old,240)+16*part;np=i(new,236)+16*part
        old_models,new_models=i(old,op+4),i(new,np+4)
        if old_models!=new_models:raise ValueError('Different model counts for eyeball audit')
        for model in range(old_models):
            om=op+i(old,op+12)+140*model;nm=np+i(new,np+12)+148*model
            old_count,new_count=i(old,om+100),i(new,nm+100)
            if old_count!=new_count:
                issues.append({'part':part,'model':model,'issue':'different eyeball count','original':old_count,'converted':new_count})
            for eye in range(min(old_count,new_count)):
                oe=om+i(old,om+104)+140*eye;ne=nm+i(new,nm+104)+172*eye
                row={'part':part,'model':model,'eye':eye,'name':name(old,oe+i(old,oe)),'original_iris_scale':f(old,oe+60),'converted_iris_scale':f(new,ne+60),'original_upper_flexdesc':list(struct.unpack_from('<3i',old,oe+68)),'converted_upper_flexdesc':list(struct.unpack_from('<3i',new,ne+68)),'issues':[]}
                if row['name']!=name(new,ne+i(new,ne)):row['issues'].append('eyeball name changed')
                for first,last in [(4,56),(60,64),(68,124)]:
                    if old[oe+first:oe+last]!=new[ne+first:ne+last]:row['issues'].append('prefix bytes %d:%d changed'%(first,last))
                for offset in [56,64]:
                    if i(new,ne+offset)!=0:row['issues'].append('modern unused material slot %d is not zero'%offset)
                rows.append(row)
    return {'eyes_checked':len(rows),'mismatched_eyes':sum(bool(r['issues']) for r in rows)+len(issues),'layout':'Original140-byte eyeball / v48 172-byte eyeball; compare shared124-byte prefix except relocated name0 and intentionally cleared material slots56/64. Iris60 and all eyelid bytes68:124 must match exactly.','issues':issues,'eyes':rows}

def audit_pair(original,converted,all_frames=False):
    original,converted=Path(original),Path(converted)
    old,version=normalize_original(original.read_bytes());new=converted.read_bytes()
    if old[:4]!=b'IDST' or new[:4]!=b'IDST' or i(new,4)!=48:
        raise ValueError('Expected original studio model and converted MDL v48')
    if i(old,8)!=i(new,8):raise ValueError('Different model checksums')
    nb=i(old,156)
    if nb!=i(new,156):raise ValueError('Different bone counts')
    na_count=i(old,180)
    if na_count!=i(new,180):raise ValueError('Different animation counts')
    quantized=[];mismatch_counts={False:0,True:0}
    rows=[];summary={False:{'rotation_degrees':0,'position_units':0,'samples':0},True:{'rotation_degrees':0,'position_units':0,'samples':0}}
    for an in range(na_count):
        oa=i(old,184)+an*92;na=i(new,184)+an*100
        label=name(old,oa+i(old,oa))
        if label!=name(new,na+i(new,na+4)):raise ValueError('Different animation labels at '+str(an))
        delta=bool(i(old,oa+8)&4);nf=max(1,i(old,oa+12))
        if nf!=i(new,na+16):raise ValueError('Different frame count: '+label)
        node=na+i(new,na+56);nodes={}
        while nb:
            if not 0<=node<=len(new)-4:raise ValueError('Animation node outside file: '+label)
            bone_id=new[node]
            if bone_id>=nb or bone_id in nodes:raise ValueError('Invalid animation bone chain: '+label)
            nodes[bone_id]=node;step=h(new,node+2)
            if not step:break
            if step<4:raise ValueError('Invalid next animation node: '+label)
            node+=step
        frames=range(nf) if all_frames else sorted({0,nf//3,2*nf//3,nf-1})
        for bone in range(nb):
            ob=i(old,160)+bone*196;bn=i(new,160)+bone*216
            source=oa+i(old,oa+48)+bone*32
            if bone not in nodes:raise ValueError('Missing converted animation bone '+str(bone)+': '+label)
            for frame in frames:
                op,oq=old_sample(old,ob,source,frame);np,nq=new_sample(new,bn,nodes[bone],frame)
                if not all(math.isfinite(x) for x in (*op,*oq,*np,*nq)):raise ValueError('Nonfinite sample: '+label)
                angle=math.degrees(2*math.acos(min(1,abs(sum(x*y for x,y in zip(oq,nq))))))
                position=math.sqrt(sum((x-y)**2 for x,y in zip(op,np)))
                group=summary[delta];group['samples']+=1;group['rotation_degrees']=max(group['rotation_degrees'],angle);group['position_units']=max(group['position_units'],position)
                sample={'animation':label,'bone':name(old,ob+i(old,ob)),'frame':frame,'delta':delta,'rotation_degrees':angle,'position_units':position}
                raw_position=bool(new[nodes[bone]+1]&1)
                # Source's float16 Vector48 truncates to 10 mantissa bits. A
                # component may lose <1 half ULP, plus the converter's 1e-4
                # constant-track threshold and float32 source cast. Keep this
                # format bound separate from the .05-unit RLE tolerance.
                bounds=[max(2**-24,2**(math.frexp(abs(value))[1]-11)) + 1e-4 + abs(struct.unpack('<f',struct.pack('<f',value))[0]-value) for value in op]
                within_raw_bound=raw_position and all(abs(x-y)<=bound for x,y,bound in zip(op,np,bounds))
                if position>.05 and within_raw_bound:
                    sample['component_ulp_bounds']=bounds;quantized.append(sample)
                if angle>.25 or (position>.05 and not within_raw_bound):
                    rows.append(sample);mismatch_counts[delta]+=1
    rows.sort(key=lambda r:r['rotation_degrees']+r['position_units'],reverse=True)
    return {'original':str(original),'converted':str(converted),'source_version':version,'bones':nb,'animations':na_count,'frames_checked':'all' if all_frames else 'four distributed per animation','summary':{'delta' if k else 'absolute':v for k,v in summary.items()},'eyeballs':audit_eyeballs(old,new),'mismatched_samples':len(rows),'mismatch_counts':{'delta' if k else 'absolute':v for k,v in mismatch_counts.items()},'native_float16_quantization':{'samples_above_0_05u_within_component_ulp_bounds':len(quantized),'maximum_position_units':max((r['position_units'] for r in quantized),default=0),'worst':sorted(quantized,key=lambda r:r['position_units'],reverse=True)[:10]},'worst':rows[:20]}

def main():
    ap=argparse.ArgumentParser(description=__doc__.splitlines()[0]);ap.add_argument('original',type=Path);ap.add_argument('converted',type=Path);ap.add_argument('--out',type=Path);ap.add_argument('--all-frames',action='store_true');args=ap.parse_args()
    result=audit_pair(args.original,args.converted,args.all_frames)
    if args.out:args.out.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2));return 1 if result['mismatched_samples'] or result['eyeballs']['mismatched_eyes'] else 0
if __name__=='__main__':raise SystemExit(main())
