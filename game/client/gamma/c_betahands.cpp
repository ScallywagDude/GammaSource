//========= GammaSource ============//
//
// Beta hands on retail viewmodels.
//
// The 2018 weapons we added (weapon_357, weapon_ar2future, ...) use retail HL2 viewmodels,
// whose hands are skinned with the materials/models/weapons/v_hand/v_hand_sheet material.
// GammaSource's content layer replaces that material with a $no_draw one, and this file
// draws the 2003 beta hands instead: a "donor" beta viewmodel with its weapon materials
// hidden, whose bones follow the retail viewmodel's arm bones.
//
// scripts/gamma_betahands.txt says, per retail viewmodel, which donor to use and how the
// bones map:
//   "models/weapons/v_357.mdl"
//   {
//       "donor"   "models/weapons/v_betahands_pistol.mdl"
//       "bones"   { "Bip01 R Hand" "@Hand" }   // '@': align the bind poses instead of copying
//   }
// Donor bones named "Bip01 X Y" also map automatically to "Bip01_X_Y" and
// "ValveBiped.Bip01_X_Y". Donor bones without a match keep their bind pose relative to
// a mapped child or parent.
//
//=============================================================================//
#include "cbase.h"
#include "c_baseviewmodel.h"
#include "c_baseplayer.h"
#include "c_baseanimating.h"
#include "studio.h"
#include "bone_setup.h"
#include "filesystem.h"
#include "KeyValues.h"
#include "igamesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Retain the old name for config compatibility, but 2003 arms are mandatory.
// Old archived `cl_betahands 0` values must not disable the default hands.
static ConVar cl_betahands( "cl_betahands", "1", FCVAR_NONE, "Use the mandatory 2003 beta hands", true, 1.0f, true, 1.0f );
static ConVar cl_betahands_debug( "cl_betahands_debug", "0", 0, "Print the beta hands bone mapping" );
// live tuning of the '@' bones: added to the rule's "align_pos" / "align_ang"
static ConVar cl_betahands_tweak_pos( "cl_betahands_tweak_pos", "0 0 0", 0, "Extra offset (x y z) for the aligned beta hands bones, in the viewmodel bone's space" );
static ConVar cl_betahands_tweak_ang( "cl_betahands_tweak_ang", "0 0 0", 0, "Extra rotation (pitch yaw roll) for the aligned beta hands bones" );

//-----------------------------------------------------------------------------
// Rules from scripts/gamma_betahands.txt
//-----------------------------------------------------------------------------
struct BetaHandsRule_t
{
	CUtlString m_Donor;
	KeyValues *m_pBones;	// donor bone -> retail bone (owned by the file's KeyValues)
	Vector m_vecOffset;		// donor-space offset, for hand-tuning
};

static KeyValues *s_pRulesFile = NULL;
static CUtlMap< C_BaseViewModel *, int > s_Checked( DefLessFunc( C_BaseViewModel * ) );	// model index last looked up

static KeyValues *BetaHands_FindRule( const char *pszViewModel )
{
	if ( !s_pRulesFile )
	{
		s_pRulesFile = new KeyValues( "BetaHands" );
		if ( !s_pRulesFile->LoadFromFile( g_pFullFileSystem, "scripts/gamma_betahands.txt", "GAME" ) )
			DevWarning( "Beta hands: scripts/gamma_betahands.txt not found\n" );
	}
	for ( KeyValues *pRule = s_pRulesFile->GetFirstTrueSubKey(); pRule; pRule = pRule->GetNextTrueSubKey() )
	{
		if ( !Q_stricmp( pRule->GetName(), pszViewModel ) )
			return pRule;
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// The hands model, drawn in the viewmodel pass with bones taken from the viewmodel
//-----------------------------------------------------------------------------
class C_GammaBetaHands : public C_BaseAnimating
{
	DECLARE_CLASS( C_GammaBetaHands, C_BaseAnimating );

public:
	C_GammaBetaHands() : m_pRule( NULL ), m_iRuleModelIndex( -1 ), m_iVMRefModel( -1 ), m_nMappedBones( 0 ), m_bRetired( false ) {}

	// Hands are swapped from inside the viewmodel's DrawModel, while the view renderer is
	// still walking a list that holds these hands. Deleting them there left a dangling
	// pointer in that list (the crash on killing NPCs). Retired hands stop drawing, leave the
	// leaf system, and are deleted after the frame (GammaBetaHands_FlushRetired).
	void Retire()
	{
		m_bRetired = true;
		m_hViewModel = NULL;
		RemoveFromLeafSystem();
	}

	bool Init( C_BaseViewModel *pViewModel, KeyValues *pRule )
	{
		m_hViewModel = pViewModel;
		// the server never precaches the donor; load it on the client
		const char *pszDonor = pRule->GetString( "donor" );
		if ( modelinfo->GetModelIndex( pszDonor ) == -1 )
			modelinfo->RegisterDynamicModel( pszDonor, true );
		if ( !InitializeAsClientEntity( pszDonor, RENDER_GROUP_VIEW_MODEL_OPAQUE ) )
			return false;
		m_iRuleModelIndex = pViewModel->GetModelIndex();
		m_vecOffset.Init();
		const char *pszOffset = pRule->GetString( "offset", "0 0 0" );
		sscanf( pszOffset, "%f %f %f", &m_vecOffset.x, &m_vecOffset.y, &m_vecOffset.z );
		m_vecAlign.Init();
		m_angAlign.Init();
		sscanf( pRule->GetString( "align_pos", "0 0 0" ), "%f %f %f", &m_vecAlign.x, &m_vecAlign.y, &m_vecAlign.z );
		sscanf( pRule->GetString( "align_ang", "0 0 0" ), "%f %f %f", &m_angAlign.x, &m_angAlign.y, &m_angAlign.z );
		m_pRule = pRule;	// the bone map is built once the (dynamically loaded) donor is in
		return true;
	}

	int ViewModelModelIndex() const { return m_iRuleModelIndex; }
	bool Follows( C_BaseViewModel *pViewModel ) const { return m_hViewModel.Get() == pViewModel; }
	int MappedBones() const { return m_nMappedBones; }

	virtual RenderGroup_t GetRenderGroup() { return RENDER_GROUP_VIEW_MODEL_OPAQUE; }
	virtual bool IsViewModel() const { return true; }

	virtual bool ShouldDraw()
	{
		C_BaseViewModel *pVM = m_hViewModel.Get();
		return !m_bRetired && cl_betahands.GetBool() && pVM && pVM->ShouldDraw() &&
			!pVM->IsEffectActive( EF_NODRAW ) && pVM->GetModelIndex() == m_iRuleModelIndex;
	}

	virtual int DrawModel( int flags )
	{
		if ( !ShouldDraw() )
			return 0;
		C_BaseViewModel *pVM = m_hViewModel.Get();
		if ( !pVM )
			return 0;
		if ( !m_Map.Count() && m_pRule )
			BuildMap( m_pRule );
		// Dynamic models may not be ready yet. Never draw an unretargeted bind pose.
		if ( !m_nMappedBones )
			return 0;
		// follow the viewmodel so the render bounds and lighting origin are right
		SetAbsOrigin( pVM->GetAbsOrigin() );
		SetAbsAngles( pVM->GetAbsAngles() );
		InvalidateBoneCache();
		return BaseClass::DrawModel( flags );
	}

	virtual void BuildTransformations( CStudioHdr *pStudioHdr, Vector *pos, Quaternion q[], const matrix3x4_t &cameraTransform, int boneMask, CBoneBitList &boneComputed )
	{
		C_BaseViewModel *pVM = m_hViewModel.Get();
		if ( !pVM || !pStudioHdr )
		{
			BaseClass::BuildTransformations( pStudioHdr, pos, q, cameraTransform, boneMask, boneComputed );
			return;
		}

		if ( !m_Map.Count() && m_pRule )
			BuildMap( m_pRule );

		if ( !pVM->SetupBones( NULL, -1, BONE_USED_BY_ANYTHING, gpGlobals->curtime ) )
		{
			BaseClass::BuildTransformations( pStudioHdr, pos, q, cameraTransform, boneMask, boneComputed );
			return;
		}
		CStudioHdr *pVMHdr = pVM->GetModelPtr();
		int numBones = pStudioHdr->numbones();
		CUtlVector< bool > done;
		done.SetCount( numBones );

		// reference poses (model space): the donor's and the viewmodel's idle animation, which
		// both hold the weapon in front of the camera (the bind poses can be anything)
		if ( m_DonorRef.Count() != numBones )
			ComputeIdlePose( pStudioHdr, m_DonorRef );
		if ( pVMHdr && ( m_VMRef.Count() != pVMHdr->numbones() || m_iVMRefModel != pVM->GetModelIndex() ) )
		{
			ComputeIdlePose( pVMHdr, m_VMRef );
			m_iVMRefModel = pVM->GetModelIndex();
		}
		const CUtlVector< matrix3x4_t > &bind = m_DonorRef;
		// Unmapped arms still need to reload, draw and recoil with the weapon.
		// A fixed idle pose made AR2's hands remain frozen through every animation.
		CUtlVector< matrix3x4_t > animated;
		int donorSequence = -1;
		if ( pVMHdr && pVM->GetSequence() >= 0 && pVM->GetSequence() < pVMHdr->GetNumSeq() )
		{
			const char *activity = pVMHdr->pSeqdesc( pVM->GetSequence() ).pszActivityName();
			for ( int s = 0; activity[0] && s < pStudioHdr->GetNumSeq(); s++ )
			{
				if ( !Q_stricmp( activity, pStudioHdr->pSeqdesc( s ).pszActivityName() ) )
				{
					donorSequence = s;
					break;
				}
			}
		}
		if ( donorSequence >= 0 )
			ComputePose( pStudioHdr, donorSequence, pVM->GetCycle(), animated );
		else
			animated = bind;

		Vector anchorOffset( 0, 0, 0 );
		const char *anchor = m_pRule ? m_pRule->GetString( "anchor", "" ) : "";
		for ( int i = 0; anchor[0] && i < numBones; i++ )
		{
			int j = m_Map[i].m_iTarget;
			if ( j >= 0 && j < m_VMRef.Count() && !Q_stricmp( anchor, pStudioHdr->pBone( i )->pszName() ) )
			{
				Vector target, donor;
				MatrixPosition( m_VMRef[j], target );
				MatrixPosition( bind[i], donor );
				anchorOffset = target - donor;
				break;
			}
		}
		for ( int i = 0; i < numBones; i++ )
			done[i] = false;

		matrix3x4_t offset;
		SetIdentityMatrix( offset );
		MatrixSetColumn( m_vecOffset, 3, offset );

		// correction for the explicitly mapped bones (rule "align_pos"/"align_ang" plus the tweak cvars)
		matrix3x4_t align;
		{
			Vector vecTweak( 0, 0, 0 );
			QAngle angTweak( 0, 0, 0 );
			sscanf( cl_betahands_tweak_pos.GetString(), "%f %f %f", &vecTweak.x, &vecTweak.y, &vecTweak.z );
			sscanf( cl_betahands_tweak_ang.GetString(), "%f %f %f", &angTweak.x, &angTweak.y, &angTweak.z );
			AngleMatrix( m_angAlign + angTweak, m_vecAlign + vecTweak, align );
		}

		// 1. mapped bones
		for ( int i = 0; i < numBones && i < m_Map.Count(); i++ )
		{
			int j = m_Map[i].m_iTarget;
			if ( j < 0 || !pVMHdr || j >= pVMHdr->numbones() )
				continue;
			matrix3x4_t vmBone;
			pVM->GetCachedBoneMatrix( j, vmBone );
			if ( m_Map[i].m_bExplicit )
			{
				matrix3x4_t vmAligned;
				ConcatTransforms( vmBone, align, vmAligned );
				vmBone = vmAligned;
			}
			matrix3x4_t &out = GetBoneForWrite( i );
			if ( m_Map[i].m_bAlignBind )
			{
				// keep the donor bone where it is relative to the viewmodel bone in their idle poses
				matrix3x4_t k, invVMRef;
				MatrixInvert( m_VMRef[j], invVMRef );
				matrix3x4_t donorRef = bind[i];
				for ( int axis = 0; axis < 3; axis++ )
					donorRef[axis][3] += anchorOffset[axis];
				ConcatTransforms( invVMRef, donorRef, k );
				ConcatTransforms( vmBone, k, out );
			}
			else
			{
				out = vmBone;
			}
			// Crossbow uses a different two-hand grip. Preserve the donor's bone
			// axes while placing each mapped joint at its animated target joint.
			if ( m_Map[i].m_bAlignBind && !m_Map[i].m_bExplicit && m_pRule && m_pRule->GetBool( "target_positions", false ) )
				for ( int axis = 0; axis < 3; ++axis )
					out[axis][3] = vmBone[axis][3];
			matrix3x4_t withOffset;
			ConcatTransforms( out, offset, withOffset );
			out = withOffset;
			done[i] = true;
		}

		// 2. unmapped bones: from a done child (bottom-up), else from the parent (top-down)
		for ( int pass = 0; pass < numBones; pass++ )
		{
			bool bChanged = false;
			for ( int i = numBones - 1; i >= 0; i-- )
			{
				if ( done[i] )
					continue;
				for ( int c = 0; c < numBones; c++ )
				{
					if ( done[c] && pStudioHdr->pBone( c )->parent == i )
					{
						// parent = child * inverse( child relative to parent at bind )
						matrix3x4_t invChildBind, rel, invRel;
						MatrixInvert( animated[c], invChildBind );
						ConcatTransforms( invChildBind, animated[i], invRel );	// parent in child space
						ConcatTransforms( GetBoneForWrite( c ), invRel, rel );
						GetBoneForWrite( i ) = rel;
						done[i] = bChanged = true;
						break;
					}
				}
			}
			for ( int i = 0; i < numBones; i++ )
			{
				int p = pStudioHdr->pBone( i )->parent;
				if ( done[i] || p < 0 || !done[p] )
					continue;
				matrix3x4_t invParentBind, rel, out;
				MatrixInvert( animated[p], invParentBind );
				ConcatTransforms( invParentBind, animated[i], rel );
				ConcatTransforms( GetBoneForWrite( p ), rel, out );
				GetBoneForWrite( i ) = out;
				done[i] = bChanged = true;
			}
			if ( !bChanged )
				break;
		}

		// 3. anything still unplaced (no mapped bone at all): bind pose at the viewmodel origin
		matrix3x4_t vmToWorld;
		AngleMatrix( pVM->GetAbsAngles(), pVM->GetAbsOrigin(), vmToWorld );
		for ( int i = 0; i < numBones; i++ )
		{
			if ( done[i] )
				continue;
			matrix3x4_t tmp, out;
			ConcatTransforms( animated[i], offset, tmp );
			ConcatTransforms( vmToWorld, tmp, out );
			GetBoneForWrite( i ) = out;
		}

		for ( int i = 0; i < numBones; i++ )
		{
			// A one-arm target (Portal) cannot drive the donor's second arm.
			// Collapse explicitly hidden branches so they produce no stray geometry.
			if ( m_Map[i].m_bHidden )
			{
				matrix3x4_t &out = GetBoneForWrite( i );
				for ( int axis = 0; axis < 3; axis++ )
				{
					out[axis][0] = out[axis][1] = out[axis][2] = 0.0f;
					out[axis][3] = pVM->GetAbsOrigin()[axis];
				}
			}
			boneComputed.Set( i );
		}
	}

private:
	struct BoneMap_t
	{
		int m_iTarget;
		bool m_bAlignBind;	// '@': keep the idle-pose relation instead of copying the bone
		bool m_bExplicit;	// named in the rule's "bones": gets the align correction
		bool m_bHidden;
	};

	static int FindVMBone( CStudioHdr *pVMHdr, const char *pszName )
	{
		for ( int j = 0; j < pVMHdr->numbones(); j++ )
		{
			if ( !Q_stricmp( pVMHdr->pBone( j )->pszName(), pszName ) )
				return j;
		}
		return -1;
	}

	void BuildMap( KeyValues *pRule )
	{
		CStudioHdr *pHdr = GetModelPtr();
		CStudioHdr *pVMHdr = m_hViewModel.Get() ? m_hViewModel->GetModelPtr() : NULL;
		m_Map.RemoveAll();
		m_nMappedBones = 0;
		if ( !pHdr || !pVMHdr )
			return;
		KeyValues *pBones = pRule->FindKey( "bones" );
		KeyValues *pHide = pRule->FindKey( "hide_bones" );
		for ( int i = 0; i < pHdr->numbones(); i++ )
		{
			const char *pszDonor = pHdr->pBone( i )->pszName();
			BoneMap_t map = { -1, pRule->GetBool( "retarget", false ), false, false };
			int parent = pHdr->pBone( i )->parent;
			map.m_bHidden = ( pHide && pHide->GetBool( pszDonor, false ) ) ||
				( parent >= 0 && parent < m_Map.Count() && m_Map[parent].m_bHidden );
			const char *pszExplicit = pBones ? pBones->GetString( pszDonor, NULL ) : NULL;
			if ( pszExplicit && pszExplicit[0] )
			{
				map.m_bAlignBind = ( pszExplicit[0] == '@' );
				map.m_bExplicit = true;
				map.m_iTarget = FindVMBone( pVMHdr, pszExplicit + ( map.m_bAlignBind ? 1 : 0 ) );
			}
			else if ( !Q_strnicmp( pszDonor, "Bip01 ", 6 ) && Q_stricmp( pszDonor, "Bip01" ) )
			{
				char szName[128];
				Q_strncpy( szName, pszDonor, sizeof( szName ) );
				for ( char *c = szName; *c; c++ )
				{
					if ( *c == ' ' )
						*c = '_';
				}
				map.m_iTarget = FindVMBone( pVMHdr, szName );
				if ( map.m_iTarget < 0 )
				{
					char szValve[160];
					Q_snprintf( szValve, sizeof( szValve ), "ValveBiped.%s", szName );
					map.m_iTarget = FindVMBone( pVMHdr, szValve );
				}
				if ( map.m_iTarget < 0 && pRule->GetString( "bone_prefix", "" )[0] )
				{
					char prefixed[192];
					Q_snprintf( prefixed, sizeof( prefixed ), "%s%s", pRule->GetString( "bone_prefix" ), szName );
					map.m_iTarget = FindVMBone( pVMHdr, prefixed );
				}
			}
			if ( map.m_iTarget >= 0 && !map.m_bHidden )
				m_nMappedBones++;
			if ( cl_betahands_debug.GetBool() )
				Msg( "beta hands: %s -> %s%s\n", pszDonor, map.m_iTarget >= 0 ? pVMHdr->pBone( map.m_iTarget )->pszName() : "(bind)", map.m_bAlignBind ? " (aligned)" : "" );
			m_Map.AddToTail( map );
		}
	}

	CHandle< C_BaseViewModel > m_hViewModel;
	KeyValues *m_pRule;
	int m_iRuleModelIndex;
	Vector m_vecOffset;
	Vector m_vecAlign;	// rule "align_pos" / "align_ang": correction in the viewmodel bone's space for '@' bones
	QAngle m_angAlign;
	CUtlVector< BoneMap_t > m_Map;
	CUtlVector< matrix3x4_t > m_DonorRef, m_VMRef;
	int m_iVMRefModel;
	int m_nMappedBones;
	bool m_bRetired;

	// model-space bone matrices of the model's ACT_VM_IDLE sequence at cycle 0 (bind pose if it has none)
	static void ComputeIdlePose( CStudioHdr *pHdr, CUtlVector< matrix3x4_t > &out )
	{
		int numBones = pHdr->numbones();
		out.SetCount( numBones );
		int iSeq = -1;
		for ( int s = 0; s < pHdr->GetNumSeq() && iSeq < 0; s++ )
		{
			if ( !Q_stricmp( pHdr->pSeqdesc( s ).pszActivityName(), "ACT_VM_IDLE" ) )
				iSeq = s;
		}
		if ( iSeq < 0 )
		{
			for ( int i = 0; i < numBones; i++ )
				MatrixInvert( pHdr->pBone( i )->poseToBone, out[i] );
			return;
		}
		ComputePose( pHdr, iSeq, 0.0f, out );
	}

	static void ComputePose( CStudioHdr *pHdr, int sequence, float cycle, CUtlVector< matrix3x4_t > &out )
	{
		int numBones = pHdr->numbones();
		out.SetCount( numBones );
		float poseParams[MAXSTUDIOPOSEPARAM];
		for ( int p = 0; p < MAXSTUDIOPOSEPARAM; p++ )
			poseParams[p] = 0.5f;
		Vector *pos = (Vector *)stackalloc( numBones * sizeof( Vector ) );
		Quaternion *q = (Quaternion *)stackalloc( numBones * sizeof( Quaternion ) );
		IBoneSetup boneSetup( pHdr, BONE_USED_BY_ANYTHING, poseParams );
		boneSetup.InitPose( pos, q );
		boneSetup.AccumulatePose( pos, q, sequence, cycle, 1.0f, 0.0f, NULL );
		for ( int i = 0; i < numBones; i++ )
		{
			matrix3x4_t local;
			QuaternionMatrix( q[i], pos[i], local );
			int parent = pHdr->pBone( i )->parent;
			if ( parent < 0 )
				out[i] = local;
			else
				ConcatTransforms( out[parent], local, out[i] );
		}
	}
};

//-----------------------------------------------------------------------------
// One set of hands per viewmodel (the player has two viewmodel slots)
//-----------------------------------------------------------------------------
static CUtlMap< C_BaseViewModel *, C_GammaBetaHands * > s_Hands( DefLessFunc( C_BaseViewModel * ) );
static CUtlVector< C_GammaBetaHands * > s_RetiredHands;

static void GammaBetaHands_Retire( C_GammaBetaHands *pHands )
{
	pHands->Retire();
	s_RetiredHands.AddToTail( pHands );
}

static void GammaBetaHands_FlushRetired()
{
	for ( int i = 0; i < s_RetiredHands.Count(); i++ )
		s_RetiredHands[i]->Release();
	s_RetiredHands.RemoveAll();
}

// Deletes retired hands once no render list can still point at them
class CGammaBetaHandsSystem : public CAutoGameSystemPerFrame
{
public:
	CGammaBetaHandsSystem() : CAutoGameSystemPerFrame( "CGammaBetaHandsSystem" ) {}
	virtual void PostRender() { GammaBetaHands_FlushRetired(); }
	// Before the entity list releases every client entity: drop the hands here, so the
	// viewmodels' UpdateOnRemove never reaches hands the list already deleted
	virtual void LevelShutdownPreEntity()
	{
		for ( unsigned short i = s_Hands.FirstInorder(); i != s_Hands.InvalidIndex(); i = s_Hands.NextInorder( i ) )
			GammaBetaHands_Retire( s_Hands[i] );
		s_Hands.RemoveAll();
		s_Checked.RemoveAll();
		GammaBetaHands_FlushRetired();
	}
	virtual void Shutdown() { GammaBetaHands_FlushRetired(); }
};
static CGammaBetaHandsSystem s_GammaBetaHandsSystem;

void GammaBetaHands_Update( C_BaseViewModel *pViewModel )
{
	unsigned short idx = s_Hands.Find( pViewModel );
	C_GammaBetaHands *pHands = ( idx != s_Hands.InvalidIndex() ) ? s_Hands[idx] : NULL;

	if ( pHands && pHands->Follows( pViewModel ) && pHands->ViewModelModelIndex() == pViewModel->GetModelIndex() )
		return;

	if ( pHands )
	{
		GammaBetaHands_Retire( pHands );
		s_Hands.RemoveAt( idx );
		pHands = NULL;
		s_Checked.Remove( pViewModel );
	}

	const model_t *pModel = pViewModel->GetModel();
	if ( !pModel )
		return;

	// look the rule up once per viewmodel model change
	unsigned short iChecked = s_Checked.Find( pViewModel );
	if ( iChecked != s_Checked.InvalidIndex() && s_Checked[iChecked] == pViewModel->GetModelIndex() )
		return;
	s_Checked.InsertOrReplace( pViewModel, pViewModel->GetModelIndex() );

	KeyValues *pRule = BetaHands_FindRule( modelinfo->GetModelName( pModel ) );
	if ( cl_betahands_debug.GetBool() )
		Msg( "beta hands: viewmodel %s, donor %s\n", modelinfo->GetModelName( pModel ), pRule ? pRule->GetString( "donor" ) : "(none)" );
	if ( !pRule )
		return;

	pHands = new C_GammaBetaHands;
	if ( !pHands->Init( pViewModel, pRule ) )
	{
		Warning( "beta hands: could not load %s\n", pRule->GetString( "donor" ) );
		pHands->Release();
		return;
	}
	s_Hands.Insert( pViewModel, pHands );
}

void GammaBetaHands_Remove( C_BaseViewModel *pViewModel )
{
	s_Checked.Remove( pViewModel );
	unsigned short idx = s_Hands.Find( pViewModel );
	if ( idx == s_Hands.InvalidIndex() )
		return;
	GammaBetaHands_Retire( s_Hands[idx] );
	s_Hands.RemoveAt( idx );
}

CON_COMMAND( gamma_viewmodelinfo, "Print the local viewmodel, sequence and beta hands mapping" )
{
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if ( !player )
		return;
	for ( int slot = 0; slot < 2; slot++ )
	{
		C_BaseViewModel *vm = player->GetViewModel( slot );
		if ( !vm || !vm->GetModel() )
			continue;
		CStudioHdr *hdr = vm->GetModelPtr();
		int sequence = vm->GetSequence();
		bool valid = hdr && sequence >= 0 && sequence < hdr->GetNumSeq();
		unsigned short index = s_Hands.Find( vm );
		C_GammaBetaHands *hands = index != s_Hands.InvalidIndex() ? s_Hands[index] : NULL;
		Msg( "gamma_viewmodelinfo slot=%d model=%s bones=%d sequence=%d label=%s activity=%s cycle=%.3f hands=%s mapped=%d\n",
			slot, modelinfo->GetModelName( vm->GetModel() ), hdr ? hdr->numbones() : 0, sequence,
			valid ? hdr->pSeqdesc( sequence ).pszLabel() : "invalid",
			valid ? hdr->pSeqdesc( sequence ).pszActivityName() : "invalid", vm->GetCycle(),
			hands && hands->GetModel() ? modelinfo->GetModelName( hands->GetModel() ) : "none",
			hands ? hands->MappedBones() : 0 );
	}
}

CON_COMMAND( cl_betahands_reload, "Reload scripts/gamma_betahands.txt" )
{
	for ( unsigned short i = s_Hands.FirstInorder(); i != s_Hands.InvalidIndex(); i = s_Hands.NextInorder( i ) )
		GammaBetaHands_Retire( s_Hands[i] );
	s_Hands.RemoveAll();
	s_Checked.RemoveAll();
	if ( s_pRulesFile )
		s_pRulesFile->deleteThis();
	s_pRulesFile = NULL;
}
