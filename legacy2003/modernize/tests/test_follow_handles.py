"""Execute actual follower handle methods without packing checked iterators."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


def method(text, signature):
    start = text.index(signature)
    opening = text.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


class FollowHandles(unittest.TestCase):
    def test_node_identity_survives_insert_and_checked_removal(self):
        native = shutil.which('cl') if os.name == 'nt' else None
        compiler = native or shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('Portable C++ compiler required')
        text = (ROOT / 'dlls/ai_behavior_follow.cpp').read_bytes().decode('latin1').replace('\r\n', '\n')
        methods = '\n'.join(method(text, signature) for signature in (
            'AI_Follower_t *CAI_FollowManager::FindFollower(',
            'AI_FollowManagerInfoHandle_t CAI_FollowManager::AddFollower(',
            'void CAI_FollowManager::RemoveFollower('))
        source = '''#include <list>
#include <vector>
#include <assert.h>
#include <stddef.h>
using std::list;
struct CBaseEntity {}; struct CAI_BaseNPC {};
struct Opaque; typedef Opaque *AI_FollowManagerInfoHandle_t;
struct AI_FollowSlot_t { int value; };
struct Formation { AI_FollowSlot_t *pSlots; };
struct AI_FollowGroup_t;
struct AI_Follower_t { CAI_BaseNPC *hFollower; int slot; AI_FollowGroup_t *pGroup; int navInfo; };
struct Slots { unsigned used; Slots():used(0) {} void SetBit(int i){used|=1u<<i;} void ClearBit(int i){used&=~(1u<<i);} };
struct AI_FollowGroup_t { list<AI_Follower_t> followers; Formation *pFormation; Slots slotUsage; };
typedef list<AI_Follower_t>::iterator FollowerListIter_t;
struct Groups : std::vector<AI_FollowGroup_t*> { int Count() const {return (int)size();} };
class CAI_FollowManager { public:
 Groups m_groups;
 AI_Follower_t *FindFollower(AI_FollowManagerInfoHandle_t);
 AI_FollowManagerInfoHandle_t AddFollower(CBaseEntity*,CAI_BaseNPC*);
 void RemoveFollower(AI_FollowManagerInfoHandle_t);
 AI_FollowGroup_t *FindCreateGroup(CBaseEntity*) {return m_groups[0];}
 int FindBestSlot(AI_FollowGroup_t *g,CAI_BaseNPC*) {for(int i=0;i<3;++i)if(!(g->slotUsage.used&(1u<<i)))return i;return -1;}
 void CalculateFieldsFromSlot(AI_FollowSlot_t *s,int *out) {*out=s->value;}
 void RemoveGroup(AI_FollowGroup_t *g) {assert(m_groups[0]==g);m_groups.clear();delete g;}
};
''' + methods + '''
int main() {
 AI_FollowSlot_t slots[3]={{10},{20},{30}}; Formation formation={slots};
 CAI_FollowManager manager; AI_FollowGroup_t *group=new AI_FollowGroup_t;
 group->pFormation=&formation;manager.m_groups.push_back(group);
 CBaseEntity target;CAI_BaseNPC a,b,c;
 AI_FollowManagerInfoHandle_t first=manager.AddFollower(&target,&a);
 AI_FollowManagerInfoHandle_t second=manager.AddFollower(&target,&b);
 AI_FollowManagerInfoHandle_t third=manager.AddFollower(&target,&c);
 assert(first&&second&&third&&first!=second&&second!=third);
 assert(manager.FindFollower(first)->hFollower==&a&&manager.FindFollower(first)->navInfo==10);
 assert(manager.AddFollower(&target,&a)==NULL);
 AI_FollowManagerInfoHandle_t invalid=reinterpret_cast<AI_FollowManagerInfoHandle_t>(0x1234);
 assert(manager.FindFollower(invalid)==NULL);manager.RemoveFollower(invalid);
 manager.RemoveFollower(second);assert(manager.FindFollower(first)->hFollower==&a);
 assert(manager.FindFollower(third)->hFollower==&c);assert(group->slotUsage.used==5);
 manager.RemoveFollower(second);assert(group->followers.size()==2);
 manager.RemoveFollower(first);manager.RemoveFollower(third);assert(manager.m_groups.empty());
 manager.RemoveFollower(first);manager.RemoveFollower(NULL);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            unit = path / 'follow.cpp'
            executable = path / ('follow.exe' if os.name == 'nt' else 'follow')
            unit.write_text(source)
            command = ([compiler, '/nologo', '/EHsc', '/MTd', '/D_DEBUG', str(unit),
                        '/Fo' + str(path) + '\\', '/Fe' + str(executable), '/Fd' + str(path / 'test.pdb')]
                       if native else [compiler, '-std=c++98', '-Wall', '-Wextra', '-pedantic',
                                       str(unit), '-o', str(executable)])
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(executable)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
