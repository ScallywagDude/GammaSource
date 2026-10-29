#ifndef GAMMA_WEAPON_WORLDMODEL_H
#define GAMMA_WEAPON_WORLDMODEL_H
#pragma once

// Player weapon entities keep their view model on the server for animation
// timing. Portal and third-person draws must use the world model without
// allowing its sequence indices to leak back into weapon prediction.
class CGammaWeaponWorldModelScope
{
public:
	CGammaWeaponWorldModelScope( C_BaseCombatWeapon *weapon ) : m_Weapon( weapon )
	{
		if ( !m_Weapon ) return;
		m_Model = m_Weapon->GetModelIndex();
		m_Sequence = m_Weapon->GetSequence();
		m_Cycle = m_Weapon->GetCycle();
		m_PlaybackRate = m_Weapon->GetPlaybackRate();

		char sequenceName[128] = "";
		CStudioHdr *header = m_Weapon->GetModelPtr();
		if ( header && m_Sequence >= 0 && m_Sequence < header->GetNumSeq() )
			Q_strncpy( sequenceName, header->pSeqdesc( m_Sequence ).pszLabel(), sizeof( sequenceName ) );
		if ( m_Weapon->GetOwner() && m_Weapon->GetOwner()->IsPlayer() )
		{
			const model_t *viewModel = modelinfo->GetModel( modelinfo->GetModelIndex( m_Weapon->GetViewModel() ) );
			studiohdr_t *viewHeader = viewModel ? modelinfo->GetStudiomodel( viewModel ) : NULL;
			if ( viewHeader && m_Sequence >= 0 && m_Sequence < viewHeader->numlocalseq )
				Q_strncpy( sequenceName, viewHeader->pLocalSeqdesc( m_Sequence )->pszLabel(), sizeof( sequenceName ) );
		}

		const int worldModel = m_Weapon->GetWorldModelIndex();
		if ( worldModel > 0 && worldModel != m_Model ) m_Weapon->SetModelIndex( worldModel );

		// A numeric view-model sequence has no meaning in the world model.
		// Prefer an identically named animation, then a valid world-model idle.
		int sequence = sequenceName[0] ? m_Weapon->LookupSequence( sequenceName ) : -1;
		if ( sequence < 0 ) sequence = m_Weapon->LookupSequence( "idle" );
		if ( sequence < 0 ) sequence = m_Weapon->LookupSequence( "idle1" );
		if ( sequence < 0 ) sequence = 0;
		m_Weapon->SetSequence( sequence );
		m_Weapon->SetCycle( m_Cycle );
		m_Weapon->SetPlaybackRate( m_PlaybackRate );
		m_Weapon->InvalidateBoneCache();
	}

	~CGammaWeaponWorldModelScope()
	{
		if ( !m_Weapon ) return;
		if ( m_Weapon->GetModelIndex() != m_Model ) m_Weapon->SetModelIndex( m_Model );
		m_Weapon->SetSequence( m_Sequence );
		m_Weapon->SetCycle( m_Cycle );
		m_Weapon->SetPlaybackRate( m_PlaybackRate );
		m_Weapon->InvalidateBoneCache();
	}

private:
	C_BaseCombatWeapon *m_Weapon;
	int m_Model, m_Sequence;
	float m_Cycle, m_PlaybackRate;
};

#endif
