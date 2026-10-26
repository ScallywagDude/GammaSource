#!/usr/bin/env python3
"""Build original x86 GAS sources through the source-shipped MASM translator.

Run in a current x86 Native Tools prompt. The translator is rebuilt from C;
the shipped executable is never required. Tool errors stop the build and a
failed invocation cannot replace the previous COFF object.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[1]


def checked(command,cwd,input_bytes=None):
    result=subprocess.run(command,cwd=cwd,input=input_bytes,capture_output=True)
    if result.stderr:
        print(result.stderr.decode('utf-8',errors='replace'),file=sys.stderr,end='')
    if result.returncode:
        if result.stdout:print(result.stdout.decode('utf-8',errors='replace'),end='')
        raise ValueError('Tool failed (%d): %s'%(result.returncode,subprocess.list2cmdline(command)))
    return result


def assemble(source,output,compiler='cl',assembler='ml',root=ROOT):
    if os.name!='nt':raise ValueError('Original x86 MASM integration requires Windows')
    source=Path(source).resolve();output=Path(output).resolve()
    if source.suffix.lower()!='.s' or not source.is_file():raise ValueError('GAS source not found: '+str(source))
    compiler=shutil.which(compiler)
    assembler=shutil.which(assembler)
    if not compiler or not assembler:raise ValueError('cl.exe and ml.exe are required from an x86 Native Tools prompt')
    output.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=source.stem+'-asm-',dir=output.parent) as directory:
        work=Path(directory)
        translator=work/'gas2masm.exe'
        checked([compiler,'/nologo','/TC','/O2','/MT','/D_CRT_SECURE_NO_WARNINGS',
                 '/Fo'+str(work/'translator.obj'),'/Fe'+str(translator),
                 str(root/'engine/gas2masm/gas2masm.c')],work)
        preprocessed=checked([compiler,'/nologo','/EP','/TC',str(source)],root/'engine').stdout
        translated=checked([str(translator)],root/'engine',preprocessed)
        # The historical translator returns zero even for some parse errors.
        if b'error:' in translated.stderr.lower():raise ValueError('GAS translator rejected the source')
        if not translated.stdout.strip().endswith(b'END'):raise ValueError('GAS translator produced incomplete MASM source')
        assembly=work/(source.stem+'.asm')
        assembly.write_bytes(translated.stdout)
        temporary_object=work/(source.stem+'.obj')
        checked([assembler,'/nologo','/c','/Cp','/coff','/safeseh','/Fo'+str(temporary_object),str(assembly)],root/'engine')
        if not temporary_object.is_file():raise ValueError('MASM did not produce the declared COFF object')
        os.replace(temporary_object,output)
    return output


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--compiler',default='cl')
    parser.add_argument('--assembler',default='ml')
    args=parser.parse_args(argv)
    try:
        result=assemble(args.source,args.output,args.compiler,args.assembler)
    except (OSError,ValueError) as error:
        print('Legacy MASM error:',error,file=sys.stderr)
        return 2
    print('Original x86 assembly:',result)
    return 0


if __name__=='__main__':sys.exit(main())
