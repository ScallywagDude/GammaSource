"""Export Source geometry rebound to a ValveBiped animation rig.

Physical joint directions and palm frames preserve the mesh across differently
authored Portal/ValveBiped axes. Helpers retain their mapped ancestor offsets.
"""
import argparse, collections, hashlib, json, math, pathlib, re, struct
import numpy as np
from character_eyes import attachment_qc, model_qc, retarget_eyes

def i(b, o): return struct.unpack_from('<i', b, o)[0]
def s(b, o): return b[o:b.index(b'\0', o)].decode('latin1')
def f(b, o, n): return struct.unpack_from('<'+'f'*n, b, o)

def bones(b):
    result=[]
    for k in range(i(b,156)):
        o=i(b,160)+216*k
        inverse=np.eye(4); inverse[:3]=np.array(f(b,o+96,12)).reshape(3,4)
        result.append(dict(name=s(b,o+i(b,o)),parent=i(b,o+4),pos=f(b,o+32,3),rot=f(b,o+60,3),inverse=inverse))
    return result

def ikchain_qc(target):
    """Keep the include model's numeric IK chain contract in the body model.

    Included animation rules and autoplay locks reference main-model chains by
    index. Merely including an animation MDL does not populate that table.
    """
    data=target.read_bytes(); target_bones=bones(data); lines=[]
    for number in range(i(data,284)):
        chain=i(data,288)+16*number
        assert i(data,chain+8)==3
        links=chain+i(data,chain+12)
        name=s(data,chain+i(data,chain))
        end=target_bones[i(data,links+56)]['name']
        knee=f(data,links+4,3)
        lines.append(f'$ikchain "{name}" "{end}" knee '+' '.join(f'{value:.9g}' for value in knee))
    assert len(lines)==4, 'Expected the four-chain HL2DM player animation rig'
    return '\n'.join(lines)

def semantic_transforms(old, new, mapping):
    """Align physical joints, independent of each author's local bone axes.

    Portal bind geometry is Y-up and its authored joint coordinate frames differ
    from ValveBiped. Multiplying complete bind frames twists the chest/limbs.
    Use the body axes and actual joint-to-child directions instead. Helpers
    mapped to an ancestor share that ancestor's transform, preserving offsets.
    """
    prefix='ValveBiped.Bip01_'
    lookup={b['name']:n for n,b in enumerate(new)}
    oldpos=np.array([np.linalg.inv(b['inverse'])[:3,3] for b in old])
    newpos=np.array([np.linalg.inv(b['inverse'])[:3,3] for b in new])
    canonical={}
    def score(name):
        name=name.lower()
        return 10 if name.startswith('valvebiped.') else 5 if name.startswith('bip_') else 4 if name in ('hips','pelvis') else 0 if name=='root' else 3
    for number,bone in enumerate(old):
        name=map_name(bone['name'])
        if name in lookup and (name not in canonical or score(bone['name'])>score(old[canonical[name]]['name'])):
            canonical[name]=number
    def basis(position, index):
        right=position[index[prefix+'R_UpperArm']]-position[index[prefix+'L_UpperArm']]
        right/=np.linalg.norm(right)
        up=position[index[prefix+'Head1']]-position[index[prefix+'Pelvis']]
        up-=right*(right@up);up/=np.linalg.norm(up)
        forward=np.cross(up,right)
        return np.column_stack((right,forward,up))
    global_rotation=basis(newpos,lookup)@basis(oldpos,canonical).T
    def palm_basis(position,index,side):
        wrist=position[index[prefix+side+'_Hand']]
        knuckles=[position[index[prefix+side+'_Finger'+str(digit)]] for digit in (1,2,3,4)]
        length=sum(knuckles)/4-wrist;length/=np.linalg.norm(length)
        radial=knuckles[0]-knuckles[-1];radial-=length*(length@radial);radial/=np.linalg.norm(radial)
        return np.column_stack((length,radial,np.cross(length,radial)))
    palms={side:palm_basis(newpos,lookup,side)@palm_basis(oldpos,canonical,side).T for side in ('L','R')}
    def align(a,b):
        a=a/np.linalg.norm(a);b=b/np.linalg.norm(b)
        cross=np.cross(a,b);cos=float(np.clip(a@b,-1,1))
        if cos<-.999999:
            axis=np.cross(a,np.array([1.,0.,0.]) if abs(a[0])<.8 else np.array([0.,1.,0.]))
            axis/=np.linalg.norm(axis)
            return 2*np.outer(axis,axis)-np.eye(3)
        skew=np.array([[0.,-cross[2],cross[1]],[cross[2],0.,-cross[0]],[-cross[1],cross[0],0.]])
        return np.eye(3)+skew+skew@skew/(1+cos)
    children={'Pelvis':['Spine'],'Spine':['Spine1'],'Spine1':['Spine2'],
              'Spine2':['Spine4','Neck1'],'Spine4':['Neck1'],'Neck1':['Head1']}
    for side in ('L','R'):
        for start,end in [('Clavicle','UpperArm'),('UpperArm','Forearm'),('Forearm','Hand'),
                          ('Hand','Finger2'),('Thigh','Calf'),('Calf','Foot'),('Foot','Toe0')]:
            children[side+'_'+start]=[side+'_'+end]
        for digit in range(5):
            children[f'{side}_Finger{digit}']=[f'{side}_Finger{digit}1']
            children[f'{side}_Finger{digit}1']=[f'{side}_Finger{digit}2']
    transforms={};scales=[]
    for name,source_id in canonical.items():
        reference_rotation=global_rotation
        for side in ('L','R'):
            if name==prefix+side+'_Hand' or name.startswith(prefix+side+'_Finger'):
                reference_rotation=palms[side]
        rotation=reference_rotation;scale=np.eye(3)
        child=next((prefix+candidate for candidate in children.get(name.removeprefix(prefix),[])
                    if prefix+candidate in canonical and prefix+candidate in lookup),None)
        if child:
            old_vector=oldpos[canonical[child]]-oldpos[source_id]
            new_vector=newpos[lookup[child]]-newpos[lookup[name]]
            old_length=np.linalg.norm(old_vector);new_length=np.linalg.norm(new_vector)
            if old_length>.001 and new_length>.001:
                rotation=align(reference_rotation@old_vector,new_vector)@reference_rotation
                axis=old_vector/old_length;ratio=new_length/old_length
                scale+=(ratio-1)*np.outer(axis,axis);scales.append(float(ratio))
        transform=np.eye(4);transform[:3,:3]=rotation@scale
        transform[:3,3]=newpos[lookup[name]]-transform[:3,:3]@oldpos[source_id]
        transforms[name]=transform
    result=[transforms[new[mapping[n]]['name']] for n in range(len(old))]
    metadata={'method':'anatomical body axes and joint directions; ancestor helpers share transforms',
              'global_rotation':global_rotation.tolist(),'axial_scale_range':[min(scales),max(scales)]}
    return result,metadata

def map_name(name):
    if name.startswith('ValveBiped.'): return name
    n=name.lower().removeprefix('bip_')
    central={'root':'Pelvis','hips':'Pelvis','pelvis':'Pelvis', 'spine1':'Spine',
             'spine2':'Spine1','spine3':'Spine2','spine_base':'Spine','spine_mid':'Spine1','chest':'Spine2','spine_0':'Spine','spine_1':'Spine1',
             'spine_2':'Spine2','spine_3':'Spine4','neck':'Neck1','neck1':'Neck1','head':'Head1'}
    if n in central: return 'ValveBiped.Bip01_'+central[n]
    # Portal 2 rigs name forearm twist helpers lowArmL_twist0_N; they skin the forearm.
    twist=re.fullmatch(r'lowarm(l|r)_twist\d+_\d+',n)
    if twist: return 'ValveBiped.Bip01_'+twist[1].upper()+'_Forearm'
    match=re.search(r'_(l|r)$',n)
    if match:
        side=match[1].upper(); part=n[:-2]
        names={'thigh':'Thigh','hip':'Thigh','knee':'Calf','ankle':'Foot','foot':'Foot',
               'toebase':'Toe0','toe':'Toe0','clavicle':'Clavicle','collar':'Clavicle',
               'bicep':'UpperArm','upperarm':'UpperArm','elbow':'Forearm','lowerarm':'Forearm',
               'wrist':'Hand','hand':'Hand','twistarm':'Forearm'}
        if part in names: return 'ValveBiped.Bip01_'+side+'_'+names[part]
        part=part.replace('_base','_0').replace('_mid','_1').replace('_end','_2')
        finger=re.fullmatch(r'(thumb|index|mid|middle|ring|pinky)_(\d)',part)
        if finger:
            digit={'thumb':0,'index':1,'mid':2,'middle':2,'ring':3,'pinky':4}[finger[1]]
            segment=int(finger[2]); suffix='' if segment==0 else str(segment)
            return 'ValveBiped.Bip01_'+side+'_Finger'+str(digit)+suffix
    return None

def strip_header_stride(data, group, extended=False):
    """VTX7 exists with both 27- and 35-byte strip headers.

    Some Portal beta files have the extended fields without the marker used by
    our converter. Infer the layout by validating every strip and requiring
    complete coverage of the group's index buffer, rather than dropping later
    strips after reading their counts at the wrong offset.
    """
    strips=group+i(data,group+20);count=i(data,group+16)
    total=i(data,group+8);vertices=i(data,group)
    for stride in ((35,27) if extended else (27,35)):
        ranges=[];valid=True
        for number in range(count):
            at=strips+stride*number
            if at<0 or at+27>len(data):valid=False;break
            n,start,nv,first=struct.unpack_from('<4i',data,at)
            flags=data[at+18]
            if not (0<=start<=total-n and 0<=first<=vertices-nv and flags in (1,2)):
                valid=False;break
            if flags&1 and n%3:valid=False;break
            ranges.append((start,start+n))
        ranges.sort()
        if valid and ranges and ranges[0][0]==0 and ranges[-1][1]==total and all(a[1]==b[0] for a,b in zip(ranges,ranges[1:])):
            return stride
    raise ValueError('Unrecognized VTX strip layout or incomplete index coverage at '+str(group))

def export(source, target, output, alias, material_dir, eye_source=None):
    mdl=source.read_bytes(); vvd=source.with_suffix('.vvd').read_bytes()
    vtx=source.with_suffix('.dx90.vtx').read_bytes()
    old=bones(mdl); new=bones(target.read_bytes()); lookup={b['name']:n for n,b in enumerate(new)}
    mapping=[]; fallbacks=[]
    for k,b in enumerate(old):
        name=map_name(b['name'])
        if name not in lookup:
            ancestor=b['parent']
            while ancestor>=0 and map_name(old[ancestor]['name']) not in lookup: ancestor=old[ancestor]['parent']
            name=map_name(old[ancestor]['name']) if ancestor>=0 else 'ValveBiped.Bip01_Pelvis'
            fallbacks.append((b['name'],name))
        mapping.append(lookup[name])
    transforms,alignment=semantic_transforms(old,new,mapping)
    normal_transforms=[np.linalg.inv(transform[:3,:3]).T for transform in transforms]
    header=['version 1','nodes']+[f'{n} "{b["name"]}" {b["parent"]}' for n,b in enumerate(new)]+['end','skeleton','time 0']
    header += [str(n)+' '+' '.join(f'{v:.9f}' for v in (*b['pos'],*b['rot'])) for n,b in enumerate(new)]
    header += ['end']
    output.mkdir(parents=True,exist_ok=True)
    (output/'reference.smd').write_text('\n'.join(header)+'\n')
    materials=[s(mdl,i(mdl,208)+64*k+i(mdl,i(mdl,208)+64*k)) for k in range(i(mdl,204))]
    skin=1 if alias=='mel' and i(mdl,224)>1 else 0
    eyes=retarget_eyes(eye_source or source,old,new,mapping,transforms,skin)
    skinrefs=struct.unpack_from('<'+'H'*i(mdl,220),mdl,i(mdl,228)+skin*i(mdl,220)*2)
    lines=header+['triangles']; cache={}; triangles=0; vertexstart=i(vvd,56);layouts=[]
    # LOD0 fixups reconstruct the vertex order used by VTX.
    vertexids=[]
    for k in range(i(vvd,48)):
        lod,start,count=struct.unpack_from('<3i',vvd,i(vvd,52)+12*k)
        vertexids.extend(range(start,start+count))
    if not vertexids: vertexids=list(range(i(vvd,16)))
    def vertex(index):
        if index in cache: return cache[index]
        o=vertexstart+48*vertexids[index]
        weights=f(vvd,o,3); ids=vvd[o+12:o+15]; count=vvd[o+15]
        position=np.array((*f(vvd,o+16,3),1.0)); normal=np.array(f(vvd,o+28,3))
        p=np.zeros(4); n=np.zeros(3); remapped=collections.defaultdict(float)
        for j in range(count):
            p+=weights[j]*(transforms[ids[j]]@position)
            n+=weights[j]*(normal_transforms[ids[j]]@normal)
            remapped[mapping[ids[j]]]+=weights[j]
        n/=max(np.linalg.norm(n),1e-12)
        uv=f(vvd,o+40,2); total=sum(remapped.values())
        assert abs(total-1)<.001
        value='0 '+' '.join(f'{x:.8f}' for x in (*p[:3],*n,uv[0],1-uv[1]))+' '+str(len(remapped))
        value+=' '+ ' '.join(f'{bone} {weight/total:.9f}' for bone,weight in remapped.items())
        cache[index]=value
        return value
    for bp in range(i(mdl,232)):
        bo=i(mdl,236)+16*bp; vb=i(vtx,32)+8*bp
        # Preserve the first bodygroup option, never combine alternate meshes.
        mo=bo+i(mdl,bo+12); vm=vb+i(vtx,vb+4)
        if not i(mdl,mo+80): continue
        modelvert=i(mdl,mo+84)//48; lo=vm+i(vtx,vm+4)
        for mesh in range(i(mdl,mo+72)):
            mm=mo+i(mdl,mo+76)+116*mesh
            material=pathlib.PurePosixPath(materials[skinrefs[i(mdl,mm)]].replace('\\','/')).name
            me=lo+i(vtx,lo+4)+9*mesh; groups=me+i(vtx,me+4)
            extended=bool(vtx[me+8]&0x80); groupstride=33 if extended else 25
            for g in range(i(vtx,me)):
                go=groups+groupstride*g; vertices=go+i(vtx,go+4); indices=go+i(vtx,go+12)
                strips=go+i(vtx,go+20); stripstride=strip_header_stride(vtx,go,extended)
                layouts.append({'body':bp,'mesh':mesh,'group':g,'strip_header_bytes':stripstride,
                                'strips':i(vtx,go+16),'source_indices':i(vtx,go+8)})
                for st in range(i(vtx,go+16)):
                    so=strips+st*stripstride; count=i(vtx,so); start=i(vtx,so+4)
                    raw=struct.unpack_from('<'+'H'*count,vtx,indices+2*start)
                    triplets=[raw[k:k+3] for k in range(0,count,3)] if vtx[so+18]&1 else [(raw[k+1],raw[k],raw[k+2]) if k%2 else raw[k:k+3] for k in range(count-2)]
                    for tri in triplets:
                        if len(set(tri))<3: continue
                        lines.append(material)
                        # VTX triangle indices use the renderer's clockwise
                        # order. SMD triangles use the opposite order, and
                        # studiomdl reverses them when building VTX output.
                        for idx in (tri[0],tri[2],tri[1]):
                            original=struct.unpack_from('<H',vtx,vertices+idx*9+4)[0]
                            lines.append(vertex(modelvert+i(mdl,mm+12)+original))
                        triangles+=1
    lines+=['end']; (output/'mesh.smd').write_text('\n'.join(lines)+'\n')
    qc=f'''$modelname "gamma/player/{alias}.mdl"
{model_qc(eyes)}
$surfaceprop "flesh"
$contents "solid"
$cdmaterials "{material_dir}"
{ikchain_qc(target)}
$sequence "ragdoll" "reference.smd" activity ACT_DIERAGDOLL 1
$includemodel "gamma/player/{target.name}"
{('$includemodel "gamma/player/'+target.stem.replace('_anims','_neutral')+'.mdl"') if target.with_name(target.stem.replace('_anims','_neutral')+'.mdl').is_file() else ''}
$attachment "anim_attachment_RH" "ValveBiped.Bip01_R_Hand" 0 0 0 rotate 0 0 0
$attachment "anim_attachment_LH" "ValveBiped.Bip01_L_Hand" 0 0 0 rotate 0 0 0
{attachment_qc(eyes,new)}
'''
    (output/'model.qc').write_text(qc)
    (output/'eye_report.json').write_text(json.dumps(eyes,indent=2)+'\n')
    report=dict(alias=alias,source=str(source),triangles=triangles,vertices=len(cache),source_bones=len(old),target_bones=len(new),ancestor_remaps=fallbacks,alignment=alignment,vtx_layouts=layouts,
                source_sha256={'.mdl':hashlib.sha256(mdl).hexdigest(),'.vvd':hashlib.sha256(vvd).hexdigest(),'.dx90.vtx':hashlib.sha256(vtx).hexdigest()})
    (output/'retarget_report.json').write_text(json.dumps(report,indent=2));print(json.dumps(report))

if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('source',type=pathlib.Path);ap.add_argument('target',type=pathlib.Path);ap.add_argument('output',type=pathlib.Path);ap.add_argument('alias');ap.add_argument('material_dir');ap.add_argument('--eyes-source',type=pathlib.Path)
    args=ap.parse_args();export(args.source,args.target,args.output,args.alias,args.material_dir,args.eyes_source)
