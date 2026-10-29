//======177== (C) Copyright 1999, 2000 Valve, L.L.C. All rights reserved. ========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================
#include "quakedef.h"
#include "server.h"
#include "master.h"
#include "proto_oob.h"
#include "host.h"
#include "eiface.h"
#include "server.h"
#include "utlmap.h"

extern ConVar sv_tags;
extern ConVar sv_lan;

#define S2A_EXTRA_DATA_HAS_GAMETAG_DATA                         0x01            // Next bytes are the game tag string
#define RETRY_INFO_REQUEST_TIME 0.4 // seconds
#define MASTER_RESPONSE_TIMEOUT 1.5 // seconds
#define INFO_REQUEST_TIMEOUT 5.0 // seconds
#define MASTER_QUERY_TIMEOUT 3.0 // seconds without any master reply before the masters count as down
#define LAN_SCAN_TIME 1.5 // seconds
#define ADDRESS_QUERY_TIMEOUT 2.5 // seconds
#define LAN_SCAN_RETRIES 3

// GammaSource: LAN servers are found by broadcasting info requests to these ports
static const int g_LanScanPorts[][2] =
{
	{ 27015, 27020 },
	{ 26900, 26905 },
};

enum EServerListRequestType
{
	eRequestInternet = 0,
	eRequestLAN,
	eRequestAddresses,
};

// GammaSource: one in-flight server list query. Several can run at once (the browser
// page, the add server dialog and every game info dialog each own one).
struct ServerListRequest_t
{
	ServerListRequest_t()
	{
		SetDefLessFunc( m_Addresses );
		SetDefLessFunc( m_RequestTimes );
	}

	IServerListResponse *m_pResponse;
	EServerListRequestType m_eType;
	char m_szGameDir[256];

	bool m_bMasterResponded;
	int m_iServersResponded;
	int m_iServersListed; // addresses that came from a master
	int m_iLanScans;

	double m_flStartTime;
	double m_flRetryTime;
	double m_flLastMasterReply;

	CUtlMap<netadr_t, bool> m_Addresses;	// address -> responded
	CUtlMap<uint, double> m_RequestTimes;	// sequence -> send time
};

static char g_MasterServers[][64] =
{
	"185.192.97.130:27010",
	"168.138.92.21:27016",
	"135.125.188.162:27010"
};

#ifdef DEDICATED
#define IsLan() false
#else
#define IsLan() sv_lan.GetInt()
#endif

//-----------------------------------------------------------------------------
// Purpose: List of master servers and some state info about them
//-----------------------------------------------------------------------------
typedef struct adrlist_s
{
	// Next master in chain
	struct adrlist_s	*next;
	// Challenge request sent to master
	qboolean			heartbeatwaiting;
	// Challenge request send time
	float				heartbeatwaitingtime; 
	// Last one is Main master
	int					heartbeatchallenge;
	// Time we sent last heartbeat
	double				last_heartbeat;
	// Master server address
	netadr_t			adr;
} adrlist_t;

//-----------------------------------------------------------------------------
// Purpose: Implements the master server interface
//-----------------------------------------------------------------------------
class CMaster : public IMaster, public IServersInfo
{
public:
	CMaster( void );
	virtual ~CMaster( void );

	// Heartbeat functions.
	void Init( void );
	void Shutdown( void );
	// Sets up master address
	void ShutdownConnection(void);
	void SendHeartbeat( struct adrlist_s *p );
	void AddServer( struct netadr_s *adr );
	void UseDefault ( void );
	void CheckHeartbeat (void);
	void RespondToHeartbeatChallenge( netadr_t &from, bf_read &msg );
	void PingServer( netadr_t &svadr );

	void ProcessConnectionlessPacket( netpacket_t *packet );

	void AddMaster_f( const CCommand &args );
	void Heartbeat_f( void );

	void RunFrame();

	void ReplyInfo( const netadr_t &adr, uint sequence );
	newgameserver_t &ProcessInfo( bf_read &buf );

	// SeversInfo
	void RequestInternetServerList( const char *gamedir, IServerListResponse *response );
	void RequestLANServerList( const char *gamedir, IServerListResponse *response );
	void RequestServersInfo( const netadr_t *adr, int count, IServerListResponse *response );
	void CancelRequest( IServerListResponse *response );
	void StopRefresh();

private:
	ServerListRequest_t *StartRequest( EServerListRequestType type, const char *gamedir, IServerListResponse *response );
	void FinishRequest( int iRequest, NServerResponse response );
	bool CheckRequestDone( ServerListRequest_t *req, NServerResponse *pResponse );
	void SendMasterQuery( ServerListRequest_t *req );
	void SendLanScan( ServerListRequest_t *req );
	void RequestServerInfo( ServerListRequest_t *req, const netadr_t &adr );
	void RetryServersInfoRequest( ServerListRequest_t *req );

	// List of known master servers
	adrlist_t *m_pMasterAddresses;

	bool m_bInitialized;

	uint m_iInfoSequence;

	// If nomaster is true, the server will not send heartbeats to the master server
	bool	m_bNoMasters;

	CUtlVector<ServerListRequest_t *> m_Requests;
};

static CMaster s_MasterServer;
IMaster *master = (IMaster *)&s_MasterServer;

IServersInfo *g_pServersInfo = (IServersInfo*)&s_MasterServer;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CMaster, IServersInfo, SERVERLIST_INTERFACE_VERSION, s_MasterServer );

#define	HEARTBEAT_SECONDS	140.0

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CMaster::CMaster( void )
{
	m_pMasterAddresses	= NULL;
	m_bNoMasters		= false;
	m_bInitialized = false;
	m_iInfoSequence = 1;

	Init();
}

CMaster::~CMaster( void )
{
	m_Requests.PurgeAndDeleteElements();
}

//-----------------------------------------------------------------------------
// Purpose: decides whether a request is finished and with which result
//-----------------------------------------------------------------------------
bool CMaster::CheckRequestDone( ServerListRequest_t *req, NServerResponse *pResponse )
{
	double now = Plat_FloatTime();
	bool bAllResponded = req->m_iServersResponded >= req->m_Addresses.Count();

	switch( req->m_eType )
	{
	case eRequestLAN:
		if( now - req->m_flStartTime < LAN_SCAN_TIME )
			return false;
		*pResponse = req->m_iServersResponded ? nServerResponded : nNoServersListedOnMasterServer;
		return true;

	case eRequestAddresses:
		if( !bAllResponded && now - req->m_flStartTime < ADDRESS_QUERY_TIMEOUT )
			return false;
		*pResponse = req->m_iServersResponded ? nServerResponded : nServerFailedToRespond;
		return true;

	case eRequestInternet:
	default:
		// the internet list also scans the LAN, give that time to answer
		if( now - req->m_flStartTime < LAN_SCAN_TIME )
			return false;

		if( !req->m_bMasterResponded )
		{
			if( now - req->m_flStartTime < MASTER_QUERY_TIMEOUT )
				return false;

			// no master answered at all: LAN servers may still have been found
			*pResponse = nServerFailedToRespond;
			return true;
		}

		if( ( !bAllResponded || now - req->m_flLastMasterReply < MASTER_RESPONSE_TIMEOUT ) &&
			now - req->m_flStartTime < INFO_REQUEST_TIMEOUT )
			return false;

		if( req->m_iServersListed == 0 && req->m_iServersResponded == 0 )
			*pResponse = nNoServersListedOnMasterServer;
		else
			*pResponse = nServerResponded;
		return true;
	}
}

void CMaster::RunFrame()
{
	CheckHeartbeat();

	for( int i = m_Requests.Count() - 1; i >= 0; i-- )
	{
		// a callback may have cancelled or started requests
		if( i >= m_Requests.Count() )
			continue;

		ServerListRequest_t *req = m_Requests[i];

		NServerResponse response;
		if( CheckRequestDone( req, &response ) )
		{
			FinishRequest( i, response );
			continue;
		}

		if( req->m_flRetryTime < Plat_FloatTime() - RETRY_INFO_REQUEST_TIME )
		{
			req->m_flRetryTime = Plat_FloatTime();

			if( req->m_eType == eRequestInternet && !req->m_bMasterResponded )
				SendMasterQuery( req );

			if( req->m_eType != eRequestAddresses && req->m_iLanScans < LAN_SCAN_RETRIES )
				SendLanScan( req );

			RetryServersInfoRequest( req );
		}
	}
}

void CMaster::FinishRequest( int iRequest, NServerResponse response )
{
	ServerListRequest_t *req = m_Requests[iRequest];
	m_Requests.Remove( iRequest );

	DevMsg( 2, "Server list request (type %d) finished: result %d, %d of %d servers answered\n",
		req->m_eType, response, req->m_iServersResponded, req->m_Addresses.Count() );

	IServerListResponse *pResponse = req->m_pResponse;
	delete req;

	if( pResponse )
		pResponse->RefreshComplete( response );
}

ServerListRequest_t *CMaster::StartRequest( EServerListRequestType type, const char *gamedir, IServerListResponse *response )
{
	CancelRequest( response );

	// GammaSource: at the main menu the engine is in single player mode and has no UDP
	// sockets, so every query went into the loopback buffer. Open them, unless a local
	// game is running (it would lose its loopback connection).
	if( !NET_IsMultiplayer() && !sv.IsActive() )
		NET_SetMutiplayer( true );

	ServerListRequest_t *req = new ServerListRequest_t;
	req->m_pResponse = response;
	req->m_eType = type;
	Q_strncpy( req->m_szGameDir, gamedir ? gamedir : "", sizeof( req->m_szGameDir ) );
	req->m_bMasterResponded = false;
	req->m_iServersResponded = 0;
	req->m_iServersListed = 0;
	req->m_iLanScans = 0;
	req->m_flStartTime = req->m_flRetryTime = req->m_flLastMasterReply = Plat_FloatTime();

	m_Requests.AddToTail( req );
	return req;
}

void CMaster::CancelRequest( IServerListResponse *response )
{
	for( int i = m_Requests.Count() - 1; i >= 0; i-- )
	{
		if( m_Requests[i]->m_pResponse == response )
		{
			DevMsg( 2, "Server list request (type %d) cancelled\n", m_Requests[i]->m_eType );
			delete m_Requests[i];
			m_Requests.Remove( i );
		}
	}
}

void CMaster::StopRefresh()
{
	m_Requests.PurgeAndDeleteElements();
}

void CMaster::ReplyInfo( const netadr_t &adr, uint sequence )
{
	static char gamedir[MAX_OSPATH];
	Q_FileBase( com_gamedir, gamedir, sizeof( gamedir ) );

	CUtlBuffer buf;
	buf.EnsureCapacity( 2048 );

	buf.PutUnsignedInt( LittleDWord( CONNECTIONLESS_HEADER ) );
	buf.PutUnsignedChar( S2C_INFOREPLY );

	buf.PutUnsignedInt(sequence);
	buf.PutUnsignedChar( PROTOCOL_VERSION ); // Hardcoded protocol version number
	buf.PutString( sv.GetName() );
	buf.PutString( sv.GetMapName() );
	buf.PutString( gamedir );
	buf.PutString( serverGameDLL->GetGameDescription() );

	// player info
	buf.PutUnsignedChar( sv.GetNumClients() );
	buf.PutUnsignedChar( sv.GetMaxClients() );
	buf.PutUnsignedChar( sv.GetNumFakeClients() );

	// Password?
	buf.PutUnsignedChar( sv.GetPassword() != NULL ? 1 : 0 );

	// Write a byte with some flags that describe what is to follow.
	const char *pchTags = sv_tags.GetString();
	int nFlags = 0;

	if ( pchTags && pchTags[0] != '\0' )
		nFlags |= S2A_EXTRA_DATA_HAS_GAMETAG_DATA;

	buf.PutUnsignedInt( nFlags );

	if ( nFlags & S2A_EXTRA_DATA_HAS_GAMETAG_DATA )
		buf.PutString( pchTags );

	NET_SendPacket( NULL, NS_SERVER, adr, (unsigned char *)buf.Base(), buf.TellPut() );
}

newgameserver_t &CMaster::ProcessInfo(bf_read &buf)
{
	static newgameserver_t s;
	memset( &s, 0, sizeof(s) );

	s.m_nProtocolVersion = buf.ReadByte();

	buf.ReadString( s.m_szServerName, sizeof(s.m_szServerName) );
	buf.ReadString( s.m_szMap, sizeof(s.m_szMap) );
	buf.ReadString( s.m_szGameDir, sizeof(s.m_szGameDir) );

	buf.ReadString( s.m_szGameDescription, sizeof(s.m_szGameDescription) );

	// player info
	s.m_nPlayers = buf.ReadByte();
	s.m_nMaxPlayers = buf.ReadByte();
	s.m_nBotPlayers = buf.ReadByte();

	// Password?
	s.m_bPassword = buf.ReadByte();
	s.m_iFlags = buf.ReadLong();

	if( s.m_iFlags & S2A_EXTRA_DATA_HAS_GAMETAG_DATA )
	{
		buf.ReadString( s.m_szGameTags, sizeof(s.m_szGameTags) );
	}

	return s;
}

void CMaster::ProcessConnectionlessPacket( netpacket_t *packet )
{
	static ALIGN4 char string[2048] ALIGN4_POST;    // Buffer for sending heartbeat

	uint ip; uint16 port;

	bf_read msg = packet->message;
	char c = msg.ReadChar();

	if ( c == 0  )
		return;

	switch( c )
	{
		case M2S_CHALLENGE:
		{
			RespondToHeartbeatChallenge( packet->from, msg );
			break;
		}
		case M2C_QUERY:
		{
			CUtlVector<netadr_t> listed;

			ip = msg.ReadLong();
			port = msg.ReadShort();

			while( ip != 0 && port != 0 && !msg.IsOverflowed() )
			{
				listed.AddToTail( netadr_t( ip, port ) );
				ip = msg.ReadLong();
				port = msg.ReadShort();
			}

			FOR_EACH_VEC( m_Requests, iReq )
			{
				ServerListRequest_t *req = m_Requests[iReq];
				if( req->m_eType != eRequestInternet )
					continue;

				req->m_bMasterResponded = true;
				req->m_flLastMasterReply = Plat_FloatTime();

				FOR_EACH_VEC( listed, i )
				{
					if( req->m_Addresses.Find( listed[i] ) != req->m_Addresses.InvalidIndex() )
						continue;

					req->m_Addresses.Insert( listed[i], false );
					req->m_iServersListed++;
					RequestServerInfo( req, listed[i] );
				}
			}
			break;
		}
		case C2S_INFOREQUEST:
		{
			// only a running multiplayer server answers, and only on its own socket
			if( packet->source != NS_SERVER || !sv.IsActive() || !sv.IsMultiplayer() )
				break;

			ReplyInfo(packet->from, msg.ReadLong());
			break;
		}
		case S2C_INFOREPLY:
		{
			uint sequence = msg.ReadLong();

			ServerListRequest_t *req = NULL;
			unsigned short rindex = 0;
			FOR_EACH_VEC( m_Requests, iReq )
			{
				rindex = m_Requests[iReq]->m_RequestTimes.Find( sequence );
				if( rindex != m_Requests[iReq]->m_RequestTimes.InvalidIndex() )
				{
					req = m_Requests[iReq];
					break;
				}
			}

			if( !req )
				break;

			newgameserver_t &s = ProcessInfo( msg );

			unsigned short index = req->m_Addresses.Find(packet->from);
			if( index == req->m_Addresses.InvalidIndex() )
			{
				// LAN broadcast replies come from addresses we did not know about
				if( req->m_eType == eRequestAddresses )
					break;

				index = req->m_Addresses.Insert( packet->from, false );
			}

			if( req->m_Addresses[index] ) // shit happens
				break;

			double requestTime = req->m_RequestTimes[rindex];

			req->m_Addresses[index] = true;
			req->m_iServersResponded++;

			s.m_nPing = (Plat_FloatTime()-requestTime)*1000.0;
			s.m_NetAdr = packet->from;
			s.m_bHadSuccessfulResponse = true;

			DevMsg( 2, "Server list: %s answered (%s, %s)\n", packet->from.ToString(), s.m_szServerName, s.m_szMap );

			if( req->m_pResponse )
				req->m_pResponse->ServerResponded( s );
			break;
		}
	}
}

void CMaster::RequestServerInfo( ServerListRequest_t *req, const netadr_t &adr )
{
	static ALIGN4 char string[256] ALIGN4_POST;    // Buffer for sending heartbeat
	bf_write msg( string, sizeof(string) );

	msg.WriteLong( CONNECTIONLESS_HEADER );
	msg.WriteByte( C2S_INFOREQUEST );
	msg.WriteLong( m_iInfoSequence );
	req->m_RequestTimes.Insert(m_iInfoSequence, Plat_FloatTime());

	m_iInfoSequence++;
	NET_SendPacket( NULL, NS_CLIENT, adr, msg.GetData(), msg.GetNumBytesWritten() );
}

void CMaster::RetryServersInfoRequest( ServerListRequest_t *req )
{
	FOR_EACH_MAP_FAST( req->m_Addresses, i )
	{
		bool bResponded = req->m_Addresses.Element(i);
		if( bResponded )
			continue;

		const netadr_t adr = req->m_Addresses.Key(i);
		RequestServerInfo( req, adr );
	}
}

//-----------------------------------------------------------------------------
// Purpose: broadcasts an info request to the usual server ports on the LAN
//-----------------------------------------------------------------------------
void CMaster::SendLanScan( ServerListRequest_t *req )
{
	req->m_iLanScans++;

	for( int r = 0; r < ARRAYSIZE( g_LanScanPorts ); r++ )
	{
		for( int port = g_LanScanPorts[r][0]; port <= g_LanScanPorts[r][1]; port++ )
		{
			netadr_t adr;
			adr.SetType( NA_BROADCAST );
			adr.SetPort( port );
			RequestServerInfo( req, adr );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sends a heartbeat to the master server
// Input  : *p - x00\x00\x00\x00\x00\x00
//-----------------------------------------------------------------------------
void CMaster::SendHeartbeat ( adrlist_t *p )
{
	static ALIGN4 char string[256] ALIGN4_POST;    // Buffer for sending heartbeat
	char        szGD[ MAX_OSPATH ];

	if ( !p )
		return;

	// Still waiting on challenge response?
	if ( p->heartbeatwaiting )
		return;

	// Waited too long
	if ( (realtime - p->heartbeatwaitingtime ) >= HB_TIMEOUT )
		return;

	// Send to master
		Q_FileBase( com_gamedir, szGD, sizeof( szGD ) );

	bf_write buf( string, sizeof(string) );
	buf.WriteByte( S2M_HEARTBEAT );
	buf.WriteLong( p->heartbeatchallenge );
	buf.WriteShort( PROTOCOL_VERSION );
	buf.WriteString( szGD );

	NET_SendPacket( NULL, NS_SERVER, p->adr, buf.GetData(), buf.GetNumBytesWritten() );
}

//-----------------------------------------------------------------------------
// Purpose: Requests a challenge so we can then send a heartbeat
//-----------------------------------------------------------------------------
void CMaster::CheckHeartbeat (void)
{
	adrlist_t *p;
	ALIGN4 char buf[256] ALIGN4_POST;

	if ( m_bNoMasters ||      // We are ignoring heartbeats
		IsLan() ||           // Lan servers don't heartbeat
		(sv.GetMaxClients() <= 1) ||  // not a multiplayer server.
		!sv.IsActive() )			  // only heartbeat if a server is running.
		return;

	p = m_pMasterAddresses;
	while ( p )
	{
		// Time for another try?
		if ( ( realtime - p->last_heartbeat) < HEARTBEAT_SECONDS)  // not time to send yet
		{
			p = p->next;
			continue;
		}

		// Should we resend challenge request?
		if ( p->heartbeatwaiting &&
			( ( realtime - p->heartbeatwaitingtime ) < HB_TIMEOUT ) )
		{
			p = p->next;
			continue;
		}

		int32 challenge = RandomInt( 0, INT_MAX );

		p->heartbeatwaiting     = true;
		p->heartbeatwaitingtime = realtime;

		p->last_heartbeat       = realtime;  // Flag at start so we don't just keep trying for hb's when
		p->heartbeatchallenge = challenge;

		bf_write msg("Master Join", buf, sizeof(buf));

		msg.WriteByte( S2M_GETCHALLENGE );
		msg.WriteLong( challenge );

		// Send to master asking for a challenge #
		NET_SendPacket( NULL, NS_SERVER, p->adr, msg.GetData(), msg.GetNumBytesWritten() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Server is shutting down, unload master servers list, tell masters that we are closing the server
//-----------------------------------------------------------------------------
void CMaster::ShutdownConnection( void )
{
	adrlist_t *p;

	if ( !host_initialized )
		return;

	if ( m_bNoMasters ||      // We are ignoring heartbeats
		IsLan() ||           // Lan servers don't heartbeat
		(sv.GetMaxClients() <= 1) )   // not a multiplayer server.
		return;

	const char packet = S2M_SHUTDOWN;

	p = m_pMasterAddresses;
	while ( p )
	{
		NET_SendPacket( NULL, NS_SERVER, p->adr, (unsigned char*)&packet, 1);
		p->last_heartbeat = -99999.0;
		p = p->next;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Add server to the master list
// Input  : *adr - 
//-----------------------------------------------------------------------------
void CMaster::AddServer( netadr_t *adr )
{
	adrlist_t *n;

	// See if it's there
	n = m_pMasterAddresses;
	while ( n )
	{
		if ( n->adr.CompareAdr( *adr ) )
			break;
		n = n->next;
	}

	// Found it in the list.
	if ( n )
		return;

	n = ( adrlist_t * ) malloc ( sizeof( adrlist_t ) );
	if ( !n )
		Sys_Error( "Error allocating %zd bytes for master address.", sizeof( adrlist_t ) );

	memset( n, 0, sizeof( adrlist_t ) );

	n->adr = *adr;

	// Queue up a full heartbeat to all master servers.
	n->last_heartbeat = -99999.0;

	// Link it in.
	n->next = m_pMasterAddresses;
	m_pMasterAddresses = n;
}

//-----------------------------------------------------------------------------
// Purpose: Add built-in default master if woncomm.lst doesn't parse
//-----------------------------------------------------------------------------
void CMaster::UseDefault ( void )
{
	netadr_t adr;

	for( int i = 0; i < ARRAYSIZE(g_MasterServers);i++ )
	{
		// Convert to netadr_t
		if ( NET_StringToAdr ( g_MasterServers[i], &adr ) )
		{
			// Add to master list
			AddServer( &adr );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMaster::RespondToHeartbeatChallenge( netadr_t &from, bf_read &msg )
{
	adrlist_t *p;
	uint challenge, challenge2;

	// No masters, just ignore.
	if ( !m_pMasterAddresses )
		return;

	p = m_pMasterAddresses;
	while ( p )
	{
		if ( from.CompareAdr( p->adr ) )
			break;

		p = p->next;
	}

	// Not a known master server.
	if ( !p )
		return;

	challenge = msg.ReadLong();
	challenge2 = msg.ReadLong();

	if( p->heartbeatchallenge != challenge2 )
	{
		Warning("unexpected master server info query packet (wrong challenge!)\n");
		return;
	}

	// Kill timer
	p->heartbeatwaiting   = false;
	p->heartbeatchallenge = challenge;

	// Send the actual heartbeat request to this master server.
	SendHeartbeat( p );
}

//-----------------------------------------------------------------------------
// Purpose: Add/remove master servers
//-----------------------------------------------------------------------------
void CMaster::AddMaster_f ( const CCommand &args )
{
	CUtlString cmd( ( args.ArgC() > 1 ) ? args[ 1 ] : "" );

	netadr_t adr;

	if( !NET_StringToAdr(cmd.String(), &adr) )
	{
		Warning("Invalid address\n");
		return;
	}

	this->AddServer(&adr);
}


//-----------------------------------------------------------------------------
// Purpose: Send a new heartbeat to the master
//-----------------------------------------------------------------------------
void CMaster::Heartbeat_f (void)
{
	adrlist_t *p;

	p = m_pMasterAddresses;
	while ( p )
	{
		// Queue up a full hearbeat
		p->last_heartbeat = -9999.0;
		p->heartbeatwaitingtime = -9999.0;
		p = p->next;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void AddMaster_f( const CCommand &args )
{
	master->AddMaster_f( args );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Heartbeat1_f( void )
{
	master->Heartbeat_f();
}

static ConCommand setmaster("addmaster", AddMaster_f );
static ConCommand heartbeat("heartbeat", Heartbeat1_f, "Force heartbeat of master servers" ); 

//-----------------------------------------------------------------------------
// Purpose: Adds master server console commands
//-----------------------------------------------------------------------------
void CMaster::Init( void )
{
	// Already able to initialize at least once?
	if ( m_bInitialized )
		return;

	// So we don't do this a send time.sv_mas
	m_bInitialized = true;

	UseDefault();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMaster::Shutdown(void)
{
	adrlist_t *p, *n;

	// Free the master list now.
	p = m_pMasterAddresses;
	while ( p )
	{
		n = p->next;
		free( p );
		p = n;
	}

	m_pMasterAddresses = NULL;
}

// ServersInfo
void CMaster::SendMasterQuery( ServerListRequest_t *req )
{
	ALIGN4 char buf[256] ALIGN4_POST;
	bf_write msg(buf, sizeof(buf));

	msg.WriteByte( C2M_CLIENTQUERY );
	msg.WriteString( req->m_szGameDir );

	adrlist_t *p;

	p = m_pMasterAddresses;
	while ( p )
	{
		NET_SendPacket(NULL, NS_CLIENT, p->adr, msg.GetData(), msg.GetNumBytesWritten() );
		p = p->next;
	}
}

void CMaster::RequestInternetServerList(const char *gamedir, IServerListResponse *response)
{
	ServerListRequest_t *req = StartRequest( eRequestInternet, gamedir, response );

	// GammaSource: Steam's masters are gone and the community ones may be down too,
	// so the internet list also picks up servers on the local network.
	if( !m_bNoMasters )
		SendMasterQuery( req );

	SendLanScan( req );
}

void CMaster::RequestLANServerList(const char *gamedir, IServerListResponse *response)
{
	ServerListRequest_t *req = StartRequest( eRequestLAN, gamedir, response );
	SendLanScan( req );
}

void CMaster::RequestServersInfo( const netadr_t *adr, int count, IServerListResponse *response )
{
	ServerListRequest_t *req = StartRequest( eRequestAddresses, "", response );

	for( int i = 0; i < count; i++ )
	{
		if( req->m_Addresses.Find( adr[i] ) != req->m_Addresses.InvalidIndex() )
			continue;

		req->m_Addresses.Insert( adr[i], false );
		RequestServerInfo( req, adr[i] );
	}
}
