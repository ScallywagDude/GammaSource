"""Repair GammaSource 2018 selector positions and missing raster icons.

Uses original sprite art for matching placeholder viewmodels and simple outline
symbols for utility props. Original 2003 content is never modified.
"""
from pathlib import Path
import argparse,re,shutil,struct,sys
from PIL import Image,ImageDraw
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE.parent))
from weapon_asset_audit import kv_parse
from hud_pixel_audit_v18 import decode_vtf

BUCKETS=[
 ['crowbar','iceaxe','stunstick','physcannon','physgun','manhack','portalgun'],
 ['pistol','flaregun','alyxgun','thumper','rollerwand','357','proto1'],
 ['smg1','smg2','shotgun','hmg1','brickbat','annabelle','citizenpackage','citizensuitcase','cubemap','beerbottle','beerbottle2'],
 ['ar1','ar2','irifle','sniperrifle','crossbow','ar2future'],
 ['rpg','frag','slam','hopwire','molotov','stickylauncher','vortexhopwire','tripwire','ml'],
 ['binoculars','extinguisher','bugbait','gauss','cguard','immolator'],
]
ICONS=['rollerwand','stickylauncher','thumper','citizenpackage','citizensuitcase','cubemap']

def block(s,key):
 m=re.search(r'(?im)^\s*"?'+re.escape(key)+r'"?\s*\{',s)
 if not m:return None
 depth=0
 for j in range(s.index('{',m.start()),len(s)):
  depth+=s[j]=='{';depth-=s[j]=='}'
  if depth==0:return m.start(),j+1
 raise ValueError('unclosed '+key)

def set_value(s,key,value):
 pat=r'(?im)^(\s*"?'+re.escape(key)+r'"?\s*)"[^"\r\n]*"'
 if re.search(pat,s):return re.sub(pat,lambda m:m[1]+'"'+str(value)+'"',s,count=1)
 at=s.index('{')+1
 return s[:at]+'\n\t"'+key+'"\t"'+str(value)+'"'+s[at:]

def write_vtf(im,path):
 im=im.convert('RGB');w,h=im.size;mips=[im]
 while im.width>1 or im.height>1:
  im=im.resize((max(1,im.width//2),max(1,im.height//2)),Image.Resampling.BOX);mips.append(im)
 hdr=bytearray(64);hdr[:4]=b'VTF\0';struct.pack_into('<iii',hdr,4,7,1,64)
 struct.pack_into('<HHIHH',hdr,16,w,h,0x2000|4|8,1,0);struct.pack_into('<fff',hdr,32,1.,1.,1.)
 struct.pack_into('<f',hdr,48,1.);struct.pack_into('<i',hdr,52,3);hdr[56]=len(mips);struct.pack_into('<i',hdr,57,-1)
 path.write_bytes(bytes(hdr)+b''.join(x.tobytes('raw','BGR') for x in reversed(mips)))

def main():
 a=argparse.ArgumentParser();a.add_argument('--repo',type=Path,default=HERE.parents[1]);a.add_argument('--preview',type=Path);a.add_argument('--backup',type=Path);args=a.parse_args()
 repo=args.repo;out=repo/'gammasource_content_2018';base=repo/'gammasource_content'
 def save(p,data):
  if args.backup and p.exists():
   dest=args.backup/p.relative_to(repo);dest.parent.mkdir(parents=True,exist_ok=True)
   if not dest.exists():shutil.copy2(p,dest)
  p.parent.mkdir(parents=True,exist_ok=True)
  if isinstance(data,str):p.write_text(data,encoding='latin1')
  else:p.write_bytes(data)
 def donor(name):
  p=out/'scripts'/('weapon_'+name+'.txt')
  if not p.exists():p=base/'scripts'/p.name
  spec=kv_parse(p.read_text(encoding='latin1'))['texturedata']['weapon_s']
  stem=spec['file'];mat=next(r/'materials'/(stem+'.vmt') for r in (out,base) if (r/'materials'/(stem+'.vmt')).exists())
  vt=kv_parse(mat.read_text(encoding='latin1'))['$basetexture'];file=next(r/'materials'/(vt+'.vtf') for r in (out,base) if (r/'materials'/(vt+'.vtf')).exists())
  meta,images=decode_vtf(file.read_bytes());x,y,w,h=(int(spec[k]) for k in ('x','y','width','height'))
  return images[0].crop((x,y,x+w,y+h)).convert('RGB')
 atlas=Image.new('RGB',(256,256));cells=[]
 for i,name in enumerate(ICONS):
  cell=Image.new('RGB',(128,64));d=ImageDraw.Draw(cell)
  if name in ('rollerwand','stickylauncher'):
   src=donor('slam' if name=='rollerwand' else 'pistol');src.thumbnail((116,52));cell.paste(src,((128-src.width)//2,(64-src.height)//2))
  elif name=='thumper':
   d.line([(49,50),(49,17),(54,11),(73,11),(79,17),(79,50)],fill='white',width=2)
   d.rectangle((44,48,84,53),outline='white',width=2);d.rectangle((46,15,82,21),outline='white',width=2)
   d.rectangle((35,25,49,33),outline='white',width=2);d.rectangle((79,25,94,33),outline='white',width=2)
   d.ellipse((57,27,71,41),outline='white',width=2);d.line((64,30,64,38),fill='white',width=2)
  elif name=='citizenpackage':
   d.polygon([(29,22),(65,12),(99,23),(64,35),(29,22)],outline='white',width=2)
   d.line([(29,22),(29,43),(64,55),(99,43),(99,23)],fill='white',width=2);d.line([(64,35),(64,55)],fill='white',width=2)
   d.line([(47,17),(81,29),(81,39)],fill='white',width=2)
  elif name=='citizensuitcase':
   d.rounded_rectangle((26,23,103,52),radius=3,outline='white',width=2);d.rounded_rectangle((51,14,77,25),radius=3,outline='white',width=2)
   d.line((39,25,39,50),fill='white',width=2);d.line((89,25,89,50),fill='white',width=2)
  else:
   d.rectangle((33,22,77,54),outline='white',width=2);d.rectangle((51,8,95,40),outline='white',width=2)
   for x,y in ((33,22),(77,22),(77,54),(33,54)):d.line((x,y,x+18,y-14),fill='white',width=2)
  cells.append(cell);atlas.paste(cell,((i%2)*128,(i//2)*64))
 material=out/'materials/sprites/w_icons6b.vmt';save(material,'"UnlitGeneric"\n{\n\t"$basetexture" "sprites/w_icons6b"\n\t"$additive" "1"\n\t"$vertexcolor" "1"\n\t"$vertexalpha" "1"\n\t"$ignorez" "1"\n}\n')
 tex=out/'materials/sprites/w_icons6b.vtf'
 if args.backup and tex.exists():
  dest=args.backup/tex.relative_to(repo);dest.parent.mkdir(parents=True,exist_ok=True)
  if not dest.exists():shutil.copy2(tex,dest)
 write_vtf(atlas,tex)
 for slot,names in enumerate(BUCKETS):
  for pos,name in enumerate(names):
   dst=out/'scripts'/('weapon_'+name+'.txt');src=dst if dst.exists() else base/'scripts'/dst.name
   if not src.exists():continue
   s=src.read_text(encoding='latin1')
   for key,value in [('bucket',slot),('bucket_position',pos),('bucket_360',slot),('bucket_position_360',pos)]:s=set_value(s,key,value)
   if name in ICONS:
    i=ICONS.index(name);x=(i%2)*128;y=(i//2)*64
    td=block(s,'TextureData');assert td,name
    text=s[td[0]:td[1]]
    for role in ('weapon','weapon_s'):
     existing=block(text,role)
     if existing:text=text[:existing[0]]+text[existing[1]:]
    icons=''.join('\n\t\t"'+role+'"\n\t\t{\n\t\t\t"file" "sprites/w_icons6b"\n\t\t\t"x" "'+str(x)+'"\n\t\t\t"y" "'+str(y)+'"\n\t\t\t"width" "128"\n\t\t\t"height" "64"\n\t\t}\n' for role in ('weapon','weapon_s'))
    text=text[:-1]+icons+'\t}'
    s=s[:td[0]]+text+s[td[1]:]
    if name in ('citizenpackage','citizensuitcase'):s=set_value(s,'printname','Citizen package' if name=='citizenpackage' else 'Suitcase')
   save(dst,s)
 if args.preview:
  preview=Image.new('RGB',(512,3*96),(12,14,13));d=ImageDraw.Draw(preview)
  for i,(name,cell) in enumerate(zip(ICONS,cells)):
   x=(i%2)*256;y=(i//2)*96;preview.paste(cell,(x+64,y));d.text((x+24,y+66),name,fill=(232,204,107))
  args.preview.parent.mkdir(parents=True,exist_ok=True);preview.save(args.preview)
 print('Repaired selector positions for',sum(map(len,BUCKETS)),'scripts; six missing icon pairs use sprites/w_icons6b.')
if __name__=='__main__':main()
