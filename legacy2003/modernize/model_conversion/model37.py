"""Conservative offline conversion of local, embedded-animation MDL37 to MDL36.

No input file is modified. The full payload retains its original byte positions;
the real v36 header and expanded/rebased v36 sequence table replace the v37 views.
Every output must pass the unchanged native engine StudioDataValidator and a deep
VTX6 walk before it is admitted to the isolated overlay.
"""
import argparse,datetime,hashlib,json,math,struct,subprocess,sys
from pathlib import Path
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
LIMIT=64*1024*1024
REFERENCES=[
 'https://github.com/ZeqMacaw/Crowbar/blob/master/Crowbar/Core/GameModel/SourceModel37/SourceMdlFileData/SourceMdlFileData37.vb',
 'https://github.com/ZeqMacaw/Crowbar/blob/master/Crowbar/Core/GameModel/SourceModel37/SourceMdlFileData/SourceMdlSequenceDesc37.vb']
DEFAULT_MODELS=['models/error.mdl','models/player.mdl','models/weapons/shell.mdl','models/weapons/rifleshell.mdl',
 'models/weapons/shotgun_shell.mdl','models/weapons/w_crowbar.mdl','models/weapons/w_pistol.mdl',
 'models/weapons/w_smg1.mdl','models/items/healthkit.mdl','models/items/battery.mdl']
class FormatError(ValueError):pass
def sha(data):return hashlib.sha256(data).hexdigest()
def i32(data,off):
    if off<0 or off+4>len(data):raise FormatError('out-of-bounds int at '+str(off))
    return struct.unpack_from('<i',data,off)[0]
def table(data,offset,count,stride,what,maxcount=1000000):
    if count<0 or count>maxcount or stride<1 or (count and (offset<0 or offset>len(data) or count>(len(data)-offset)//stride)):
        raise FormatError('out-of-bounds '+what)
    return offset
def string(data,offset,what):
    if offset<388 or offset>=len(data):raise FormatError('bad '+what+' string offset')
    end=data.find(b'\0',offset)
    if end<0:raise FormatError('unterminated '+what)
    return data[offset:end]

def convert(data):
    if not 388<=len(data)<=LIMIT or data[:4]!=b'IDST' or i32(data,4)!=37 or i32(data,76)!=len(data):
        raise FormatError('requires a complete IDST37 blob with the 388-byte header')
    nb,bo,na,ao,ng,go,nd,do,ns,so=(i32(data,p) for p in (156,160,180,184,188,192,196,200,204,208))
    if not 0<=nb<=128 or not 0<=na<=10000 or not 0<=ns<=10000:raise FormatError('unreasonable studio table counts')
    table(data,bo,nb,196,'bones',128);table(data,ao,na,92,'animation descriptors',10000)
    table(data,go,ng,8,'animation groups',10000);table(data,do,nd,124,'bone descriptors',128)
    table(data,so,ns,188,'v37 sequence descriptors',10000)
    if na!=ng or nb!=nd or i32(data,216)!=1:
        raise FormatError('only complete local animation/bone tables and one embedded sequence group are supported')
    groups=[struct.unpack_from('<ii',data,go+8*k) for k in range(ng)]
    if any(group!=0 or index!=k for k,(group,index) in enumerate(groups)):
        raise FormatError('external or non-identity animation group mapping is unsupported')
    # Bonedesc is extra v37 indexing metadata; require the same named hierarchy.
    # Runtime animation scales/poses come from the unchanged 196-byte bone table.
    for k in range(nb):
        b=bo+196*k;d=do+124*k
        if i32(data,b+4)!=i32(data,d+4) or string(data,b+i32(data,b),'bone')!=string(data,d+i32(data,d),'bone descriptor'):
            raise FormatError('v37 bone descriptor hierarchy differs from embedded runtime bones')
    out=bytearray(data)
    out[:372]=data[:188]+data[204:388]
    while len(out)%4:out.append(0)
    new_seq=len(out)
    out.extend(bytes(4284*ns))
    seq_records=[]
    for k in range(ns):
        old=so+188*k;new=new_seq+4284*k
        gx,gy=i32(data,old+64),i32(data,old+68)
        if not 1<=gx<=32 or not 1<=gy<=32 or i32(data,old+52)!=gx*gy or i32(data,old+60)!=0:
            raise FormatError('unsupported sequence group/blend dimensions at sequence '+str(k))
        if any(data[old+176:old+188]):raise FormatError('nonzero unknown sequence extension')
        blend=old+i32(data,old+56)
        table(data,blend,gx*gy,2,'sequence blend IDs',1024)
        ids=struct.unpack_from('<%dH'%(gx*gy),data,blend)
        if any(a>=na for a in ids):raise FormatError('sequence blend references an invalid local animation')
        out[new:new+56]=data[old:old+56]
        for x in range(gx):
            for y in range(gy):struct.pack_into('<i',out,new+56+4*(x*32+y),ids[y*gx+x])
        # Old movementindex has no v37 counterpart; the 2003 runtime never reads
        # that obsolete field. Animation-desc piecewise movements stay unchanged.
        struct.pack_into('<i',out,new+4152,0)
        out[new+4156:new+4192]=data[old+64:old+100]
        out[new+4192:new+4196]=data[old+60:old+64]
        out[new+4196:new+4284]=data[old+100:old+188]
        for src,dst in [(0,0),(4,4),(24,24),(148,4244),(152,4248),(156,4252),(164,4260),(168,4264)]:
            relative=i32(data,old+src)
            if relative:
                target=old+relative
                if not 388<=target<len(data):raise FormatError('sequence relative pointer outside original payload')
                struct.pack_into('<i',out,new+dst,target-new)
        label=string(data,old+i32(data,old),'sequence label').decode('utf-8',errors='replace')
        seq_records.append({'index':k,'label':label,'old_offset':old,'new_offset':new,'blend_dimensions':[gx,gy],'animation_ids':list(ids)})
    struct.pack_into('<i',out,4,36)
    struct.pack_into('<i',out,76,len(out))
    struct.pack_into('<i',out,192,new_seq)
    if len(out)>LIMIT:raise FormatError('converted output exceeds 64 MiB')
    if out[388:len(data)]!=data[388:]:raise FormatError('internal error: original studio payload changed')
    return bytes(out),{'source_header_bytes':388,'destination_header_bytes':372,'source_sequence_stride':188,
        'destination_sequence_stride':4284,'bone_stride':196,'animation_descriptor_stride':92,'model_stride':140,
        'mesh_stride':68,'vertex_stride':64,'original_payload_preserved_at_same_offsets':True,
        'original_payload_sha256':sha(data[388:]),'animations':na,'bones':nb,'sequences':seq_records}

def check_vtx(data,mdl):
    if not 36<=len(data)<=LIMIT:raise FormatError('invalid VTX length')
    version,cache,maxstrip,maxtri,maxvert,crc,nlod,repl,nb,bodies=struct.unpack_from('<iiHHiiiiii',data,0)
    if version!=6 or crc!=i32(mdl,8) or not 1<=nlod<=8 or not 1<=maxvert<=4 or nb!=i32(mdl,236):
        raise FormatError('VTX6 header/checksum/bodypart mismatch')
    def rel(owner,field,count,stride,name,maximum=1000000):
        return table(data,owner+i32(data,owner+field),count,stride,name,maximum)
    table(data,bodies,nb,8,'VTX bodyparts',256);table(data,repl,nlod,8,'VTX LOD replacements',8)
    for lod in range(nlod):
        at=repl+8*lod;n=i32(data,at);rp=rel(at,4,n,6,'VTX material replacements',2048)
        for r in range(n):
            item=rp+6*r; target=item+i32(data,item+2)
            if target<36 or target>=len(data) or data.find(b'\0',target)<0:raise FormatError('bad VTX material string')
    counts={'bodyparts':nb,'models':0,'meshes':0,'strip_groups':0,'vertices':0,'strips':0,'indices':0}
    for b in range(nb):
        bp=bodies+8*b;mn=i32(data,bp);mp=rel(bp,4,mn,8,'VTX models',1024)
        mb=i32(mdl,240)+16*b;table(mdl,mb,1,16,'MDL bodypart')
        if mn!=i32(mdl,mb+4):raise FormatError('VTX/MDL model count mismatch')
        for m in range(mn):
            mo=mp+8*m;lc=i32(data,mo);lp=rel(mo,4,lc,12,'VTX model LODs',8)
            if not 1<=lc<=nlod:raise FormatError('bad VTX model LOD count')
            mm=mb+i32(mdl,mb+12)+140*m;table(mdl,mm,1,140,'MDL model')
            counts['models']+=1
            for l in range(lc):
                lo=lp+12*l;nc=i32(data,lo);me=rel(lo,4,nc,9,'VTX meshes',8192)
                if nc!=i32(mdl,mm+72):raise FormatError('VTX/MDL mesh count mismatch')
                for k in range(nc):
                    mesh=me+9*k;gc=i32(data,mesh);gp=rel(mesh,4,gc,25,'VTX strip groups',8192)
                    mdmesh=mm+i32(mdl,mm+76)+68*k;table(mdl,mdmesh,1,68,'MDL mesh')
                    meshverts=i32(mdl,mdmesh+8)
                    if data[mesh+8]&~3:raise FormatError('unsupported VTX mesh flags')
                    counts['meshes']+=1
                    for g in range(gc):
                        group=gp+25*g;nv,vr,ni,ir,ns,sr,flags=struct.unpack_from('<iiiiiiB',data,group)
                        vp=table(data,group+vr,nv,15,'VTX6 vertices',65535)
                        ip=table(data,group+ir,ni,2,'VTX indices',1000000)
                        sp=table(data,group+sr,ns,27,'VTX strips',65535)
                        if flags&~3:raise FormatError('unsupported VTX group flags')
                        for v in range(nv):
                            vertex=struct.unpack_from('<4B4hhB',data,vp+15*v)
                            orig,n=vertex[-2:]
                            if not 0<=n<=4 or not 0<=orig<meshverts:raise FormatError('invalid VTX vertex or mesh-local vertex ID')
                            for influence in range(n):
                                bone=vertex[4+influence]
                                if vertex[influence]>3 or bone<0 or bone>=(16 if flags&2 else i32(mdl,156)):
                                    raise FormatError('invalid VTX bone mapping')
                        indices=struct.unpack_from('<%dH'%ni,data,ip) if ni else ()
                        if any(v>=nv for v in indices):raise FormatError('VTX index outside strip group')
                        for s in range(ns):
                            strip=sp+27*s;nidx,idx,nvert,vert,bones,flg,changes,cr=struct.unpack_from('<iiiihBii',data,strip)
                            if min(nidx,idx,nvert,vert,bones,changes)<0 or bones>16 or idx+nidx>ni or vert+nvert>nv or flg not in (1,2):
                                raise FormatError('invalid or unsupported VTX strip')
                            cp=table(data,strip+cr,changes,8,'VTX bone changes',1024)
                            for c in range(changes):
                                hw,bone=struct.unpack_from('<ii',data,cp+8*c)
                                if not 0<=hw<16 or not -1<=bone<i32(mdl,156):raise FormatError('invalid VTX bone state change')
                        counts['strip_groups']+=1;counts['vertices']+=nv;counts['strips']+=ns;counts['indices']+=ni
    return counts


# See modernize/integrate_beta_megabuild.py for safe staged conversion.
