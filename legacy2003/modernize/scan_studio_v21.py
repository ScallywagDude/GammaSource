#!/usr/bin/env python3
"""Offline, non-destructive Source 2003 MDL 36/37 asset-sidecar inventory.

This is deliberately a header/sidecar audit, NOT a v37 animation converter.
The engine's studio-structure validator and actual render test must still pass.
"""
import argparse, json, pathlib, struct
VTX = ('.dx80.vtx', '.dx7_3bone.vtx', '.dx7_2bone.vtx')

def inspect_mdl(path, vertex_lookup=None):
    """Inspect MDL and optional VTX sidecars.

    vertex_lookup(ext) resolves each sidecar using game filesystem search
    order and returns (layer_index, path) or None. Without a callback, only
    sidecars beside the model are considered (original standalone behavior).
    """
    b = path.read_bytes()[:80]
    if len(b) != 80 or b[:4] != b'IDST':
        return {'status':'invalid_mdl_header'}
    v, crc, size = struct.unpack_from('<III', b, 4)[0], struct.unpack_from('<I',b,8)[0], struct.unpack_from('<I',b,76)[0]
    actual = path.stat().st_size
    result = {'mdl_version':v,'checksum':crc,'declared_bytes':size,'actual_bytes':actual}
    if v not in (36,37):
        result['status']='unsupported_mdl_version'
        return result
    if size != actual or actual < 80 or actual > 64*1024*1024 or b'\x00' not in b[12:76]:
        result['status']='invalid_mdl_size_or_name'
        return result
    variants=[]
    for ext in VTX:
        found = vertex_lookup(ext) if vertex_lookup is not None else None
        vertex = found[1] if found is not None else path.with_suffix(ext)
        if not vertex.is_file():
            variants.append({'ext':ext,'status':'missing'})
            continue
        data=vertex.read_bytes()[:36]
        item={'ext':ext,'bytes':vertex.stat().st_size}
        if found is not None:
            item['layer']=found[0]
        if len(data)<36 or vertex.stat().st_size > 64*1024*1024:
            item['status']='truncated_or_oversized'
        else:
            vv, cc, nlod, nparts, off=(struct.unpack_from('<I',data,0)[0],
                                      struct.unpack_from('<I',data,16)[0],
                                      struct.unpack_from('<I',data,20)[0],
                                      struct.unpack_from('<I',data,28)[0],
                                      struct.unpack_from('<I',data,32)[0])
            item.update({'vtx_version':vv,'checksum':cc})
            ok = vv==6 and cc==crc and 1 <= nlod <=8 and 1<=nparts<=256 and 36<=off and nparts <= (item['bytes']-off)//8
            # v22: this is ONLY header eligibility for offline conversion.
            # A v7 vertex buffer has a different packed vertex struct from
            # the legacy v6 renderer. Deep validation/conversion must pass.
            v7 = vv==7 and cc==crc and 1<=nlod<=8 and 1<=nparts<=256 and 36<=off and nparts <= (item['bytes']-off)//8
            item['status'] = ('2003_vtx6_candidate' if ok else
                'v7_offline_conversion_candidate' if v7 else 'incompatible_or_corrupt_vtx')
        variants.append(item)
    result['vertex_companions']=variants
    # A present VTX does not establish correctness of studio tables, skeletons,
    # animations, meshes or materials. Report a candidate, not a playable model.
    result['status']='2003_loader_candidate' if any(x['status']=='2003_vtx6_candidate' for x in variants) else 'no_compatible_2003_vertex_file'
    result['has_v7_conversion_candidate']=any(x['status']=='v7_offline_conversion_candidate' for x in variants)
    result['conversion_tool']='modernize/convert_vtx7_to_v6_v22.py' if result['has_v7_conversion_candidate'] else None
    result['runtime_studio_layout_verified']=False
    return result

def inventory(branch_roots):
    result=[]
    for branch, root in branch_roots:
        root=pathlib.Path(root).resolve()
        if not root.is_dir():
            raise ValueError('missing asset root: %s'%root)
        for path in sorted(root.rglob('*.mdl')):
            if path.is_symlink() or not path.is_file():
                continue
            # Keep roots distinct. Do not silently merge HL2, HL1, CS or TF2.
            record={'branch':branch,'path':str(path.relative_to(root)).replace('\\','/')}
            record.update(inspect_mdl(path))
            result.append(record)
    return result

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--root',action='append',required=True,metavar='BRANCH=DIRECTORY',help='repeat for each separate game asset layer')
    ap.add_argument('--output',type=pathlib.Path,help='optional JSON output file')
    args=ap.parse_args()
    roots=[]
    for item in args.root:
        if '=' not in item or not item.split('=',1)[0]: ap.error('--root requires BRANCH=DIRECTORY')
        roots.append(tuple(item.split('=',1)))
    data=inventory(roots)
    report={'scan_version':22,'models':data,'counts':{}}
    for row in data: report['counts'][row['status']]=report['counts'].get(row['status'],0)+1
    serialized=json.dumps(report,indent=2,sort_keys=True)+'\n'
    if args.output: args.output.write_text(serialized,encoding='utf-8')
    else: print(serialized,end='')
if __name__=='__main__':main()
