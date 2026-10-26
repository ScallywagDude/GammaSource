"""Compare final live helper matrices with original 2003 procedural rules.

Consumes read-only gamma_pose_snapshot console JSON. Each expected helper uses
the live control and parent matrices, isolating the procedural transformation
from base/overlay/IK composition, animation sampling and camera orientation.
"""
import argparse
import collections
import json
import math
from pathlib import Path

import audit_legacy_procedures as procedures
from audit_legacy_animation_samples import i, normalize_original, qnorm


def matrix_pose(m):
    """Quaternion from a row-major 3x4 proper rotation, matching Source axes."""
    trace=m[0]+m[5]+m[10]
    if trace>0:
        s=math.sqrt(max(0,trace+1))*2
        q=((m[9]-m[6])/s,(m[2]-m[8])/s,(m[4]-m[1])/s,s/4)
    elif m[0]>m[5] and m[0]>m[10]:
        s=math.sqrt(max(0,1+m[0]-m[5]-m[10]))*2
        q=(s/4,(m[1]+m[4])/s,(m[2]+m[8])/s,(m[9]-m[6])/s)
    elif m[5]>m[10]:
        s=math.sqrt(max(0,1+m[5]-m[0]-m[10]))*2
        q=((m[1]+m[4])/s,s/4,(m[6]+m[9])/s,(m[2]-m[8])/s)
    else:
        s=math.sqrt(max(0,1+m[10]-m[0]-m[5]))*2
        q=((m[2]+m[8])/s,(m[6]+m[9])/s,s/4,(m[4]-m[1])/s)
    return (m[3],m[7],m[11]),qnorm(q)


def local_control(parent,world):
    inverse=(-parent[1][0],-parent[1][1],-parent[1][2],parent[1][3])
    return qnorm(procedures.qmul(inverse,world[1]))


def orthogonality(m):
    columns=[(m[k],m[4+k],m[8+k]) for k in range(3)]
    return max(abs(sum(x*y for x,y in zip(a,b))-(1 if j==k else 0))
               for j,a in enumerate(columns) for k,b in enumerate(columns))


def read_snapshots(path):
    snapshots={};errors=[]
    for number,line in enumerate(path.read_text(errors='replace').splitlines(),1):
        start=line.find('GAMMA_POSE {')
        if start<0:continue
        try:
            row=json.loads(line[start+len('GAMMA_POSE '):])
            snapshots.setdefault(row['snapshot'],[]).append(row)
        except (ValueError,KeyError) as error:errors.append({'line':number,'error':str(error)})
    return snapshots,errors


def audit(original,log):
    source,version=normalize_original(original.read_bytes())
    rules=procedures.procedures(source)
    parents=[i(source,i(source,160)+196*bone+4) for bone in range(i(source,156))]
    snapshots,parse_errors=read_snapshots(log)
    results=[];missing=nonfinite=0;max_position=max_angle=max_ortho=0
    for serial,records in snapshots.items():
        begin=next((row for row in records if row['record']=='begin'),None)
        end=next((row for row in records if row['record']=='end'),None)
        if not begin or not end:
            parse_errors.append({'snapshot':serial,'error':'Incomplete snapshot'});continue
        if Path(begin['model']).name.lower()!=original.name.lower():continue
        bones={row['index']:row for row in records if row['record']=='bone'}
        poses={}
        for bone,row in bones.items():
            matrix=row['matrix']
            if len(matrix)!=12 or not all(value is not None and math.isfinite(value) for value in matrix):
                nonfinite+=1;continue
            poses[bone]=matrix_pose(matrix)
        for bone,rule in rules.items():
            if rule['type']!=2:continue  # Alyx/Kleiner use only quaternion helpers.
            control=rule['control'];control_parent=parents[control];parent=parents[bone]
            if any(index not in poses for index in [bone,parent,control,control_parent]):
                parse_errors.append({'snapshot':serial,'bone':bone,'error':'Missing required final matrix'});continue
            expected_local=procedures.solve(rule,local_control(poses[control_parent],poses[control]))
            expected=procedures.compose(poses[parent],expected_local)
            position,angle=procedures.error(expected,poses[bone])
            ortho=orthogonality(bones[bone]['matrix'])
            present=bool(bones[bone]['flags']&4 and bones[bone]['procedure_type']==2 and bones[bone]['procedure_offset'])
            if not present:missing+=1
            max_position=max(max_position,position);max_angle=max(max_angle,angle);max_ortho=max(max_ortho,ortho)
            results.append({'snapshot':serial,'label':begin['label'],'time':begin['time'],'bone':bone,'name':rule['name'],'procedure_present':present,'position_error_units':position,'rotation_error_degrees':angle,'matrix_orthogonality_error':ortho})
    failures=[row for row in results if not row['procedure_present'] or row['position_error_units']>.02 or row['rotation_error_degrees']>.05 or row['matrix_orthogonality_error']>.001]
    return {'original':str(original),'source_version':version,'runtime_log':str(log),'snapshot_records':len(snapshots),'helper_transform_samples':len(results),'missing_helper_samples':missing,'nonfinite_matrices':nonfinite,'max_position_error_units':max_position,'max_rotation_error_degrees':max_angle,'max_matrix_orthogonality_error':max_ortho,'failed_helper_samples':len(failures),'parse_errors':parse_errors,'worst':sorted(results,key=lambda row:row['position_error_units']+row['rotation_error_degrees'],reverse=True)[:30],'results':results,'scope':'Original quaternion-interpolated helper rule evaluated with actual client control/parent matrices. Does not independently certify base animation, scene movement, vertex skinning, speech or visual quality.'}


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('original',type=Path);parser.add_argument('log',type=Path);parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args();result=audit(args.original,args.log)
    args.out.parent.mkdir(parents=True,exist_ok=True);args.out.write_text(json.dumps(result,indent=2,allow_nan=False)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k not in ['results','worst']},indent=2))
    raise SystemExit(1 if result['failed_helper_samples'] or result['parse_errors'] or not result['helper_transform_samples'] else 0)
