#ifndef HL2_MODERN_PLAYERMODEL_POLICY_H
#define HL2_MODERN_PLAYERMODEL_POLICY_H
// HL2-only player models. Do not share with HL1, Counter-Strike or TF2.
// The list is intentionally finite: servers must not accept arbitrary paths
// from a player's userinfo string. Missing assets are skipped at runtime.
#include <string.h>
namespace HL2ModernPlayerModels {
struct Entry { const char *label; const char *path; const char *portrait; };
static const Entry kModels[] = {
    { "Classic Player", "models/player.mdl", "player" },
    { "Combine Soldier", "models/combine_soldier.mdl", "combine_soldier" },
    { "Combine Elite", "models/combine_super_soldier.mdl", "combine_super_soldier" },
    { "Metropolice", "models/police.mdl", "police" },
    { "Citizen Male 01", "models/humans/group01/male_01.mdl", "humans_group01_male_01" },
    { "Citizen Male 02", "models/humans/group01/male_02.mdl", "humans_group01_male_02" },
    { "Citizen Male 03", "models/humans/group01/male_03.mdl", "humans_group01_male_03" },
    { "Citizen Male 04", "models/humans/group01/male_04.mdl", "humans_group01_male_04" },
    { "Citizen Male 05", "models/humans/group01/male_05.mdl", "humans_group01_male_05" },
    { "Citizen Male 06", "models/humans/group01/male_06.mdl", "humans_group01_male_06" },
    { "Citizen Male 07", "models/humans/group01/male_07.mdl", "humans_group01_male_07" },
    { "Citizen Male 08", "models/humans/group01/male_08.mdl", "humans_group01_male_08" },
    { "Citizen Male 09", "models/humans/group01/male_09.mdl", "humans_group01_male_09" },
    { "Citizen Female 01", "models/humans/group01/female_01.mdl", "humans_group01_female_01" },
    { "Citizen Female 02", "models/humans/group01/female_02.mdl", "humans_group01_female_02" },
    { "Citizen Female 03", "models/humans/group01/female_03.mdl", "humans_group01_female_03" },
    { "Citizen Female 04", "models/humans/group01/female_04.mdl", "humans_group01_female_04" },
    { "Citizen Female 05", "models/humans/group01/female_05.mdl", "humans_group01_female_05" },
    { "Citizen Female 06", "models/humans/group01/female_06.mdl", "humans_group01_female_06" },
    { "Citizen Female 07", "models/humans/group01/female_07.mdl", "humans_group01_female_07" },
    { "Rebel Male 01", "models/humans/group03/male_01.mdl", "humans_group03_male_01" },
    { "Rebel Male 02", "models/humans/group03/male_02.mdl", "humans_group03_male_02" },
    { "Rebel Male 03", "models/humans/group03/male_03.mdl", "humans_group03_male_03" },
    { "Rebel Male 04", "models/humans/group03/male_04.mdl", "humans_group03_male_04" },
    { "Rebel Male 05", "models/humans/group03/male_05.mdl", "humans_group03_male_05" },
    { "Rebel Male 06", "models/humans/group03/male_06.mdl", "humans_group03_male_06" },
    { "Rebel Male 07", "models/humans/group03/male_07.mdl", "humans_group03_male_07" },
    { "Rebel Male 08", "models/humans/group03/male_08.mdl", "humans_group03_male_08" },
    { "Rebel Male 09", "models/humans/group03/male_09.mdl", "humans_group03_male_09" },
    { "Rebel Female 01", "models/humans/group03/female_01.mdl", "humans_group03_female_01" },
    { "Rebel Female 02", "models/humans/group03/female_02.mdl", "humans_group03_female_02" },
    { "Rebel Female 03", "models/humans/group03/female_03.mdl", "humans_group03_female_03" },
    { "Rebel Female 04", "models/humans/group03/female_04.mdl", "humans_group03_female_04" },
    { "Rebel Female 05", "models/humans/group03/female_05.mdl", "humans_group03_female_05" },
    { "Rebel Female 06", "models/humans/group03/female_06.mdl", "humans_group03_female_06" },
    { "Rebel Female 07", "models/humans/group03/female_07.mdl", "humans_group03_female_07" },
    { "Medic Rebel Male 01", "models/humans/group03m/male_01.mdl", "humans_group03m_male_01" },
    { "Medic Rebel Male 02", "models/humans/group03m/male_02.mdl", "humans_group03m_male_02" },
    { "Medic Rebel Male 03", "models/humans/group03m/male_03.mdl", "humans_group03m_male_03" },
    { "Medic Rebel Male 04", "models/humans/group03m/male_04.mdl", "humans_group03m_male_04" },
    { "Medic Rebel Male 05", "models/humans/group03m/male_05.mdl", "humans_group03m_male_05" },
    { "Medic Rebel Female 01", "models/humans/group03m/female_01.mdl", "humans_group03m_female_01" },
    { "Medic Rebel Female 02", "models/humans/group03m/female_02.mdl", "humans_group03m_female_02" },
    { "Medic Rebel Female 03", "models/humans/group03m/female_03.mdl", "humans_group03m_female_03" },
    { "Medic Rebel Female 04", "models/humans/group03m/female_04.mdl", "humans_group03m_female_04" },
    { "Medic Rebel Female 05", "models/humans/group03m/female_05.mdl", "humans_group03m_female_05" }
};
inline unsigned int Count() { return sizeof(kModels)/sizeof(kModels[0]); }
inline const Entry *Find(const char *path) {
    if (!path || !*path || strlen(path) > 95) return 0;
    for (unsigned int i=0; i<Count(); ++i)
        if (!strcmp(path, kModels[i].path)) return &kModels[i];
    return 0;
}
inline bool IsHL2GameDir(const char *dir) {
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
}
}
#endif
