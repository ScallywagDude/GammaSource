#!/usr/bin/env python3
"""Recognize HL2 directories with trailing path separators and mixed case."""
from pathlib import Path
p=Path(__file__).resolve().parents[1]/'modernize/hl2_playermodel_policy.h'
s=p.read_text()
start=s.index('inline bool IsHL2GameDir(const char *dir) {')
old='''inline bool IsHL2GameDir(const char *dir) {
    if (!dir) return false;
    const char *name=dir;
    for (const char *p=dir; *p; ++p) if (*p=='/' || *p=='\\\\') name=p+1;
    return !strcmp(name,"hl2") || !strcmp(name,"hl2mp");
}'''
# Instead of backslash escaping fragility replace function through its closing brace.
end=s.index('\n}',start)+2
new=r'''inline bool IsHL2GameDir(const char *dir) {
    // HL2MODERN_GAMEDIR_NORMALIZE: the engine may give a trailing slash or
    // mixed-case Windows paths. Compare ONLY the final directory component.
    if (!dir || !*dir) return false;
    const char *last=dir+strlen(dir);
    while (last>dir && (last[-1]=='/' || last[-1]=='\\')) --last;
    if (last==dir) return false;
    const char *name=last;
    while (name>dir && name[-1]!='/' && name[-1]!='\\') --name;
    const char hl2[]="hl2", hl2mp[]="hl2mp";
    const size_t len=(size_t)(last-name);
    if (len!=sizeof(hl2)-1 && len!=sizeof(hl2mp)-1) return false;
    char lowered[sizeof(hl2mp)];
    for (size_t i=0;i<len;++i) {
        unsigned char c=(unsigned char)name[i];
        if (c>='A' && c<='Z') c=(unsigned char)(c-'A'+'a');
        lowered[i]=(char)c;
    }
    lowered[len]=0;
    return !strcmp(lowered,hl2) || !strcmp(lowered,hl2mp);
}'''
if 'HL2MODERN_GAMEDIR_NORMALIZE' not in s:
    s=s[:start]+new+s[end:]
    p.write_text(s)
print('HL2 branch identification handles Windows case and trailing separators.')
