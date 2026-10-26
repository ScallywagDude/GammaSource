#!/usr/bin/env python3
"""Decode actual HUD VTF pixels and audit weapon-script atlas rectangles.

Reads unpacked, branch-specific local assets. No downloads or source-asset edits.
Diagnostic contact sheets must be outside the distributable source directory.
This checks pixels/rectangles, not artwork identity, model matching or game scale.
"""
from __future__ import annotations
import argparse
from collections import Counter
import hashlib
import io
import json
import math
from pathlib import Path
import struct

from PIL import Image, ImageDraw, ImageFont
from asset_validation_v18 import AssetIndex, relative_asset
from weapon_asset_audit import kv_parse, tokens

ROOT = Path(__file__).resolve().parents[1]
FORMAT_NAMES = {0:'RGBA8888',1:'ABGR8888',2:'RGB888',3:'BGR888',4:'RGB565',5:'I8',6:'IA88',
    7:'P8',8:'A8',9:'RGB888_BLUESCREEN',10:'BGR888_BLUESCREEN',11:'ARGB8888',12:'BGRA8888',
    13:'DXT1',14:'DXT3',15:'DXT5',16:'BGRX8888',17:'BGR565',18:'BGRX5551',19:'BGRA4444',
    20:'DXT1_ONEBITALPHA',21:'BGRA5551',22:'UV88',23:'UVWQ8888',24:'RGBA16161616F',25:'RGBA16161616'}
BPP = {0:4,1:4,2:3,3:3,5:1,6:2,8:1,9:3,10:3,11:4,12:4,16:4,17:2,18:2,19:2,21:2,25:8}


class PixelDecodeError(ValueError):
    pass


def image_size(width, height, image_format):
    if width < 1 or height < 1:
        raise PixelDecodeError('nonpositive image dimensions')
    if image_format in (13,20):
        return ((width+3)//4)*((height+3)//4)*8
    if image_format in (14,15):
        return ((width+3)//4)*((height+3)//4)*16
    if image_format not in BPP:
        raise PixelDecodeError('unsupported pixel format: '+FORMAT_NAMES.get(image_format,str(image_format)))
    return width*height*BPP[image_format]


def rgb565(value):
    r,g,b=(value>>11)&31,(value>>5)&63,value&31
    return ((r<<3)|(r>>2),(g<<2)|(g>>4),(b<<3)|(b>>2))


def decode_dxt(payload, width, height, image_format):
    block_bytes=8 if image_format in (13,20) else 16
    output=bytearray(width*height*4)
    offset=0
    for by in range(0,height,4):
        for bx in range(0,width,4):
            block=payload[offset:offset+block_bytes]; offset+=block_bytes
            color_offset=0 if block_bytes==8 else 8
            c0,c1,indices=struct.unpack_from('<HHI',block,color_offset)
            a,b=rgb565(c0),rgb565(c1)
            palette=[a+(255,),b+(255,)]
            if c0>c1 or image_format in (14,15):
                palette.extend([tuple((2*a[i]+b[i])//3 for i in range(3))+(255,),
                                tuple((a[i]+2*b[i])//3 for i in range(3))+(255,)])
            else:
                palette.extend([tuple((a[i]+b[i])//2 for i in range(3))+(255,),(0,0,0,0)])
            alpha_bits=0; alpha_palette=None
            if image_format==14:
                alpha_bits=int.from_bytes(block[:8],'little')
            elif image_format==15:
                a0,a1=block[0],block[1]
                if a0>a1:
                    alpha_palette=[a0,a1]+[((7-i)*a0+i*a1)//7 for i in range(1,7)]
                else:
                    alpha_palette=[a0,a1]+[((5-i)*a0+i*a1)//5 for i in range(1,5)]+[0,255]
                alpha_bits=int.from_bytes(block[2:8],'little')
            for pixel in range(16):
                x,y=bx+pixel%4,by+pixel//4
                if x>=width or y>=height: continue
                color=palette[(indices>>(2*pixel))&3]
                alpha=color[3]
                if image_format==14: alpha=((alpha_bits>>(4*pixel))&15)*17
                elif image_format==15: alpha=alpha_palette[(alpha_bits>>(3*pixel))&7]
                at=(y*width+x)*4
                output[at:at+4]=bytes(color[:3]+(alpha,))
    return Image.frombytes('RGBA',(width,height),bytes(output))


def decode_pixels(payload, width, height, image_format):
    needed=image_size(width,height,image_format)
    if len(payload)!=needed:
        raise PixelDecodeError(f'pixel payload size {len(payload)} does not match {needed}')
    if image_format in (13,14,15,20):
        return decode_dxt(payload,width,height,image_format)
    if image_format in (0,1,2,3,11,12,16):
        modes={0:'RGBA',1:'ABGR',2:'RGB',3:'BGR',11:'ARGB',12:'BGRA',16:'BGRX'}
        mode=modes[image_format]
        output_mode='RGB' if image_format in (2,3,16) else 'RGBA'
        return Image.frombytes(output_mode,(width,height),payload,'raw',mode).convert('RGBA')
    out=bytearray(width*height*4)
    for i in range(width*height):
        if image_format in (17,18,19,21):
            value=struct.unpack_from('<H',payload,i*2)[0]
            if image_format==17:
                r,g,b=rgb565(value)
                color=(r,g,b,255)
            elif image_format==19:
                color=(((value>>8)&15)*17,((value>>4)&15)*17,(value&15)*17,((value>>12)&15)*17)
            else:
                r,g,b=(value>>10)&31,(value>>5)&31,value&31
                color=((r<<3)|(r>>2),(g<<3)|(g>>2),(b<<3)|(b>>2),255 if image_format==18 else (255 if value&0x8000 else 0))
        elif image_format==5:
            v=payload[i];color=(v,v,v,255)
        elif image_format==6:
            v,a=payload[i*2:i*2+2];color=(v,v,v,a)
        elif image_format==8:
            color=(255,255,255,payload[i])
        elif image_format in (9,10):
            r,g,b=payload[i*3:i*3+3]
            if image_format==10:r,b=b,r
            color=(r,g,b,0 if (r,g,b)==(0,0,255) else 255)
        elif image_format==25:
            color=tuple(round(v*255/65535) for v in struct.unpack_from('<4H',payload,i*8))
        else:
            raise PixelDecodeError('unsupported uncompressed image format')
        out[i*4:i*4+4]=bytes(color)
    return Image.frombytes('RGBA',(width,height),bytes(out))


def dds_reference_check(payload,width,height,image_format,image):
    """Cross-check top-level BC pixels with Pillow's independent DDS decoder."""
    fourcc={13:b'DXT1',20:b'DXT1',14:b'DXT3',15:b'DXT5'}[image_format]
    header=b'DDS '+struct.pack('<7I',124,0x81007,height,width,len(payload),0,1)+bytes(44)
    header+=struct.pack('<II4s5I',32,4,fourcc,0,0,0,0,0)+struct.pack('<5I',0x1000,0,0,0,0)
    reference=Image.open(io.BytesIO(header+payload)).convert('RGBA')
    actual,expected=image.tobytes(),reference.tobytes()
    differences=0
    for at in range(0,len(actual),4):
        # RGB beneath alpha zero is not rendered and is codec-implementation-specific.
        if actual[at+3]!=expected[at+3] or (actual[at+3] and actual[at:at+3]!=expected[at:at+3]):
            differences+=1
    return {'decoder':'Pillow DDS/BCn','visible_pixel_differences':differences,
        'status':'matches' if differences==0 else 'decoder_rounding_or_color_difference'}


def decode_vtf(data):
    """Decode every stored mip/frame of a non-cubemap VTF 7.0/7.1 HUD image."""
    if len(data)<64 or data[:4]!=b'VTF\0':raise PixelDecodeError('invalid or truncated VTF header')
    major,minor,header=struct.unpack_from('<III',data,4)
    if major!=7 or minor not in (0,1):raise PixelDecodeError(f'unsupported VTF version {major}.{minor}; only 7.0/7.1 decoded')
    width,height=struct.unpack_from('<HH',data,16)
    flags=struct.unpack_from('<I',data,20)[0]
    frames,start_frame=struct.unpack_from('<HH',data,24)
    fmt=struct.unpack_from('<i',data,52)[0]
    mip_count=data[56]
    low_fmt=struct.unpack_from('<i',data,57)[0]
    low_w,low_h=data[61:63]
    if flags&0x4000:raise PixelDecodeError('HUD cubemap is unsupported; faces are not flattened')
    if not 64<=header<=len(data) or not 1<=width<=8192 or not 1<=height<=8192:
        raise PixelDecodeError('invalid header length or image dimensions')
    if not 1<=frames<=256 or not 1<=mip_count<=int(math.log2(max(width,height)))+1:
        raise PixelDecodeError('invalid frame or mip count')
    if sum(max(1,width>>m)*max(1,height>>m) for m in range(mip_count))*frames>64000000:
        raise PixelDecodeError('HUD decode pixel budget exceeded')
    at=header
    thumbnail_decoded=False
    if low_w and low_h:
        size=image_size(low_w,low_h,low_fmt)
        decode_pixels(data[at:at+size],low_w,low_h,low_fmt)
        at+=size;thumbnail_decoded=True
    levels=[];top_frames=[];reference_checks=[]
    for mip in range(mip_count-1,-1,-1):
        w,h=max(1,width>>mip),max(1,height>>mip)
        size=image_size(w,h,fmt)
        for frame in range(frames):
            image=decode_pixels(data[at:at+size],w,h,fmt)
            levels.append({'mip':mip,'frame':frame,'dimensions':[w,h],'byte_offset':at,'bytes':size})
            if mip==0:
                top_frames.append(image)
                if fmt in (13,14,15,20):
                    reference_checks.append(dict(frame=frame,**dds_reference_check(data[at:at+size],w,h,fmt,image)))
            at+=size
    metadata={'status':'pixels_decoded','version':[major,minor],'dimensions':[width,height],
        'image_format':fmt,'format_name':FORMAT_NAMES.get(fmt,str(fmt)),'flags':flags,
        'frames':frames,'start_frame':start_frame,'mip_count':mip_count,
        'thumbnail_pixels_decoded':thumbnail_decoded,'decoded_subimages':len(levels),
        'decoded_image_bytes':at-header,'trailing_bytes':len(data)-at,'levels':levels,
        'independent_top_frame_checks':reference_checks,
        'sha256':hashlib.sha256(data).hexdigest()}
    return metadata,top_frames


def truth(value):
    return str(value).strip().casefold() not in ('','0','0.0','false','no')


def select_sprite_spec(specification):
    """Mirror LoadHudTextures: direct file, then legacy 640, then 320."""
    if 'font' in specification:
        return specification,'font_role_source_unsupported'
    if specification.get('file'):
        return specification,'direct'
    for resolution in ('640','320'):
        child=specification.get(resolution)
        if isinstance(child,dict) and child.get('file'):
            return child,('font_legacy_resolution_'+resolution+'_source_unsupported' if 'font' in child else 'legacy_resolution_'+resolution)
    return specification,'unconfigured_or_source_unsupported'


def sprite_material_name(name, legacy_resolution=False):
    if not isinstance(name,str) or not name:return None
    leaf=not any(c in name for c in ('/','\\',':'))
    normalized='sprites/'+name if legacy_resolution and leaf else name
    return normalized if len(normalized)<64 else None


def material_data(index, name, stack=()):
    ref=index.resolve(name,'materials','.vmt');path=index.disk_path(ref)
    if path is None:return ref,None
    identity=ref['path'].casefold()
    if identity in stack:return dict(ref,status='cyclic_material_include'),None
    try:
        text=path.read_text(encoding='latin1');doc=kv_parse(text)
        shader=next(iter(tokens(text)),'')
        if shader.casefold()=='patch':
            _,base=material_data(index,doc.get('include',''),stack+(identity,))
            if base is None:return dict(ref,status='unresolved_material_include'),None
            merged=dict(base)
            for action in ('insert','replace'):
                if isinstance(doc.get(action),dict):merged.update(doc[action])
            doc=merged
        ref['shader']=shader
        ref['sampling_parameters']={k:v for k,v in doc.items() if k in
            ('$additive','$translucent','$alphatest','$alphatestreference','$alpha','$vertexalpha','$vertexcolor','$basetexturetransform')}
        return ref,doc
    except (OSError,ValueError) as exc:return dict(ref,status='invalid_material',error=str(exc)),None


def pixel_metrics(image, additive=False, alpha_test=False, alpha_reference=0.5, alpha_scale=1.0):
    raw=image.tobytes();mask=bytearray(image.width*image.height)
    alpha_pixels=0;color_pixels=0
    threshold=max(0,min(255,round(alpha_reference*255))) if alpha_test else 0
    for i in range(image.width*image.height):
        r,g,b,a=raw[i*4:i*4+4]
        a=max(0,min(255,round(a*alpha_scale)))
        if a>0:alpha_pixels+=1
        if a>0 and max(r,g,b)>2:color_pixels+=1
        visible=a>=threshold if alpha_test else a>0
        if additive:visible=visible and max(r,g,b)>2
        if visible:mask[i]=255
    visible=Image.frombytes('L',image.size,bytes(mask))
    bbox=visible.getbbox();count=sum(1 for value in mask if value)
    touches=bool(bbox and (bbox[0]==0 or bbox[1]==0 or bbox[2]==image.width or bbox[3]==image.height))
    return {'pixels':image.width*image.height,'nonzero_alpha_pixels':alpha_pixels,
        'nonblack_alpha_pixels':color_pixels,'effective_nonblank_pixels':count,
        'effective_nonblank_fraction':round(count/(image.width*image.height),6),
        'effective_content_bounds':list(bbox) if bbox else None,
        'content_touches_rectangle_boundary':touches,
        'uniform_rgba':len(set(zip(*(iter(raw),)*4)))==1}


def fit_dimensions(width,height,max_width,max_height):
    scale=min(1.0,max_width/width,max_height/height)
    return max(1,int(width*scale)),max(1,int(height*scale))


def checkerboard(size):
    canvas=Image.new('RGBA',size,(34,37,40,255));draw=ImageDraw.Draw(canvas)
    for y in range(0,size[1],12):
        for x in range(0,size[0],12):
            if (x//12+y//12)%2:draw.rectangle((x,y,x+11,y+11),fill=(52,55,58,255))
    return canvas


def contact_sheets(items, output, prefix, atlas=False):
    if not items:return []
    output.mkdir(parents=True,exist_ok=True)
    columns,rows=(3,3) if atlas else (5,6)
    cell_w,cell_h=(344,304) if atlas else (248,134)
    paths=[];font=ImageFont.load_default()
    for page in range(0,len(items),columns*rows):
        selected=items[page:page+columns*rows]
        canvas=Image.new('RGBA',(columns*cell_w,math.ceil(len(selected)/columns)*cell_h),(20,23,26,255))
        draw=ImageDraw.Draw(canvas)
        for number,(label,image) in enumerate(selected):
            x=(number%columns)*cell_w;y=(number//columns)*cell_h
            panel=checkerboard((cell_w-12,cell_h-42))
            if image is not None:
                size=fit_dimensions(*image.size,panel.width-8,panel.height-8)
                thumb=image.resize(size,Image.Resampling.NEAREST)
                panel.alpha_composite(thumb,((panel.width-size[0])//2,(panel.height-size[1])//2))
            canvas.alpha_composite(panel,(x+6,y+4))
            draw.multiline_text((x+7,y+cell_h-34),label,font=font,fill=(232,234,236,255),spacing=2)
        file=output/f'{prefix}_{page//(columns*rows)+1:02d}.png'
        canvas.convert('RGB').save(file)
        paths.append(str(file.resolve()))
    return paths


def audit_branch(branch, roots, image_dir=None, registered_classnames=None):
    index=AssetIndex(roots)
    if registered_classnames is None:
        inventory=json.loads((ROOT/'modernize/weapon_branch_inventory.json').read_text(encoding='utf-8'))
        registered_classnames={entry['classname'] for entry in inventory['branches'].get(branch,[])}
    scripts=set()
    for catalog in index.files:
        for paths in catalog.values():
            scripts.update(path for path in paths if path.casefold().startswith('scripts/weapon_') and
                path.casefold().endswith('.txt') and Path(path).stem.casefold()!='weapon_manifest')
    # Resolve once in search-path order, even where a later root also has a script.
    scripts=sorted({p.casefold() for p in scripts})
    textures={};decoded={};weapon_rows=[];icon_images=[]
    for relative in scripts:
        script=index.resolve(relative);path=index.disk_path(script)
        row={'classname':Path(relative).stem,'script':script,'icons':[],
            'source_registered':Path(relative).stem in registered_classnames,
            'source_membership':'registered_in_active_branch_source' if Path(relative).stem in registered_classnames else 'script_only_or_unregistered_in_active_branch_source',
            'image_to_model_match':'unverified_no_compatible_rendered_model',
            'runtime_hud_scale_verified':False}
        try:doc=kv_parse(path.read_text(encoding='latin1'))
        except (OSError,ValueError) as exc:
            row.update(status='invalid_weapon_script',error=str(exc));weapon_rows.append(row);continue
        specifications=doc.get('texturedata',{})
        if not isinstance(specifications,dict):specifications={}
        row['status']='audited' if specifications else 'no_hud_texturedata'
        for role,spec in sorted(specifications.items()):
            icon={'role':role,'status':'invalid_sprite_data'}
            row['icons'].append(icon)
            if not isinstance(spec,dict):continue
            original_spec=spec
            spec,selection=select_sprite_spec(spec)
            icon['configuration_selection']=selection
            if selection.startswith('legacy_resolution_'):
                icon['source_parser_status']='legacy_resolution_parser_restored_runtime_unverified'
            if selection.startswith('font_'):
                icon.update(status='font_glyph_unverified_source_unsupported',font=spec.get('font'),character=spec.get('character'));continue
            if 'file' not in spec and not any(key in spec for key in ('x','y','width','height')):
                icon['status']='unconfigured_icon';continue
            normalized=sprite_material_name(spec.get('file',''),selection.startswith('legacy_resolution_'))
            icon['source_material_name']=spec.get('file','')
            icon['normalized_material_name']=normalized
            if normalized is None:
                icon['status']='invalid_material_name';continue
            try:x,y,w,h=(int(spec.get(k,0)) for k in ('x','y','width','height'))
            except (ValueError,TypeError):icon['status']='invalid_rectangle';continue
            icon['rectangle']=[x,y,w,h]
            if min(x,y)<0 or min(w,h)<1 or x>8192 or y>8192 or w>8192-x or h>8192-y:
                icon['status']='invalid_rectangle';continue
            icon['source_aspect_ratio']=round(w/h,6)
            material,parameters=material_data(index,normalized)
            icon['material']=material
            if parameters is None:icon['status']='material_unavailable';continue
            texture=index.resolve(parameters.get('$basetexture',''),'materials','.vtf')
            texture_path=index.disk_path(texture)
            texture_key=texture.get('path')
            icon['texture']=texture
            if texture_path is None:icon['status']='texture_unavailable';continue
            if texture_key not in textures:
                record=dict(texture)
                try:
                    if texture_path.stat().st_size>128*1024*1024:raise PixelDecodeError('VTF file budget exceeded')
                    metadata,frames=decode_vtf(texture_path.read_bytes())
                    record['content']=metadata;decoded[texture_key]=frames
                except (OSError,ValueError,struct.error) as exc:
                    record['content']={'status':'pixels_not_decoded','error':str(exc)}
                textures[texture_key]=record
            if texture_key not in decoded:icon['status']='pixels_not_decoded';continue
            frames=decoded[texture_key];tw,th=frames[0].size
            icon['texture_dimensions']=[tw,th]
            if x+w>tw or y+h>th:
                icon['status']='outside_texture_bounds';continue
            additive=truth(parameters.get('$additive','0'))
            alphatest=truth(parameters.get('$alphatest','0'))
            try:
                alpha_reference=float(parameters.get('$alphatestreference','0.5'))
                alpha_scale=float(parameters.get('$alpha','1'))
                if not math.isfinite(alpha_reference) or not math.isfinite(alpha_scale):raise ValueError('nonfinite alpha')
            except (ValueError,TypeError):
                icon['status']='invalid_material_alpha';continue
            crops=[frame.crop((x,y,x+w,y+h)) for frame in frames]
            metrics=[pixel_metrics(crop,additive,alphatest,alpha_reference,alpha_scale) for crop in crops]
            visible=sum(m['effective_nonblank_pixels']>0 for m in metrics)
            icon.update(status='pixels_verified_nonblank' if visible==len(frames) else
                ('some_blank_animation_frames' if visible else 'blank_icon_pixels'),
                decoded_frames=len(frames),nonblank_frames=visible,frames=metrics,
                material_visibility_rule='additive_rgb_and_alpha' if additive else ('alpha_test' if alphatest else 'alpha'),
                content_identity_verified=False)
            initial=textures[texture_key]['content']['start_frame']
            if initial>=len(crops):initial=0
            membership='' if row['source_registered'] else ' [script only]'
            icon_images.append((f'{row["classname"]}{membership} / {role}\n{w}x{h}  {icon["status"]}',crops[initial]))
        weapon_rows.append(row)
    sheets=[]
    if image_dir is not None:
        sheets+=contact_sheets(icon_images,image_dir,branch+'_weapon_icons')
        atlas_items=[]
        for key,frames in sorted(decoded.items()):
            name=Path(key).stem;meta=textures[key]['content']
            atlas_items.append((f'{name}  {frames[0].width}x{frames[0].height}\nVTF {meta["version"][0]}.{meta["version"][1]} {meta["format_name"]}',frames[0]))
        sheets+=contact_sheets(atlas_items,image_dir,branch+'_atlases',atlas=True)
    counts=Counter(icon['status'] for weapon in weapon_rows for icon in weapon['icons'])
    return {'branch':branch,'asset_search_roots':[str(p) for p in index.roots],
        'status':'audited' if index.roots else 'unverified_no_asset_directory',
        'scripts_inspected':len(weapon_rows),'texture_count':len(textures),
        'source_registered_class_count':len(registered_classnames),
        'registered_scripts_inspected':sum(row['source_registered'] for row in weapon_rows),
        'script_only_entries':[row['classname'] for row in weapon_rows if not row['source_registered']],
        'registered_classes_without_scripts':sorted(registered_classnames-{row['classname'] for row in weapon_rows}),
        'textures_pixels_decoded':len(decoded),'icon_status_counts':dict(sorted(counts.items())),
        'textures':list(textures.values()),'weapons':weapon_rows,'local_contact_sheets':sheets}


def report_markdown(report):
    lines=['# v18 HUD pixel audit','',
        'Actual local VTF pixel data was decoded without changing source assets. All stored frames and mip levels of supported non-cubemap VTF 7.0/7.1 textures were decoded; unsupported formats remain explicit. Rectangles were checked against decoded dimensions and tested for effective nonblank pixels using the material alpha/additive rule.','',
        'Contact sheets preserve icon aspect ratio and use nearest-neighbor pixels over a checkerboard. They are diagnostic derivatives outside the source archive. No placeholder icons or replacement artwork were generated.','',
        'The sprite parser mirrors the final restored source behavior: a role with a `font` key is unsupported; a nonempty direct `file` wins unchanged; otherwise a nonempty legacy child `640` file is preferred over `320`. A selected child with a `font` key is also unsupported. Only selected legacy leaf-only file names are prefixed with `sprites/`; slash/backslash/colon-qualified and all direct paths remain unchanged. Nonempty invalid chosen metadata is reported without silently selecting a fallback. Chosen legacy blocks are recorded and remain runtime-unverified. Font entries are explicitly unverified, not certified as decoded icons.','',
        '**Limits:** nonblank pixels do not prove the artwork depicts the correct weapon. Image-to-model matching, semantic icon identity, VGUI runtime scale, blending/gamma on the GPU, animation appearance and actual game rendering remain unverified. Content touching a crop boundary is recorded as a review hint, not proof of cropping. Uniform colors are recorded without declaring them an artwork defect.','']
    for branch,data in report['branches'].items():
        lines.extend([f'## {branch}','',f'Scripts inspected: {data["scripts_inspected"]}; source-registered scripts among these: {data["registered_scripts_inspected"]}; source-registered classes in inventory: {data["source_registered_class_count"]}; referenced textures: {data["texture_count"]}; whole textures decoded: {data["textures_pixels_decoded"]}.','',
            'Scripts are asset evidence and do not establish a supported source weapon. Script-only entries: '+(', '.join('`'+name+'`' for name in data['script_only_entries']) or 'none')+'.','',
            '| Pixel/rectangle state | Icons |','|---|---:|'])
        lines.extend(f'| {state} | {count} |' for state,count in data['icon_status_counts'].items())
        lines.extend(['','| Weapon | Source registration | Role | Configuration | Rectangle | Result |','|---|---|---|---|---|---|'])
        for weapon in data['weapons']:
            for icon in weapon['icons']:
                lines.append(f'| {weapon["classname"]} | {"registered" if weapon["source_registered"] else "script only"} | {icon["role"]} | {icon.get("configuration_selection","")} | {icon.get("rectangle","")} | {icon["status"]} |')
        if data['local_contact_sheets']:
            lines.extend(['','Local diagnostic sheets (excluded from the source ZIP):',''])
            lines.extend('- `'+p+'`' for p in data['local_contact_sheets'])
        lines.append('')
    return '\n'.join(lines)+'\n'


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--assets-hl2',action='append',type=Path)
    parser.add_argument('--assets-hl1',action='append',type=Path)
    parser.add_argument('--image-dir',type=Path,default=ROOT.parent/'asset_visual_audit_v18')
    parser.add_argument('--no-images',action='store_true')
    parser.add_argument('--out-json',type=Path,default=ROOT/'modernize/reports/HUD_PIXEL_AUDIT_V18.json')
    args=parser.parse_args(argv)
    image_dir=args.image_dir.resolve()
    if not args.no_images and (image_dir==ROOT or ROOT in image_dir.parents):
        parser.error('Contact sheets must stay outside the distributable source tree')
    branches={}
    for branch in ('hl2','hl1'):
        supplied=getattr(args,'assets_'+branch)
        roots=supplied if supplied is not None else ([ROOT.parent/'hl2_2003',ROOT.parent/'hl2'] if branch=='hl2' else [ROOT.parent/'hl1'])
        roots=[p for p in roots if p.is_dir()]
        branches[branch]=audit_branch(branch,roots,None if args.no_images else image_dir)
        print(branch+': '+json.dumps({k:branches[branch][k] for k in ('scripts_inspected','texture_count','textures_pixels_decoded','icon_status_counts')}),flush=True)
    report={'schema':1,'runtime_verified':False,'model_artwork_matching_verified':False,
        'source_assets_modified':False,'proprietary_images_packaged':False,
        'codec_scope':'Explicit DXT1/DXT3/DXT5 and relevant uncompressed formats; VTF 7.0/7.1 only; all stored mip/frame pixels decoded',
        'branches':branches}
    args.out_json.parent.mkdir(parents=True,exist_ok=True)
    args.out_json.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    args.out_json.with_suffix('.md').write_text(report_markdown(report),encoding='utf-8')
    return 0


if __name__=='__main__':
    raise SystemExit(main())
