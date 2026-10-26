"""Compare original and actual cached subframe interpolation before outer layers/IK.

Both runtimes normalize a linear quaternion blend between decoded adjacent
frames. This audit explicitly exercises the fractional result, including the
hemisphere decision; the ordinary sample checker only checks integer frames.
"""
import argparse
import json
import math
from pathlib import Path
import audit_legacy_animation_samples as samples


def blend(a,b,t):
    p,q=a;r,s=b
    if sum(x*y for x,y in zip(q,s))<0:s=tuple(-x for x in s)
    return tuple(x*(1-t)+y*t for x,y in zip(p,r)),samples.qnorm(tuple(x*(1-t)+y*t for x,y in zip(q,s)))


def audit(original,converted,animation_names=None):
    old,version=samples.normalize_original(original.read_bytes());new=converted.read_bytes()
    if new[:4]!=b'IDST' or samples.i(new,4)!=48:raise ValueError('Expected actual converted v48 cache')
    if samples.i(old,8)!=samples.i(new,8):raise ValueError('Original/cache checksum mismatch')
    nb,na=samples.i(old,156),samples.i(old,180)
    if nb!=samples.i(new,156) or na!=samples.i(new,180):raise ValueError('Bone or animation count mismatch')
    tested=changed_hemisphere=0;max_angle=max_pos=0;worst=[];animations=0
    for an in range(na):
        oa=samples.i(old,184)+92*an;ca=samples.i(new,184)+100*an
        label=samples.name(old,oa+samples.i(old,oa));nf=samples.i(old,oa+12)
        if nf<2 or animation_names and label.lower() not in animation_names:continue
        animations+=1
        node=ca+samples.i(new,ca+56);nodes={}
        while True:
            if not 0<=node<=len(new)-4:raise ValueError('Invalid node '+label)
            bone=new[node]
            if bone>=nb or bone in nodes:raise ValueError('Invalid node chain '+label)
            nodes[bone]=node;step=samples.h(new,node+2)
            if not step:break
            node+=step
        for bone in range(nb):
            ob=samples.i(old,160)+196*bone;cb=samples.i(new,160)+216*bone
            source=oa+samples.i(old,oa+48)+32*bone;dest=nodes[bone]
            prev_old=samples.old_sample(old,ob,source,0);prev_new=samples.new_sample(new,cb,dest,0)
            for frame in range(nf-1):
                next_old=samples.old_sample(old,ob,source,frame+1);next_new=samples.new_sample(new,cb,dest,frame+1)
                old_dot=sum(x*y for x,y in zip(prev_old[1],next_old[1]));new_dot=sum(x*y for x,y in zip(prev_new[1],next_new[1]))
                # Sign changes from equivalent quaternion storage are harmless.
                # Only compare the fractional poses to detect a path flip.
                for fraction in (.25,.5,.75):
                    op,oq=blend(prev_old,next_old,fraction);np,nq=blend(prev_new,next_new,fraction)
                    angle=math.degrees(2*math.acos(min(1,abs(sum(x*y for x,y in zip(oq,nq))))))
                    distance=math.sqrt(sum((x-y)**2 for x,y in zip(op,np)))
                    tested+=1;max_angle=max(max_angle,angle);max_pos=max(max_pos,distance)
                    if angle>.25:
                        row={'animation':label,'bone':samples.name(old,ob+samples.i(old,ob)),'frame':frame,'fraction':fraction,'rotation_degrees':angle,'position_units':distance,'old_endpoint_dot':old_dot,'new_endpoint_dot':new_dot}
                        worst.append(row)
                prev_old,prev_new=next_old,next_new
        print('Audited subframes:',label,'frames',nf,flush=True)
    worst.sort(key=lambda row:row['rotation_degrees'],reverse=True)
    return {'original':str(original),'converted':str(converted),'source_version':version,'moving_animations':animations,'fractional_samples':tested,'fractions':[.25,.5,.75],'max_rotation_degrees':max_angle,'max_position_units':max_pos,'rotation_mismatches_over_0_25_degrees':len(worst),'worst':worst[:20],'scope':'Actual fractional decoded pose samples before sequence grid blending, autolayers, overlays, IK and procedural helpers. Position error includes native Vector48 quantization; use integer sample audit for component-derived format bounds.'}


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('original',type=Path);parser.add_argument('converted',type=Path)
    parser.add_argument('--animation',action='append',help='Limit to these exact animation labels; repeat as needed.')
    parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args()
    result=audit(args.original,args.converted,{name.lower() for name in args.animation} if args.animation else None)
    args.out.parent.mkdir(parents=True,exist_ok=True);args.out.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k!='worst'},indent=2))
    raise SystemExit(1 if result['rotation_mismatches_over_0_25_degrees'] else 0)
