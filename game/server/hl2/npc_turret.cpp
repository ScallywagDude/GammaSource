//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// TODO: 
//		Take advantage of new NPC fields like GetEnemy() and get rid of that OFFSET() stuff
//		Revisit enemy validation stuff, maybe it's not necessary with the newest NPC code
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "Sprite.h"
#include "basecombatweapon.h"
#include "ai_basenpc.h"
#include "AI_Senses.h"
#include "AI_Memory.h"
#include "gamerules.h"
#include "ammodef.h"
#include "ndebugoverlay.h"
#include "IEffects.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "filesystem.h"
#include "te_effect_dispatch.h"
#include "tier0/memdbgon.h"

class CSprite;

#define TURRET_RANGE	(100 * 12)
#define TURRET_SPREAD	VECTOR_CONE_5DEGREES
#define TURRET_TURNRATE	360		// max angles per second
#define TURRET_MAXWAIT	15		// seconds turret will stay active w/o a target
#define TURRET_MAXSPIN	5		// seconds turret barrel will spin w/o a target
#define TURRET_MACHINE_VOLUME	0.5

#define TURRET_BC_YAW	"aim_yaw"
#define TURRET_BC_PITCH	"aim_pitch"

#define TURRET_ORIENTATION_FLOOR 0
#define TURRET_ORIENTATION_CEILING 1

//=========================================================
// private activities
//=========================================================
int ACT_TURRET_OPEN;
int ACT_TURRET_CLOSE;
int ACT_TURRET_OPEN_IDLE;
int ACT_TURRET_CLOSED_IDLE;
int ACT_TURRET_FIRE;
int ACT_TURRET_RELOAD;

// ===============================================
//  Private spawn flags  (must be above (1<<15))
// ===============================================
#define SF_NPC_TURRET_AUTOACTIVATE		0x00000020
#define SF_NPC_TURRET_STARTINACTIVE		0x00000040

extern short		g_sModelIndexSmoke;			// (in combatweapon.cpp) holds the index for the smoke cloud

ConVar	sk_miniturret_health( "sk_miniturret_health","10");
ConVar	sk_sentry_health( "sk_sentry_health","10");
ConVar	sk_turret_health( "sk_turret_health","50");

class CBetaBaseTurret : public CAI_BaseNPC
{
	DECLARE_CLASS( CBetaBaseTurret, CAI_BaseNPC );
public:
	CBetaBaseTurret();
	void Spawn(void);
	virtual void Precache(void);
	bool KeyValue( const char *szKeyName, const char *szValue );
	//void TurretUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	
	virtual void		TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *accumulator = NULL );
	virtual int			OnTakeDamage( const CTakeDamageInfo &info );
	virtual Class_T		Classify(void);

	int BloodColor( void ) { return DONT_BLEED; }
	bool Event_Gibbed( const CTakeDamageInfo &info ) { return FALSE; }	// UNDONE: Throw turret gibs?

	Vector	EyeOffset( Activity nActivity );
	Vector	EyePosition( void );

	// Inputs
	void InputToggle( inputdata_t &inputdata );
	void InputEnable( inputdata_t &inputdata );
	void InputDisable( inputdata_t &inputdata );
	void UpdateOnRemove();
	const char *SelectModel( const char *preferred );
	bool MuzzleTransform( Vector &position, QAngle &angles );
	void EmitTurretSound( const char *legacy, const char *fallback );

	// Think functions

	void ActiveThink(void);
	void SearchThink(void);
	void AutoSearchThink(void);
	void TurretDeath(void);

	void Deploy(void);
	void Retire(void);
	
	void Initialize(void);

	virtual void Ping(void);
	virtual void EyeOn(void);
	virtual void EyeOff(void);

	DECLARE_DATADESC();

	// other functions
	int MoveTurret(void);
	virtual void Shoot(const Vector &vecSrc, const Vector &vecDirToEnemy) { };

	CSprite *m_pEyeGlow;
	int		m_eyeBrightness;
	bool m_bLegacyTurretSounds;
	bool m_bLegacyGunSounds;

	int	m_iDeployHeight;
	int	m_iRetractHeight;
	int m_iMinPitch;

	int m_iBaseTurnRate;	// angles per second
	float m_fTurnRate;		// actual turn rate
	int	m_iOn;
	int m_fBeserk;			// Sometimes this bitch will just freak out
	int m_iAutoStart;		// true if the turret auto deploys when a target
							// enters its range

	Vector	m_vecLastSight;	// Last seen position
	float	m_flLastSight;	// Last time we saw a target
	float	m_flMaxWait;	// Max time to search w/o a target

	// movement
	float	m_flStartYaw;
	QAngle	m_vecGoalAngles;

	int		m_iAmmoType;

	float	m_flPingTime;	// Time until the next ping, used when searching
	float	m_flDamageTime;	// Time we last took damage.

	COutputEvent m_OnDeploy;
	COutputEvent m_OnRetire;

	// external
	//COutputEvent		m_OnDamaged;
	//COutputEvent		m_OnDeath;
	//COutputEvent		m_OnHalfHealth;
	//COutputEHANDLE		m_OnFoundEnemy; 
	//COutputEvent		m_OnLostEnemyLOS; 
	//COutputEvent		m_OnLostEnemy; 
	//COutputEHANDLE		m_OnFoundPlayer;
	//COutputEvent		m_OnLostPlayerLOS;
	//COutputEvent		m_OnLostPlayer; 
	//COutputEvent		m_OnHearWorld;
	//COutputEvent		m_OnHearPlayer;
	//COutputEvent		m_OnHearCombat;

};


BEGIN_DATADESC( CBetaBaseTurret )

	DEFINE_FIELD( m_pEyeGlow, FIELD_CLASSPTR ),
	DEFINE_FIELD( m_eyeBrightness, FIELD_INTEGER ),
	DEFINE_FIELD( m_bLegacyTurretSounds, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bLegacyGunSounds, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_iDeployHeight, FIELD_INTEGER ),
	DEFINE_FIELD( m_iRetractHeight, FIELD_INTEGER ),
	DEFINE_FIELD( m_iMinPitch, FIELD_INTEGER ),

	DEFINE_FIELD( m_iBaseTurnRate, FIELD_INTEGER ),
	DEFINE_FIELD( m_fTurnRate, FIELD_FLOAT ),
	DEFINE_FIELD( m_iOn, FIELD_INTEGER ),
	DEFINE_FIELD( m_fBeserk, FIELD_INTEGER ),
	DEFINE_FIELD( m_iAutoStart, FIELD_INTEGER ),

	DEFINE_FIELD( m_vecLastSight, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_flLastSight, FIELD_TIME ),
	DEFINE_FIELD( m_flMaxWait, FIELD_FLOAT ),

	DEFINE_FIELD( m_flStartYaw, FIELD_FLOAT ),
	DEFINE_FIELD( m_vecGoalAngles, FIELD_VECTOR ),

	DEFINE_FIELD( m_iAmmoType, FIELD_INTEGER ),

	DEFINE_FIELD( m_flPingTime, FIELD_TIME ),
	DEFINE_FIELD( m_flDamageTime, FIELD_TIME ),

	// Function pointers
	//DEFINE_USEFUNC( TurretUse ),
	DEFINE_THINKFUNC( ActiveThink ),
	DEFINE_THINKFUNC( SearchThink ),
	DEFINE_THINKFUNC( AutoSearchThink ),
	DEFINE_THINKFUNC( TurretDeath ),
	DEFINE_THINKFUNC( Deploy ),
	DEFINE_THINKFUNC( Retire ),
	DEFINE_THINKFUNC( Initialize ),

	// Inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "Toggle", InputToggle ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),
	
	// Outputs
	DEFINE_OUTPUT(m_OnDeploy, "OnDeploy"),
	DEFINE_OUTPUT(m_OnRetire, "OnRetire"),

END_DATADESC()


CBetaBaseTurret::CBetaBaseTurret()
{
	m_pEyeGlow = NULL;
	m_eyeBrightness = 0;
	m_bLegacyTurretSounds = m_bLegacyGunSounds = false;
	m_iDeployHeight = m_iRetractHeight = m_iMinPitch = 0;
	m_iBaseTurnRate = 0;
	m_fTurnRate = 0;
	m_iOn = m_fBeserk = m_iAutoStart = 0;
	m_vecLastSight.Init();
	m_flLastSight = m_flMaxWait = m_flStartYaw = 0;
	m_vecGoalAngles.Init();
	m_iAmmoType = -1;
	m_flPingTime = m_flDamageTime = 0;
}



//------------------------------------------------------------------------------
// Purpose : 
// Input   :
// Output  :
//------------------------------------------------------------------------------
Vector	CBetaBaseTurret::EyeOffset( Activity nActivity )
{
	return Vector( 0, 0, -20 );
}


static bool BetaTurretWaveExists( const char *wave )
{
	char path[MAX_PATH];
	Q_snprintf( path, sizeof(path), "sound/%s", wave );
	return filesystem->FileExists( path, "GAME" );
}

const char *CBetaBaseTurret::SelectModel( const char *preferred )
{
	// Preserve explicit map model keys and prefer the recovered original art.
	if ( GetModelName() != NULL_STRING && STRING(GetModelName())[0] )
		return STRING(GetModelName());
	if ( filesystem->FileExists(preferred,"GAME") ) return preferred;
	return "models/Sentry_Guns/Floor_Sentry.mdl";
}

bool CBetaBaseTurret::MuzzleTransform( Vector &position, QAngle &angles )
{
	int attachment = LookupAttachment("eyes");
	if ( !attachment ) attachment = LookupAttachment("0");
	if ( !attachment ) attachment = 1;
	if ( GetAttachment(attachment,position,angles) ) return true;
	position = WorldSpaceCenter();
	angles = GetAbsAngles();
	return false;
}

Vector CBetaBaseTurret::EyePosition( void )
{
	Vector position;
	QAngle angles;
	MuzzleTransform(position,angles);
	return position;
}

void CBetaBaseTurret::EmitTurretSound( const char *legacy, const char *fallback )
{
	EmitSound(m_bLegacyTurretSounds ? legacy : fallback);
}

void CBetaBaseTurret::UpdateOnRemove()
{
	if ( m_pEyeGlow ) { UTIL_Remove(m_pEyeGlow); m_pEyeGlow = NULL; }
	BaseClass::UpdateOnRemove();
}

bool CBetaBaseTurret::KeyValue( const char *szKeyName, const char *szValue )
{
	if (FStrEq(szKeyName, "maxsleep"))
	{
		m_flMaxWait = atof(szValue);
	}
	else if (FStrEq(szKeyName, "turnrate"))
	{
		m_iBaseTurnRate = atoi(szValue);
	}
	else if (FStrEq(szKeyName, "style") ||
			 FStrEq(szKeyName, "height") ||
			 FStrEq(szKeyName, "value1") ||
			 FStrEq(szKeyName, "value2") ||
			 FStrEq(szKeyName, "value3"))
	{
	}
	else
		return BaseClass::KeyValue( szKeyName, szValue );

	return true;
}


void CBetaBaseTurret::Spawn()
{ 
	Precache( );
	BaseClass::Spawn();
	SetNavType( NAV_FLY );
	CapabilitiesClear();
	m_lifeState = LIFE_ALIVE;
	m_iMaxHealth = m_iHealth;
	m_NPCState = NPC_STATE_IDLE;
	SetBloodColor( DONT_BLEED );
	SetDistLook( TURRET_RANGE );
	SetBoneCacheFlags( BCF_NO_ANIMATION_SKIP );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetNextThink( gpGlobals->curtime + 1 );
	SetMoveType( MOVETYPE_FLY );
	SetSequence( 0 );
	SetCycle( 0 );
	SetSolid( SOLID_BBOX );
	m_takedamage		= DAMAGE_YES;
	AddFlag( FL_AIMTARGET );

	m_iAmmoType = GetAmmoDef()->Index("SMG1");

	AddFlag( FL_NPC );

	if (( m_spawnflags & SF_NPC_TURRET_AUTOACTIVATE ) && !( m_spawnflags & SF_NPC_TURRET_STARTINACTIVE ))
	{
		m_iAutoStart = true;
	}

	ResetSequenceInfo( );

	SetPoseParameter( TURRET_BC_YAW, 0 );
	SetPoseParameter( TURRET_BC_PITCH, 0 );

}


void CBetaBaseTurret::Precache( )
{
	// Register the original activity names before a model can cache its IDs.
	ADD_CUSTOM_ACTIVITY( CBetaBaseTurret, ACT_TURRET_OPEN );
	ADD_CUSTOM_ACTIVITY( CBetaBaseTurret, ACT_TURRET_CLOSE );
	ADD_CUSTOM_ACTIVITY( CBetaBaseTurret, ACT_TURRET_CLOSED_IDLE );
	ADD_CUSTOM_ACTIVITY( CBetaBaseTurret, ACT_TURRET_OPEN_IDLE );
	ADD_CUSTOM_ACTIVITY( CBetaBaseTurret, ACT_TURRET_FIRE );
	ADD_CUSTOM_ACTIVITY( CBetaBaseTurret, ACT_TURRET_RELOAD );
	BaseClass::Precache();
	m_bLegacyTurretSounds = BetaTurretWaveExists("turret/tu_ping.wav") &&
		BetaTurretWaveExists("turret/tu_deploy.wav") && BetaTurretWaveExists("turret/tu_alert.wav") &&
		BetaTurretWaveExists("turret/tu_die.wav") && BetaTurretWaveExists("turret/tu_die2.wav") && BetaTurretWaveExists("turret/tu_die3.wav");
	m_bLegacyGunSounds = BetaTurretWaveExists("weapons/hks1.wav") &&
		BetaTurretWaveExists("weapons/hks2.wav") && BetaTurretWaveExists("weapons/hks3.wav");
	if ( m_bLegacyTurretSounds )
	{
		PrecacheScriptSound("NPC_Turret.Ping"); PrecacheScriptSound("NPC_Turret.Deploy");
		PrecacheScriptSound("NPC_Turret.Retire"); PrecacheScriptSound("NPC_Turret.Alert"); PrecacheScriptSound("NPC_Turret.Die");
	}
	// These are actual supplied 2003 assets, used only when the older waves are absent.
	PrecacheSound("npc/turret_floor/ping.wav"); PrecacheSound("npc/turret_floor/deploy.wav");
	PrecacheSound("npc/turret_floor/retract.wav"); PrecacheSound("npc/turret_floor/alert.wav");
	PrecacheSound("npc/turret_floor/die.wav"); PrecacheSound("npc/entity/turret/turret_fire1.wav");
}

void CBetaBaseTurret::Initialize(void)
{
	m_iOn = 0;
	m_fBeserk = 0;

	SetPoseParameter( TURRET_BC_YAW, 0 );
	SetPoseParameter( TURRET_BC_PITCH, 0 );

	if (m_iBaseTurnRate == 0) m_iBaseTurnRate = TURRET_TURNRATE;
	if (m_flMaxWait == 0) m_flMaxWait = TURRET_MAXWAIT;

	m_vecGoalAngles = GetAbsAngles();

	if (m_iAutoStart)
	{
		m_flLastSight = gpGlobals->curtime + m_flMaxWait;
		SetThink( &CBetaBaseTurret::AutoSearchThink );		
		SetNextThink( gpGlobals->curtime + .1 );
	}
	else
		SetThink( NULL );
}


//-----------------------------------------------------------------------------
// Purpose: Input handler for toggling the turret on/off.
//-----------------------------------------------------------------------------
void CBetaBaseTurret::InputToggle( inputdata_t &inputdata )
{
	if ( m_lifeState != LIFE_ALIVE ) return;
	//if ( !ShouldToggle( useType, m_iOn ) )
	//	return;

	if (m_iOn)
	{
		SetEnemy( NULL );
		SetNextThink( gpGlobals->curtime + 0.1f );
		m_iAutoStart = FALSE;// switching off a turret disables autostart
		//!!!! this should spin down first!!BUGBUG
		SetThink( &CBetaBaseTurret::Retire );
	}
	else 
	{
		SetNextThink( gpGlobals->curtime + 0.1f ); // turn on delay

		// if the turret is flagged as an autoactivate turret, re-enable its ability open self.
		if ( m_spawnflags & SF_NPC_TURRET_AUTOACTIVATE )
		{
			m_iAutoStart = TRUE;
		}
		
		SetThink( &CBetaBaseTurret::Deploy );
	}
}


void CBetaBaseTurret::InputEnable( inputdata_t &inputdata )
{
	if ( m_lifeState != LIFE_ALIVE || m_iOn ) return;
	m_iAutoStart = HasSpawnFlags(SF_NPC_TURRET_AUTOACTIVATE);
	SetThink( &CBetaBaseTurret::Deploy );
	SetNextThink( gpGlobals->curtime + 0.1f );
}

void CBetaBaseTurret::InputDisable( inputdata_t &inputdata )
{
	if ( m_lifeState != LIFE_ALIVE ) return;
	m_iAutoStart = false;
	SetEnemy(NULL);
	if ( m_iOn ) SetThink( &CBetaBaseTurret::Retire );
	else SetThink( NULL );
	SetNextThink( gpGlobals->curtime + 0.1f );
}

void CBetaBaseTurret::Ping( void )
{
	// make the pinging noise every second while searching
	if (m_flPingTime == 0)
		m_flPingTime = gpGlobals->curtime + 1;
	else if (m_flPingTime <= gpGlobals->curtime)
	{
		m_flPingTime = gpGlobals->curtime + 1;
		EmitTurretSound( "NPC_Turret.Ping", "npc/turret_floor/ping.wav" );
		EyeOn( );
	}
	else if (m_eyeBrightness > 0)
	{
		EyeOff( );
	}
}


void CBetaBaseTurret::EyeOn( )
{
	if (m_pEyeGlow)
	{
		if (m_eyeBrightness != 255)
		{
			m_eyeBrightness = 255;
		}
		m_pEyeGlow->SetBrightness( m_eyeBrightness );
	}
}


void CBetaBaseTurret::EyeOff( )
{
	if (m_pEyeGlow)
	{
		if (m_eyeBrightness > 0)
		{
			m_eyeBrightness = MAX( 0, m_eyeBrightness - 30 );
			m_pEyeGlow->SetBrightness( m_eyeBrightness );
		}
	}
}


void CBetaBaseTurret::ActiveThink(void)
{
	int fAttack = 0;
	Vector vecDirToEnemy;

	SetNextThink( gpGlobals->curtime + 0.1f );
	StudioFrameAdvance( );

	if ((!m_iOn) || (GetEnemy() == NULL))
	{
		SetEnemy( NULL );
		m_flLastSight = gpGlobals->curtime + m_flMaxWait;
		SetThink( &CBetaBaseTurret::SearchThink );
		return;
	}
	
	// if it's dead, look for something new
	if ( !GetEnemy()->IsAlive() )
	{
		if (!m_flLastSight)
		{
			m_flLastSight = gpGlobals->curtime + 0.5; // continue-shooting timeout
		}
		else
		{
			if (gpGlobals->curtime > m_flLastSight)
			{	
				SetEnemy( NULL );
				m_flLastSight = gpGlobals->curtime + m_flMaxWait;
				SetThink( &CBetaBaseTurret::SearchThink );
				return;
			}
		}
	}

	Vector vecMid = EyePosition( );
	Vector vecMidEnemy = GetEnemy()->BodyTarget(vecMid);

	// g_pEffects->Sparks( vecMid );
	// g_pEffects->Sparks( vecMidEnemy );

	// Look for our current enemy
	//int fEnemyVisible = FBoxVisible( this, GetEnemy(), vecMidEnemy );	
	int fEnemyVisible = FInViewCone( GetEnemy() ) && FVisible( GetEnemy() );	

	vecDirToEnemy = vecMidEnemy - vecMid;	// calculate dir and dist to enemy
	// NDebugOverlay::Line( vecMid, vecMidEnemy, 0, 255, 0, false, 0.1 );
	
	float flDistToEnemy = vecDirToEnemy.Length();

	QAngle vecAnglesToEnemy;
	VectorNormalize( vecDirToEnemy );
	VectorAngles( vecDirToEnemy, vecAnglesToEnemy );

	// Current enmey is not visible.
	if (!fEnemyVisible || (flDistToEnemy > TURRET_RANGE))
	{
		// DevMsg( "lost you\n" );

		if (!m_flLastSight)
		{
			m_flLastSight = gpGlobals->curtime + 0.5;
		}
		else
		{
			// Should we look for a new target?
			if (gpGlobals->curtime > m_flLastSight)
			{
				ClearEnemyMemory();
				SetEnemy( NULL );
				m_flLastSight = gpGlobals->curtime + m_flMaxWait;
				SetThink( &CBetaBaseTurret::SearchThink );
				return;
			}
		}
		fEnemyVisible = 0;
	}
	else
	{
		m_vecLastSight = vecMidEnemy;
	}

	Vector vecLOS = vecDirToEnemy; //vecMid - m_vecLastSight;
	VectorNormalize( vecLOS );

	Vector vecMuzzle, vecMuzzleDir;
	QAngle vecMuzzleAng;
	MuzzleTransform( vecMuzzle, vecMuzzleAng );

	AngleVectors( vecMuzzleAng, &vecMuzzleDir );
	
	// Is the Gun looking at the target
	if (DotProduct(vecLOS, vecMuzzleDir) <= 0.9848) // 10 degree slop
	{
		fAttack = FALSE;
	}
	else
	{
		fAttack = TRUE;
	}

	// fire the gun
	if (fAttack || m_fBeserk)
	{
		ResetActivity();
		SetActivity( (Activity)ACT_TURRET_FIRE );
		Shoot(vecMuzzle, vecMuzzleDir );
	} 
	else
	{
		SetActivity( (Activity)ACT_TURRET_OPEN_IDLE );
	}

	//move the gun
	if (m_fBeserk)
	{
		// DevMsg( "berserk" );

		if (random->RandomInt(0,9) == 0)
		{
			m_vecGoalAngles.y = random->RandomFloat(-180,180);
			m_vecGoalAngles.x = random->RandomFloat(-90,90);
			OnTakeDamage( CTakeDamageInfo( this, this, 1, DMG_GENERIC ) ); // don't beserk forever
			return;
		}
	} 
	else if (fEnemyVisible)
	{
		// DevMsg( "->[%.2f]\n", vec.x);
		m_vecGoalAngles.y = vecAnglesToEnemy.y;
		m_vecGoalAngles.x = vecAnglesToEnemy.x;

	}

	MoveTurret();
}


void CBetaBaseTurret::Deploy(void)
{
	SetNextThink( gpGlobals->curtime + 0.1f );
	StudioFrameAdvance( );

	if ( GetActivity() != ACT_TURRET_OPEN )
	{
		m_iOn = 1;
		SetActivity( (Activity)ACT_TURRET_OPEN );
		EmitTurretSound( "NPC_Turret.Deploy", "npc/turret_floor/deploy.wav" );

		m_OnDeploy.FireOutput(NULL, this);
	}

	if (IsSequenceFinished())
	{
		Vector curmins, curmaxs;
		curmins = WorldAlignMins();
		curmaxs = WorldAlignMaxs();

		curmaxs.z = m_iDeployHeight;
		curmins.z = -m_iDeployHeight;

		SetCollisionBounds( curmins, curmaxs );



		SetActivity( (Activity)ACT_TURRET_OPEN_IDLE );

		m_flPlaybackRate = 0;
		SetThink( &CBetaBaseTurret::SearchThink );
	}

	m_flLastSight = gpGlobals->curtime + m_flMaxWait;
}

void CBetaBaseTurret::Retire(void)
{
	// make the turret level
	m_vecGoalAngles = GetAbsAngles( );

	SetNextThink( gpGlobals->curtime + 0.1f );

	StudioFrameAdvance( );

	EyeOff( );

	if ( GetActivity() != ACT_TURRET_CLOSE )
	{
		SetActivity( (Activity)ACT_TURRET_OPEN_IDLE );
		
		if (!MoveTurret())
		{
			SetActivity( (Activity)ACT_TURRET_CLOSE );
			EmitTurretSound( "NPC_Turret.Retire", "npc/turret_floor/retract.wav" );

			m_OnRetire.FireOutput(NULL, this);
		}
	}
	else if (IsSequenceFinished())
	{	
		m_iOn = 0;
		m_flLastSight = 0;

		SetActivity( (Activity)ACT_TURRET_CLOSED_IDLE );

		Vector curmins, curmaxs;
		curmins = WorldAlignMins();
		curmaxs = WorldAlignMaxs();

		curmaxs.z = m_iRetractHeight;
		curmins.z = -m_iRetractHeight;

		SetCollisionBounds( curmins, curmaxs );


		if (m_iAutoStart)
		{
			SetThink( &CBetaBaseTurret::AutoSearchThink );		
			SetNextThink( gpGlobals->curtime + .1 );
		}
		else
		{
			SetThink( NULL );
		}
	}
}


//
// This search function will sit with the turret deployed and look for a new target. 
// After a set amount of time, the barrel will spin down. After m_flMaxWait, the turret will
// retact.
//
void CBetaBaseTurret::SearchThink(void)
{
	// ensure rethink
	SetActivity( (Activity)ACT_TURRET_OPEN_IDLE );

	StudioFrameAdvance( );
	SetNextThink( gpGlobals->curtime + 0.1f );

	Ping( );

	// If we have a target and we're still healthy
	if (GetEnemy() != NULL)
	{
		if (!GetEnemy()->IsAlive() )
			SetEnemy( NULL );// Dead enemy forces a search for new one
	}

	// Acquire Target
	if (GetEnemy() == NULL)
	{
		GetSenses()->Look(TURRET_RANGE);
		SetEnemy( BestEnemy() );
	}

	// If we've found a target, spin up the barrel and start to attack
	if (GetEnemy() != NULL)
	{
		m_flLastSight = 0;
		SetThink( &CBetaBaseTurret::ActiveThink );
	}
	else
	{
		// Are we out of time, do we need to retract?
 		if (gpGlobals->curtime > m_flLastSight)
		{
			//Before we retrace, make sure that we are spun down.
			m_flLastSight = 0;
			SetThink( &CBetaBaseTurret::Retire );
		}
		
		// generic hunt for new victims
		m_vecGoalAngles.y = (m_vecGoalAngles.y + 0.1 * m_iBaseTurnRate);
		if (m_vecGoalAngles.y >= 360)
			m_vecGoalAngles.y -= 360;

		MoveTurret();
	}
}


// 
// This think function will deploy the turret when something comes into range. This is for
// automatically activated turrets.
//
void CBetaBaseTurret::AutoSearchThink(void)
{
	// ensure rethink
	StudioFrameAdvance( );
	SetNextThink( gpGlobals->curtime + random->RandomFloat( 0.2, 0.3 ) );

	// If we have a target and we're still healthy

	if (GetEnemy() != NULL)
	{
		if (!GetEnemy()->IsAlive() )
			SetEnemy( NULL );// Dead enemy forces a search for new one
	}

	// Acquire Target

	if (GetEnemy() == NULL)
	{
		GetSenses()->Look( TURRET_RANGE );
		SetEnemy( BestEnemy() );
	}

	if (GetEnemy() != NULL)
	{
		SetThink( &CBetaBaseTurret::Deploy );
		EmitTurretSound( "NPC_Turret.Alert", "npc/turret_floor/alert.wav" );
	}
}


void CBetaBaseTurret ::	TurretDeath( void )
{
	StudioFrameAdvance( );
	SetNextThink( gpGlobals->curtime + 0.1f );

	if (m_lifeState != LIFE_DEAD)
	{
		m_lifeState = LIFE_DEAD;

		EmitTurretSound( "NPC_Turret.Die", "npc/turret_floor/die.wav" );

		SetActivity( (Activity)ACT_TURRET_CLOSE );

		EyeOn( );	
	}

	EyeOff( );

	if (m_flDamageTime + random->RandomFloat( 0, 2 ) > gpGlobals->curtime)
	{
		// lots of smoke
		Vector pos;
		CollisionProp()->RandomPointInBounds( vec3_origin, Vector( 1, 1, 1 ), &pos );
		pos.z = CollisionProp()->GetCollisionOrigin().z;
		
		CBroadcastRecipientFilter filter;
		te->Smoke( filter, 0.0, &pos,
			g_sModelIndexSmoke,
			2.5,
			10 );
	}
	
	if (m_flDamageTime + random->RandomFloat( 0, 5 ) > gpGlobals->curtime)
	{
		Vector vecSrc;
		CollisionProp()->RandomPointInBounds( vec3_origin, Vector( 1, 1, 1 ), &vecSrc );
		g_pEffects->Sparks( vecSrc );
	}

	if (IsSequenceFinished() && !MoveTurret( ) && m_flDamageTime + 5 < gpGlobals->curtime)
	{
		m_flPlaybackRate = 0;
		SetThink( NULL );
	}
}



void CBetaBaseTurret::TraceAttack( const CTakeDamageInfo &inputInfo, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *accumulator )
{
	CTakeDamageInfo info = inputInfo;

	if ( ptr->hitgroup == 10 )
	{
		// hit armor
		if ( m_flDamageTime != gpGlobals->curtime || (random->RandomInt(0,10) < 1) )
		{
			g_pEffects->Ricochet( ptr->endpos, (vecDir*-1.0f) );
			m_flDamageTime = gpGlobals->curtime;
		}

		info.SetDamage( 0.1 );// don't hurt the NPC much, but allow bits_COND_LIGHT_DAMAGE to be generated
	}

	if ( !m_takedamage )
		return;

	AddMultiDamage( info, this );
}


int CBetaBaseTurret::OnTakeDamage( const CTakeDamageInfo &inputInfo )
{
	if ( !m_takedamage )
		return 0;

	CTakeDamageInfo info = inputInfo;

	if (!m_iOn)
		info.ScaleDamage( 0.1f );

	m_iHealth -= info.GetDamage();
	if (m_iHealth <= 0)
	{
		m_iHealth = 0;
		m_takedamage = DAMAGE_NO;
		m_flDamageTime = gpGlobals->curtime;

		RemoveFlag( FL_NPC ); // why are they set in the first place???

		SetThink( &CBetaBaseTurret::TurretDeath );

		m_OnDamaged.FireOutput( info.GetInflictor(), this );
		m_OnDeath.FireOutput( info.GetAttacker(), this );

		SetNextThink( gpGlobals->curtime + 0.1f );

		return 0;
	}

	if (m_iHealth <= 10)
	{
		if (m_iOn && (1 || random->RandomInt(0, 0x7FFF) > 800))
		{
			m_fBeserk = 1;
			SetThink( &CBetaBaseTurret::SearchThink );
		}
	}

	return 1;
}


int CBetaBaseTurret::MoveTurret(void)
{
	// The recovered Floor_Sentry art has one XR pitch pose and numbered
	// attachments. Turn its fixed base for yaw; keep the original model's
	// two-pose aiming path when the preferred model is supplied.
	if ( LookupPoseParameter(TURRET_BC_YAW) < 0 )
	{
		QAngle angles = GetAbsAngles();
		float oldYaw = angles.y;
		angles.y = UTIL_ApproachAngle(m_vecGoalAngles.y,angles.y,0.1f*m_iBaseTurnRate);
		SetAbsAngles(angles);
		Vector muzzle; QAngle aim;
		MuzzleTransform(muzzle,aim);
		float pitchDelta = AngleNormalize(m_vecGoalAngles.x-aim.x);
		int pose = LookupPoseParameter("XR");
		if ( pose >= 0 ) SetPoseParameter(pose,GetPoseParameter(pose)+clamp(pitchDelta,-0.1f*m_iBaseTurnRate,0.1f*m_iBaseTurnRate)/1.5f);
		return fabs(AngleNormalize(angles.y-oldYaw))>0.1f || fabs(pitchDelta)>0.1f;
	}

	bool bDidMove = false;
	int iPose;

	matrix3x4_t localToWorld;
	
	Vector aimPosition; QAngle aimAngles;
	MuzzleTransform(aimPosition,aimAngles);
	AngleMatrix(aimAngles,aimPosition,localToWorld);

	Vector vecGoalDir;
	AngleVectors( m_vecGoalAngles, &vecGoalDir );

	Vector vecGoalLocalDir;
	VectorIRotate( vecGoalDir, localToWorld, vecGoalLocalDir );

	QAngle vecGoalLocalAngles;
	VectorAngles( vecGoalLocalDir, vecGoalLocalAngles );

	float flDiff;
	QAngle vecNewAngles;

  // update pitch
	flDiff = AngleNormalize( UTIL_ApproachAngle(  vecGoalLocalAngles.x, 0.0, 0.1 * m_iBaseTurnRate ) );
	iPose = LookupPoseParameter( TURRET_BC_PITCH );
	SetPoseParameter( iPose, GetPoseParameter( iPose ) + flDiff / 1.5 );

	if (fabs(flDiff) > 0.1)
	{
		bDidMove = true;
	}

	// update yaw, with acceleration
#if 0
	float flDist = AngleNormalize( vecGoalLocalAngles.y );
	float flNewDist;
	float flNewTurnRate;

	ChangeDistance( 0.1, flDist, 0.0, m_fTurnRate, m_iBaseTurnRate, m_iBaseTurnRate * 4, flNewDist, flNewTurnRate );
	m_fTurnRate = flNewTurnRate;
	flDiff = flDist - flNewDist;
#else
	flDiff = AngleNormalize( UTIL_ApproachAngle(  vecGoalLocalAngles.y, 0.0, 0.1 * m_iBaseTurnRate ) );
#endif

	iPose = LookupPoseParameter( TURRET_BC_YAW );
	SetPoseParameter( iPose, GetPoseParameter( iPose ) + flDiff / 1.5 );
	if (fabs(flDiff) > 0.1)
	{
		bDidMove = true;
	}

	if (bDidMove)
	{
		// DevMsg( "(%.2f, %.2f)\n", AngleNormalize( vecGoalLocalAngles.x ), AngleNormalize( vecGoalLocalAngles.y ) );
	}
	return bDidMove;
}

//
// ID as a machine
//
Class_T	CBetaBaseTurret::Classify ( void )

{
	if (m_iOn || m_iAutoStart)
	{
		return	CLASS_MILITARY;
	}

	return CLASS_MILITARY;
}




//////////////////////////////////////////////////////////////////////////////////////////////////////


// The active original ceiling implementation remains in npc_turret_ceiling.cpp.
#if 0
class CCeilingTurret : public CBetaBaseTurret
{
	DECLARE_CLASS( CCeilingTurret, CBetaBaseTurret );
public:
	void Spawn(void);
	void Precache(void);

	// other functions
	void Shoot( const Vector &vecSrc, const Vector &vecDirToEnemy );
};

#define TURRET_GLOW_SPRITE "sprites/glow01.vmt"

LINK_ENTITY_TO_CLASS( npc_turret_ceiling, CCeilingTurret );

void CCeilingTurret::Spawn()
{ 
	Precache( );

	SetModel( "models/combine_turrets/ceiling_turret.mdl" );
	
	BaseClass::Spawn( );

	m_iHealth			= sk_turret_health.GetFloat();
	m_HackedGunPos		= Vector( 0, 0, 12.75 );

	AngleVectors( GetAbsAngles(), NULL, NULL, &m_vecViewOffset );
	m_vecViewOffset		= m_vecViewOffset * Vector( 0, 0, -64 );

	m_flFieldOfView		= VIEW_FIELD_FULL;

	m_iRetractHeight = 16;
	m_iDeployHeight = 32;
	m_iMinPitch	= -45;
	UTIL_SetSize(this, Vector(-32, -32, -m_iRetractHeight), Vector(32, 32, m_iRetractHeight));
	
	SetThink( &CBetaBaseTurret::Initialize );	

	m_pEyeGlow = CSprite::SpriteCreate( TURRET_GLOW_SPRITE, GetAbsOrigin(), FALSE );
	m_pEyeGlow->SetTransparency( kRenderGlow, 255, 0, 0, 0, kRenderFxNoDissipation );
	m_pEyeGlow->SetAttachment( this, 2 );
	m_eyeBrightness = 0;

	SetNextThink( gpGlobals->curtime + 0.3f );
}

void CCeilingTurret::Precache()
{
	PrecacheModel( "models/combine_turrets/ceiling_turret.mdl");	
	PrecacheModel( TURRET_GLOW_SPRITE );

	PrecacheScriptSound( "CeilingTurret.Shoot" );
	BaseClass::Precache();
}

void CCeilingTurret::Shoot(const Vector &vecSrc, const Vector &vecDirToEnemy)
{
	//NDebugOverlay::Line( vecSrc, vecSrc + vecDirToEnemy * 512, 0, 255, 255, false, 0.1 );
	FireBullets( 1, vecSrc, vecDirToEnemy, TURRET_SPREAD, TURRET_RANGE, m_iAmmoType, 1 );
	EmitSound( "CeilingTurret.Shoot" );
	
	DoMuzzleFlash();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

#endif

class CBetaMiniTurret : public CBetaBaseTurret
{
	DECLARE_CLASS( CBetaMiniTurret, CBetaBaseTurret );
public:
	void Spawn( );
	void Precache(void);
	// other functions
	void Shoot(const Vector &vecSrc, const Vector &vecDirToEnemy);
};

LINK_ENTITY_TO_CLASS( npc_miniturret, CBetaMiniTurret );


void CBetaMiniTurret::Spawn()
{ 
	Precache( );
	SetModel( SelectModel("models/miniturret.mdl") );
	m_iHealth = GetHealth() > 0 ? GetHealth() : MAX(1,sk_miniturret_health.GetInt());
	m_HackedGunPos		= Vector( 0, 0, 12.75 );
	SetViewOffset( Vector(0,0,12.75f) );
	m_flFieldOfView		= VIEW_FIELD_NARROW;

	CBetaBaseTurret::Spawn( );

	m_iAmmoType = GetAmmoDef()->Index("Pistol");

	m_iRetractHeight = 16;
	m_iDeployHeight = 32;
	m_iMinPitch	= -45;
	UTIL_SetSize(this, Vector(-16, -16, -m_iRetractHeight), Vector(16, 16, m_iRetractHeight));

	SetThink( &CBetaBaseTurret::Initialize );	
	SetNextThink( gpGlobals->curtime + 0.3f );
}


void CBetaMiniTurret::Precache()
{
	BaseClass::Precache();
	PrecacheModel( SelectModel("models/miniturret.mdl") );	

	if ( BetaTurretWaveExists("weapons/hks1.wav") && BetaTurretWaveExists("weapons/hks2.wav") && BetaTurretWaveExists("weapons/hks3.wav") ) PrecacheScriptSound( "MiniTurret.Shoot" );

}



void CBetaMiniTurret::Shoot(const Vector &vecSrc, const Vector &vecDirToEnemy)
{
	FireBullets( 1, vecSrc, vecDirToEnemy, TURRET_SPREAD, TURRET_RANGE, m_iAmmoType, 1 );

	EmitSound( m_bLegacyGunSounds ? "MiniTurret.Shoot" : "npc/entity/turret/turret_fire1.wav" );

	DoMuzzleFlash();
}





//=========================================================
// Sentry gun - smallest turret, placed near grunt entrenchments
//=========================================================
class CBetaSentry : public CBetaBaseTurret
{
	DECLARE_CLASS( CBetaSentry, CBetaBaseTurret );
public:
	void Spawn( );
	void Precache(void);
	// other functions
	void Shoot(const Vector &vecSrc, const Vector &vecDirToEnemy);
	int OnTakeDamage( const CTakeDamageInfo &info );
	void SentryTouch( CBaseEntity *pOther );
	void SentryDeath( void );

protected:

	DECLARE_DATADESC();
};


BEGIN_DATADESC( CBetaSentry )

	// Function pointers
	DEFINE_ENTITYFUNC( SentryTouch ),
	DEFINE_THINKFUNC( SentryDeath ),

END_DATADESC()


LINK_ENTITY_TO_CLASS( npc_sentry, CBetaSentry );

void CBetaSentry::Precache()
{
	BaseClass::Precache();
	PrecacheModel( SelectModel("models/sentry.mdl") );	

	if ( BetaTurretWaveExists("weapons/hks1.wav") && BetaTurretWaveExists("weapons/hks2.wav") && BetaTurretWaveExists("weapons/hks3.wav") ) PrecacheScriptSound( "Sentry.Shoot" );
	if ( BetaTurretWaveExists("turret/tu_die.wav") ) PrecacheScriptSound( "Sentry.Die" );

}

void CBetaSentry::Spawn()
{ 
	Precache( );
	SetModel( SelectModel("models/sentry.mdl") );
	m_iHealth = GetHealth() > 0 ? GetHealth() : MAX(1,sk_sentry_health.GetInt());
	m_HackedGunPos		= Vector( 0, 0, 48 );
	SetViewOffset( Vector(0,0,48) );
	m_flMaxWait = 1E6;

	CBetaBaseTurret::Spawn();
	m_iRetractHeight = 64;
	m_iDeployHeight = 64;
	m_iMinPitch	= -60;
	UTIL_SetSize(this, Vector(-16, -16, -m_iRetractHeight), Vector(16, 16, m_iRetractHeight));

	SetTouch( &CBetaSentry::SentryTouch );
	SetThink( &CBetaBaseTurret::Initialize );	
	SetNextThink( gpGlobals->curtime + 0.3f );
}

void CBetaSentry::Shoot(const Vector &vecSrc, const Vector &vecDirToEnemy)
{
	FireBullets( 1, vecSrc, vecDirToEnemy, TURRET_SPREAD, TURRET_RANGE, m_iAmmoType, 1 );

	EmitSound( m_bLegacyGunSounds ? "Sentry.Shoot" : "npc/entity/turret/turret_fire1.wav" );

	DoMuzzleFlash();
}


int CBetaSentry::OnTakeDamage( const CTakeDamageInfo &info )
{
	if ( !m_takedamage )
		return 0;

	if (!m_iOn)
	{
		SetThink( &CBetaBaseTurret::Deploy );
		SetNextThink( gpGlobals->curtime + 0.1f );
	}

	m_iHealth -= info.GetDamage();
	if (m_iHealth <= 0)
	{
		m_iHealth = 0;
		m_takedamage = DAMAGE_NO;
		m_flDamageTime = gpGlobals->curtime;

		RemoveFlag( FL_NPC ); // why are they set in the first place???

		SetThink( &CBetaSentry::SentryDeath );
		m_OnDamaged.FireOutput( info.GetInflictor(), this );
		m_OnDeath.FireOutput( info.GetAttacker(), this );
		SetNextThink( gpGlobals->curtime + 0.1f );

		return 0;
	}

	return 1;
}


void CBetaSentry::SentryTouch( CBaseEntity *pOther )
{
	if ( pOther && (pOther->IsPlayer() || (pOther->GetFlags() & FL_NPC)) )
	{
		OnTakeDamage( CTakeDamageInfo( pOther, pOther, 0, 0 ) );
	}
}


void CBetaSentry ::	SentryDeath( void )
{
	StudioFrameAdvance( );
	SetNextThink( gpGlobals->curtime + 0.1f );

	if (m_lifeState != LIFE_DEAD)
	{
		m_lifeState = LIFE_DEAD;

		EmitTurretSound( "Sentry.Die", "npc/turret_floor/die.wav" );

		SetPoseParameter( TURRET_BC_YAW, 0 );
		SetPoseParameter( TURRET_BC_PITCH, 0 );

		SetActivity( (Activity)ACT_TURRET_CLOSE );

		SetSolid( SOLID_NONE );
		QAngle angles = GetAbsAngles();
		angles.y = UTIL_AngleMod( GetAbsAngles().y + random->RandomInt( 0, 2 ) * 120 );
		SetAbsAngles( angles );

		EyeOn( );
	}

	EyeOff( );

	Vector vecSrc;
	QAngle vecAng;
	MuzzleTransform( vecSrc, vecAng );

	if (m_flDamageTime + random->RandomFloat( 0, 2 ) > gpGlobals->curtime)
	{
		// lots of smoke
		Vector pos = vecSrc + Vector( random->RandomFloat( -16, 16 ), 
				random->RandomFloat( -16, 16 ),
				 -32 );

		CBroadcastRecipientFilter filter;
		te->Smoke( filter, 0.0, &pos,
			g_sModelIndexSmoke,
			1.5,
			8 );
	}
	
	if (m_flDamageTime + random->RandomFloat( 0, 8 ) > gpGlobals->curtime)
	{
		g_pEffects->Sparks( vecSrc );
	}

	if (IsSequenceFinished() && m_flDamageTime + 5 < gpGlobals->curtime)
	{
		m_flPlaybackRate = 0;
		SetThink( NULL );
	}
}

