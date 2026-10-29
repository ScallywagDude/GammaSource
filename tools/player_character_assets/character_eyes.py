"""Preserve authored eye projection metadata across character rig conversion."""
import math
from pathlib import Path
import struct

import numpy as np


def integer(data,at):return struct.unpack_from('<i',data,at)[0]
def string(data,at):return data[at:data.index(b'\0',at)].decode('latin1')
def floats(data,at,count):return np.array(struct.unpack_from('<'+'f'*count,data,at))


def source_eyes(path,skin=0):
    data=Path(path).read_bytes();version=integer(data,4);legacy=version in (36,37)
    def header(offset):return integer(data,offset+(16 if version==37 and offset>=188 else 0))
    bone_stride,inverse_at=(196,80) if legacy else (216,96)
    bones=[]
    for n in range(header(156)):
        at=header(160)+bone_stride*n;inverse=np.eye(4);inverse[:3]=floats(data,at+inverse_at,12).reshape(3,4)
        bones.append((string(data,at+integer(data,at)),np.linalg.inv(inverse)))
    count_at,index_at=(208,212) if legacy else (204,208)
    texture_stride=32 if legacy else 64
    textures=[]
    for n in range(header(count_at)):
        at=header(index_at)+texture_stride*n;textures.append(string(data,at+integer(data,at)))
    refs_at,families_at,skins_at=(224,228,232) if legacy else (220,224,228)
    skin=min(skin,header(families_at)-1)
    skinrefs=struct.unpack_from('<'+'H'*header(refs_at),data,header(skins_at)+skin*header(refs_at)*2)
    body_count,body_at=(236,240) if legacy else (232,236)
    model_stride,mesh_stride,eye_stride=(140,68,140) if legacy else (148,116,172)
    result=[]
    for part in range(header(body_count)):
        body=header(body_at)+16*part
        # This importer retains the first option of each bodygroup.
        model=body+integer(data,body+12)
        if not integer(data,model+80):continue
        eye_materials={}
        for n in range(integer(data,model+72)):
            mesh=model+integer(data,model+76)+mesh_stride*n
            if integer(data,mesh+24)==1:
                eye_materials[integer(data,mesh+28)]=Path(textures[skinrefs[integer(data,mesh)]].replace('\\','/')).name
        for n in range(integer(data,model+100)):
            at=model+integer(data,model+104)+eye_stride*n
            assert n in eye_materials,('Eye without matching mesh',path,n)
            bone=integer(data,at+4);name,bind=bones[bone]
            origin=(bind@np.array([*floats(data,at+8,3),1.]))[:3]
            up=bind[:3,:3]@floats(data,at+28,3)
            forward=bind[:3,:3]@floats(data,at+40,3)
            result.append({'bone':name,'material':eye_materials[n],
                           'origin':origin,'up':up,'forward':forward,
                           'radius':float(floats(data,at+24,1)[0]),
                           'zoffset':float(floats(data,at+20,1)[0]),
                           'iris_scale':float(floats(data,at+60,1)[0])})
    return result


def retarget_eyes(source,old,new,mapping,transforms,skin=0):
    by_name={b['name']:n for n,b in enumerate(old)};result=[]
    for eye in source_eyes(source,skin):
        number=by_name[eye['bone']];transform=transforms[number]
        origin=(transform@np.array([*eye['origin'],1.]))[:3]
        up=transform[:3,:3]@eye['up'];up/=np.linalg.norm(up)
        forward=transform[:3,:3]@eye['forward'];forward/=np.linalg.norm(forward)
        result.append({'name':eye['material'],'bone':new[mapping[number]]['name'],
                       'material':eye['material'],'origin':origin.tolist(),'up':up.tolist(),'forward':forward.tolist(),
                       'radius':eye['radius'],'zoffset':eye['zoffset'],'iris_scale':eye['iris_scale']})
    return result


def model_qc(eyes):
    if not eyes:return '$body "body" "mesh.smd"'
    lines=['$model "body" "mesh.smd" {']
    for eye in eyes:
        assert eye['radius']>0 and eye['iris_scale']>0
        center=' '.join(f'{value:.9f}' for value in eye['origin'])
        lines.append(f'\teyeball "{eye["name"]}" "{eye["bone"]}" {center} "{eye["material"]}" '
                     f'{2*eye["radius"]:.9f} {math.degrees(math.atan(eye["zoffset"])):.9f} "unused" {1/eye["iris_scale"]:.9f}')
    lines.append('}')
    return '\n'.join(lines)


def attachment_frame(eyes,bone_inverse):
    center=np.mean([eye['origin'] for eye in eyes],axis=0)
    up=np.mean([eye['up'] for eye in eyes],axis=0);up/=np.linalg.norm(up)
    right=next(eye for eye in eyes if eye['material'].lower().endswith('_r'))
    left=next(eye for eye in eyes if eye['material'].lower().endswith('_l'))
    rightward=np.array(right['origin'])-np.array(left['origin'])
    rightward-=up*(rightward@up);rightward/=np.linalg.norm(rightward)
    forward=np.cross(up,rightward);forward/=np.linalg.norm(forward)
    leftward=np.cross(up,forward)
    world=np.eye(4);world[:3,:3]=np.column_stack((forward,leftward,up));world[:3,3]=center
    return bone_inverse@world


def attachment_qc(eyes,target_bones):
    if not eyes:return '$attachment "eyes" "ValveBiped.Bip01_Head1" 3.5 -3 0 rotate 0 -90 -90'
    bone=next(b for b in target_bones if b['name']==eyes[0]['bone'])
    local=attachment_frame(eyes,bone['inverse'])
    yaw=math.atan2(local[1,0],local[0,0])
    pitch=math.atan2(-local[2,0],math.hypot(local[0,0],local[1,0]))
    roll=math.atan2(local[2,1],local[2,2])
    origin=' '.join(f'{value:.9f}' for value in local[:3,3])
    angles=' '.join(f'{math.degrees(value):.9f}' for value in (pitch,yaw,roll))
    return f'$attachment "eyes" "{bone["name"]}" {origin} rotate {angles}'


def restore_compiled_eye_frames(path,eyes):
    """QC preserves centers, radii and iris size; restore the authored look axes.

    Stock QC derives eye forward from model +X, while this shared rig's bind
    pose faces -Y. Re-express the transformed original directions in the actual
    compiled head frame. The runtime uses these vectors for iris projection.
    """
    path=Path(path);data=bytearray(path.read_bytes());bone_lookup={}
    for n in range(integer(data,156)):
        at=integer(data,160)+216*n;inverse=np.eye(4);inverse[:3]=floats(data,at+96,12).reshape(3,4)
        bone_lookup[string(data,at+integer(data,at))]=(n,inverse)
    body=integer(data,236);model=body+integer(data,body+12)
    assert integer(data,model+100)==len(eyes),(path,integer(data,model+100),len(eyes))
    for n,eye in enumerate(eyes):
        at=model+integer(data,model+104)+172*n;bone,inverse=bone_lookup[eye['bone']]
        struct.pack_into('<i',data,at+4,bone)
        origin=(inverse@np.array([*eye['origin'],1.]))[:3]
        up=inverse[:3,:3]@np.array(eye['up']);up/=np.linalg.norm(up)
        forward=inverse[:3,:3]@np.array(eye['forward']);forward/=np.linalg.norm(forward)
        struct.pack_into('<3f',data,at+8,*origin)
        struct.pack_into('<3f',data,at+28,*up)
        struct.pack_into('<3f',data,at+40,*forward)
    if eyes:
        bone,inverse=bone_lookup[eyes[0]['bone']]
        local=attachment_frame(eyes,inverse)
        for n in range(integer(data,240)):
            at=integer(data,244)+92*n
            if string(data,at+integer(data,at))=='eyes':
                struct.pack_into('<i',data,at+8,bone)
                struct.pack_into('<12f',data,at+12,*local[:3].reshape(-1))
    path.write_bytes(data)


if __name__=='__main__':
    import argparse,json
    parser=argparse.ArgumentParser(description='Restore authored iris axes after compiling a player QC')
    parser.add_argument('model',type=Path);parser.add_argument('eye_report',type=Path)
    args=parser.parse_args()
    restore_compiled_eye_frames(args.model,json.loads(args.eye_report.read_text()))
    print('Restored authored eye projection frames in',args.model)
