"""Tiny Win32 debugger: run a program, report exceptions (code, address, module) and how
it exits, plus debug strings. Usage: minidbg.py <cwd> <exe> [args...]"""
import ctypes, sys, os
from ctypes import wintypes as W
k32 = ctypes.WinDLL('kernel32', use_last_error=True)
psapi = ctypes.WinDLL('psapi')

DEBUG_ONLY_THIS_PROCESS = 0x2
CREATE_NEW_CONSOLE = 0x10
EXCEPTION_DEBUG_EVENT, CREATE_PROCESS_DEBUG_EVENT, EXIT_PROCESS_DEBUG_EVENT, LOAD_DLL_DEBUG_EVENT, OUTPUT_DEBUG_STRING_EVENT = 1, 3, 5, 6, 8
DBG_CONTINUE, DBG_EXCEPTION_NOT_HANDLED = 0x00010002, 0x80010001

class STARTUPINFO(ctypes.Structure):
    _fields_ = [('cb', W.DWORD), ('lpReserved', W.LPWSTR), ('lpDesktop', W.LPWSTR), ('lpTitle', W.LPWSTR),
                ('dwX', W.DWORD), ('dwY', W.DWORD), ('dwXSize', W.DWORD), ('dwYSize', W.DWORD),
                ('dwXCountChars', W.DWORD), ('dwYCountChars', W.DWORD), ('dwFillAttribute', W.DWORD),
                ('dwFlags', W.DWORD), ('wShowWindow', W.WORD), ('cbReserved2', W.WORD),
                ('lpReserved2', ctypes.c_void_p), ('hStdInput', W.HANDLE), ('hStdOutput', W.HANDLE), ('hStdError', W.HANDLE)]
class PROCESS_INFORMATION(ctypes.Structure):
    _fields_ = [('hProcess', W.HANDLE), ('hThread', W.HANDLE), ('dwProcessId', W.DWORD), ('dwThreadId', W.DWORD)]
class EXCEPTION_RECORD(ctypes.Structure):
    pass
EXCEPTION_RECORD._fields_ = [('ExceptionCode', W.DWORD), ('ExceptionFlags', W.DWORD), ('ExceptionRecord', ctypes.c_void_p),
                             ('ExceptionAddress', ctypes.c_void_p), ('NumberParameters', W.DWORD), ('__pad', W.DWORD),
                             ('ExceptionInformation', ctypes.c_uint64 * 15)]
class EXCEPTION_DEBUG_INFO(ctypes.Structure):
    _fields_ = [('ExceptionRecord', EXCEPTION_RECORD), ('dwFirstChance', W.DWORD)]
class LOAD_DLL(ctypes.Structure):
    _fields_ = [('hFile', W.HANDLE), ('lpBaseOfDll', ctypes.c_void_p), ('dwDebugInfoFileOffset', W.DWORD),
                ('nDebugInfoSize', W.DWORD), ('lpImageName', ctypes.c_void_p), ('fUnicode', W.WORD)]
class EXIT_PROCESS(ctypes.Structure):
    _fields_ = [('dwExitCode', W.DWORD)]
class OUTPUT_DEBUG_STRING(ctypes.Structure):
    _fields_ = [('lpDebugStringData', ctypes.c_void_p), ('fUnicode', W.WORD), ('nDebugStringLength', W.WORD)]
class U(ctypes.Union):
    _fields_ = [('Exception', EXCEPTION_DEBUG_INFO), ('LoadDll', LOAD_DLL), ('ExitProcess', EXIT_PROCESS),
                ('DebugString', OUTPUT_DEBUG_STRING), ('raw', ctypes.c_byte * 176)]
class DEBUG_EVENT(ctypes.Structure):
    _fields_ = [('dwDebugEventCode', W.DWORD), ('dwProcessId', W.DWORD), ('dwThreadId', W.DWORD), ('__pad', W.DWORD), ('u', U)]

cwd, exe, args = sys.argv[1], sys.argv[2], sys.argv[3:]
cmd = '"%s" %s' % (exe, ' '.join(args))
si = STARTUPINFO(); si.cb = ctypes.sizeof(si); si.dwFlags = 1; si.wShowWindow = 7  # SW_SHOWMINNOACTIVE
pi = PROCESS_INFORMATION()
if not k32.CreateProcessW(None, ctypes.create_unicode_buffer(cmd), None, None, False, DEBUG_ONLY_THIS_PROCESS, None, cwd, ctypes.byref(si), ctypes.byref(pi)):
    print('CreateProcess failed', ctypes.get_last_error()); sys.exit(1)

modules = []  # (base, name)
def modname(addr):
    best = None
    for base, name in modules:
        if base <= addr and (best is None or base > best[0]): best = (base, name)
    return '%s+0x%x' % (os.path.basename(best[1]), addr - best[0]) if best else hex(addr)

def read_path(hfile):
    buf = ctypes.create_unicode_buffer(1024)
    if k32.GetFinalPathNameByHandleW(hfile, buf, 1024, 0): return buf.value
    return '?'

ev = DEBUG_EVENT(); seen = 0
while True:
    if not k32.WaitForDebugEvent(ctypes.byref(ev), 180000):
        print('timeout; killing'); k32.TerminateProcess(pi.hProcess, 1); break
    status = DBG_CONTINUE
    c = ev.dwDebugEventCode
    if c == CREATE_PROCESS_DEBUG_EVENT:
        # first bytes of union: hFile, hProcess, hThread, lpBaseOfImage
        raw = bytes(ev.u.raw)
        hfile = int.from_bytes(raw[0:8], 'little'); base = int.from_bytes(raw[24:32], 'little')
        modules.append((base, read_path(hfile)))
    elif c == LOAD_DLL_DEBUG_EVENT:
        modules.append((ev.u.LoadDll.lpBaseOfDll or 0, read_path(ev.u.LoadDll.hFile)))
    elif c == OUTPUT_DEBUG_STRING_EVENT:
        n = ev.u.DebugString.nDebugStringLength
        buf = ctypes.create_string_buffer(n)
        k32.ReadProcessMemory(pi.hProcess, ctypes.c_void_p(ev.u.DebugString.lpDebugStringData), buf, n, None)
        print('ODS:', buf.value.decode('latin-1', 'replace').rstrip())
    elif c == EXCEPTION_DEBUG_EVENT:
        rec = ev.u.Exception.ExceptionRecord
        code = rec.ExceptionCode
        first = ev.u.Exception.dwFirstChance
        if code not in (0x80000003, 0x406D1388, 0x4001000A, 0xE06D7363) or not first:
            seen += 1
            if seen < 40:
                print('EXC 0x%08X at %s first=%d' % (code, modname(rec.ExceptionAddress or 0), first))
            if first and code == 0xC0000005:
                # scan the faulting thread's stack for return addresses inside known modules
                ht = k32.OpenThread(0x001F03FF, False, ev.dwThreadId)
                ctx = (ctypes.c_byte * 1232)()
                pctx = ctypes.addressof(ctx)
                aligned = (pctx + 15) & ~15
                buf = (ctypes.c_byte * (1232 + 16)).from_address(pctx) if False else None
                raw = ctypes.create_string_buffer(1232 + 16)
                al = (ctypes.addressof(raw) + 15) & ~15
                ctypes.c_uint32.from_address(al + 0x30).value = 0x10000B  # CONTEXT_AMD64 | CONTROL | INTEGER
                k32.OpenThread.restype = W.HANDLE; k32.GetThreadContext.argtypes = [W.HANDLE, ctypes.c_void_p]; k32.ReadProcessMemory.argtypes = [W.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p]
                ht = k32.OpenThread(0x001F03FF, False, ev.dwThreadId)
                okc = k32.GetThreadContext(ht, al); print('  ctx', okc, ctypes.get_last_error())
                if okc:
                    # proper unwind with dbghelp
                    dbg = ctypes.WinDLL('dbghelp')
                    dbg.SymInitialize(W.HANDLE(pi.hProcess), None, True)
                    class ADDRESS64(ctypes.Structure):
                        _fields_ = [('Offset', ctypes.c_uint64), ('Segment', W.WORD), ('Mode', ctypes.c_int)]
                    class KDHELP64(ctypes.Structure):
                        _fields_ = [('raw', ctypes.c_uint64 * 20)]
                    class STACKFRAME64(ctypes.Structure):
                        _fields_ = [('AddrPC', ADDRESS64), ('AddrReturn', ADDRESS64), ('AddrFrame', ADDRESS64), ('AddrStack', ADDRESS64),
                                    ('AddrBStore', ADDRESS64), ('FuncTableEntry', ctypes.c_void_p), ('Params', ctypes.c_uint64 * 4),
                                    ('Far', W.BOOL), ('Virtual', W.BOOL), ('Reserved', ctypes.c_uint64 * 3), ('KdHelp', KDHELP64)]
                    sf = STACKFRAME64()
                    sf.AddrPC.Offset = ctypes.c_uint64.from_address(al + 0xF8).value; sf.AddrPC.Mode = 3
                    sf.AddrStack.Offset = ctypes.c_uint64.from_address(al + 0x98).value; sf.AddrStack.Mode = 3
                    sf.AddrFrame.Offset = ctypes.c_uint64.from_address(al + 0xA0).value; sf.AddrFrame.Mode = 3
                    dbg.StackWalk64.argtypes = [W.DWORD, W.HANDLE, W.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]
                    fta = ctypes.cast(dbg.SymFunctionTableAccess64, ctypes.c_void_p); gmb = ctypes.cast(dbg.SymGetModuleBase64, ctypes.c_void_p)
                    for _ in range(40):
                        if not dbg.StackWalk64(0x8664, pi.hProcess, ht, ctypes.byref(sf), ctypes.c_void_p(al), None, fta, gmb, None): break
                        print('  frame', modname(sf.AddrPC.Offset))
                    lo = None; vals = []
                if False:
                    rsp = ctypes.c_uint64.from_address(al + 0x98).value
                    stack = ctypes.create_string_buffer(0x4000)
                    k32.ReadProcessMemory(pi.hProcess, ctypes.c_void_p(rsp), stack, 0x4000, None)
                    vals = [int.from_bytes(stack.raw[i:i + 8], 'little') for i in range(0, 0x4000, 8)]
                    ours = []
                    for b, n in modules:
                        if 'source2018_install' not in n.lower(): continue
                        hdr = ctypes.create_string_buffer(0x400)
                        k32.ReadProcessMemory(pi.hProcess, ctypes.c_void_p(b), hdr, 0x400, None)
                        nt = int.from_bytes(hdr.raw[0x3c:0x40], 'little')
                        size = int.from_bytes(hdr.raw[nt + 0x50:nt + 0x54], 'little') if nt < 0x3f0 else 0
                        ours.append((b, b + size, os.path.basename(n)))
                    shown = 0
                    for v in vals:
                        for b, e, n in ours:
                            if b <= v < e:
                                print('  stack %s+0x%x' % (n, v - b)); shown += 1
                        if shown > 30: break
                    lo = None
                    for v in ([] if lo is None else vals):
                        if v > lo and v < lo + 0x7fffffff * 4:
                            m = modname(v)
                            if '+0x' in m and int(m.split('+0x')[1], 16) < 0x2000000 and not m.startswith(('ntdll', 'KERNEL', 'kernel', 'ucrt', 'VCRUNTIME', 'd3d9', 'nvd', 'ig', 'atio')):
                                print('  stack', m); shown += 1
                                if shown > 24: break
                k32.CloseHandle(ht)
        if code == 0xE06D7363 and seen < 40:
            print('C++ exception at', modname(rec.ExceptionAddress or 0)); seen += 1
        status = DBG_CONTINUE if code == 0x80000003 else DBG_EXCEPTION_NOT_HANDLED
    elif c == EXIT_PROCESS_DEBUG_EVENT:
        print('EXIT code', ev.u.ExitProcess.dwExitCode, 'thread', ev.dwThreadId)
        k32.ContinueDebugEvent(ev.dwProcessId, ev.dwThreadId, DBG_CONTINUE)
        break
    k32.ContinueDebugEvent(ev.dwProcessId, ev.dwThreadId, status)
