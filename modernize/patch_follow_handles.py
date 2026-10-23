#!/usr/bin/env python3
"""Keep follower handles independent of the standard library iterator layout."""
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]


def patch(root=ROOT):
    path = root / 'dlls/ai_behavior_follow.cpp'
    before = path.read_bytes()
    newline = b'\r\n' if b'\r\n' in before else b'\n'
    text = before.decode('latin1').replace('\r\n', '\n')
    if '// HL2MODERN_FOLLOW_NODE_HANDLES' not in text:
        text = text.replace('ASSERT_INVARIANT( sizeof(FollowerListIter_t) == sizeof(AI_FollowManagerInfoHandle_t) );', '''// HL2MODERN_FOLLOW_NODE_HANDLES: list node addresses survive insertions.
// Debug iterators contain additional tracking state and cannot be packed into a pointer.
AI_Follower_t *CAI_FollowManager::FindFollower(AI_FollowManagerInfoHandle_t handle)
{
    if (!handle) return NULL;
    for (int group=0;group<m_groups.Count();++group)
        for (FollowerListIter_t item=m_groups[group]->followers.begin();
             item!=m_groups[group]->followers.end();++item)
            if ((void *)&*item==(void *)handle) return &*item;
    return NULL;
}''')
        text = text.replace('\tvoid RemoveFollower( AI_FollowManagerInfoHandle_t );',
            '\tvoid RemoveFollower( AI_FollowManagerInfoHandle_t );\n\tAI_Follower_t *FindFollower(AI_FollowManagerInfoHandle_t handle);')
        text = text.replace('return *((AI_FollowManagerInfoHandle_t *)((void *)&iterNode));',
                            'return reinterpret_cast<AI_FollowManagerInfoHandle_t>(&*iterNode);')
        text = text.replace('FollowerListIter_t iterNode = *((FollowerListIter_t *)(&hInfo));',
                            'AI_Follower_t *iterNode=FindFollower(hInfo);')
        text = text.replace('if ( hInfo && iterNode->pGroup )', 'if ( iterNode && iterNode->pGroup )')
        text = text.replace('pGroup->followers.erase( iterNode );', '''FollowerListIter_t item=pGroup->followers.begin();
        while (item!=pGroup->followers.end() && &*item!=iterNode) ++item;
        if (item!=pGroup->followers.end()) pGroup->followers.erase(item);''')
        path.write_bytes(text.encode('latin1').replace(b'\n', newline))
    path = root / 'dlls/tf2_dll/tf_obj.cpp'
    before = path.read_bytes()
    after = before.replace(b'Assert( m_hBuiltOnEntity == GetMoveParent() );',
                           b'Assert( m_hBuiltOnEntity.Get() == GetMoveParent() );')
    if before != after:
        path.write_bytes(after)


if __name__ == '__main__':
    patch()
