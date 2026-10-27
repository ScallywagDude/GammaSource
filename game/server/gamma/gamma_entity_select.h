// GammaSource: one map classname that creates either of two entity classes.
// The ConVar is read each time an entity is created, so map entities, spawned
// ones and save games all follow its current value; change it between maps.
#ifndef GAMMA_ENTITY_SELECT_H
#define GAMMA_ENTITY_SELECT_H
#ifdef _WIN32
#pragma once
#endif

#include "util.h"

template <class TOn, class TOff>
class CGammaSelectEntityFactory : public IEntityFactory
{
public:
	CGammaSelectEntityFactory( const char *pClassName, ConVar *pSelector ) : m_pSelector( pSelector )
	{
		EntityFactoryDictionary()->InstallFactory( this, pClassName );
	}

	IServerNetworkable *Create( const char *pClassName )
	{
		if ( m_pSelector->GetBool() )
			return _CreateEntityTemplate( (TOn *)NULL, pClassName )->NetworkProp();
		return _CreateEntityTemplate( (TOff *)NULL, pClassName )->NetworkProp();
	}

	void Destroy( IServerNetworkable *pNetworkable )
	{
		if ( pNetworkable )
		{
			pNetworkable->Release();
		}
	}

	size_t GetEntitySize()
	{
		return sizeof( TOn ) > sizeof( TOff ) ? sizeof( TOn ) : sizeof( TOff );
	}

private:
	ConVar *m_pSelector;
};

#define GAMMA_LINK_ENTITY_TO_CLASS_SELECT( mapClassName, selector, OnClass, OffClass ) \
	static CGammaSelectEntityFactory<OnClass, OffClass> mapClassName( #mapClassName, &selector );

#endif // GAMMA_ENTITY_SELECT_H
