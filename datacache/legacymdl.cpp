//========= GammaSource ============//
//
// Load-time conversion of 2001-2003 studio models (MDL v36, and v37 via an in-memory
// v37 -> v36 step) to MDL v48 + .vvd + VTX7, so this engine reads every model
// generation the 2003 content uses. This is a C++ port of
// modernize/model_conversion/mdl36_to_48.py and model37.py in the GammaSource tree.
//
// Procedural helper bones, IK rules/locks, autolayers and flex mappings are
// translated below; the old and modern payloads must retain their own offsets.
//
//=================================//

#include <math.h>
#include <string.h>
#include "tier0/dbg.h"
#include "tier1/utlvector.h"
#include "tier1/utlbuffer.h"
#include "tier1/utldict.h"
#include "tier1/strtools.h"
#include "mathlib/compressed_vector.h"
#include "legacymdl.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace
{

typedef unsigned char byte_t;

//-----------------------------------------------------------------------------
// Bounds-checked little-endian reads over the source blob
//-----------------------------------------------------------------------------
struct Src
{
	const byte_t *p;
	int n;
	bool bad;

	int I( int o ) { if ( o < 0 || o + 4 > n ) { bad = true; return 0; } int v; memcpy( &v, p + o, 4 ); return v; }
	short S( int o ) { if ( o < 0 || o + 2 > n ) { bad = true; return 0; } short v; memcpy( &v, p + o, 2 ); return v; }
	unsigned short U16( int o ) { return (unsigned short)S( o ); }
	byte_t B( int o ) { if ( o < 0 || o >= n ) { bad = true; return 0; } return p[o]; }
	float F( int o ) { if ( o < 0 || o + 4 > n ) { bad = true; return 0; } float v; memcpy( &v, p + o, 4 ); return v; }
	const byte_t *At( int o, int len ) { if ( o < 0 || len < 0 || o + len > n ) { bad = true; return NULL; } return p + o; }
	const char *Str( int o )
	{
		if ( o <= 0 || o >= n ) return "";
		const void *e = memchr( p + o, 0, n - o );
		if ( !e ) { bad = true; return ""; }
		return (const char *)( p + o );
	}
	const char *RStr( int base, int rel ) { return rel ? Str( base + rel ) : ""; }
};

//-----------------------------------------------------------------------------
// Growable output blob with a deferred string pool
//-----------------------------------------------------------------------------
struct Out
{
	CUtlVector<byte_t> b;
	struct PendingString_t { int owner; int field; CUtlString text; };
	CUtlVector<PendingString_t> strings;

	int Size() const { return b.Count(); }
	void Align( int n ) { while ( b.Count() % n ) b.AddToTail( 0 ); }
	int Alloc( int size, int align = 4 )
	{
		Align( align );
		int o = b.Count();
		if ( size > 0 ) { b.AddMultipleToTail( size ); memset( b.Base() + o, 0, size ); }
		return o;
	}
	void Append( const void *pData, int len )
	{
		if ( len <= 0 ) return;
		int o = b.AddMultipleToTail( len );
		if ( pData ) memcpy( b.Base() + o, pData, len ); else memset( b.Base() + o, 0, len );
	}
	void Copy( int o, const void *pData, int len ) { if ( pData && len > 0 ) memcpy( b.Base() + o, pData, len ); }
	void PutI( int o, int v ) { memcpy( b.Base() + o, &v, 4 ); }
	void PutF( int o, float v ) { memcpy( b.Base() + o, &v, 4 ); }
	void PutS( int o, short v ) { memcpy( b.Base() + o, &v, 2 ); }
	void PutFv( int o, const float *v, int count ) { memcpy( b.Base() + o, v, 4 * count ); }
	void String( int owner, int field, const char *text )
	{
		int i = strings.AddToTail();
		strings[i].owner = owner; strings[i].field = field; strings[i].text = text;
	}
	void FlushStrings()
	{
		CUtlDict<int, int> pool;
		for ( int i = 0; i < strings.Count(); i++ )
		{
			const char *t = strings[i].text.Get();
			int idx = pool.Find( t );
			int at;
			if ( idx == pool.InvalidIndex() )
			{
				at = b.Count();
				Append( t, V_strlen( t ) + 1 );
				pool.Insert( t, at );
			}
			else
			{
				at = pool[idx];
			}
			PutI( strings[i].field, at - strings[i].owner );
		}
		strings.Purge();
	}
};

//-----------------------------------------------------------------------------
// Math (same rules as mathlib)
//-----------------------------------------------------------------------------
struct Q4 { double x, y, z, w; };

Q4 AngleQuaternion( const double a[3] )
{
	double sr = sin( a[0] * 0.5 ), cr = cos( a[0] * 0.5 );
	double sp = sin( a[1] * 0.5 ), cp = cos( a[1] * 0.5 );
	double sy = sin( a[2] * 0.5 ), cy = cos( a[2] * 0.5 );
	double srXcp = sr * cp, crXsp = cr * sp, crXcp = cr * cp, srXsp = sr * sp;
	Q4 q = { srXcp * cy - crXsp * sy, crXsp * cy + srXcp * sy, crXcp * sy - srXsp * cy, crXcp * cy + srXsp * sy };
	return q;
}

void QuaternionAngles( const Q4 &q, double out[3] )
{
	double x = q.x, y = q.y, z = q.z, w = q.w;
	double m00 = 1.0 - 2.0 * y * y - 2.0 * z * z;
	double m10 = 2.0 * x * y + 2.0 * w * z;
	double m20 = 2.0 * x * z - 2.0 * w * y;
	double m21 = 2.0 * y * z + 2.0 * w * x;
	double m22 = 1.0 - 2.0 * x * x - 2.0 * y * y;
	double m01 = 2.0 * x * y - 2.0 * w * z;
	double m11 = 1.0 - 2.0 * x * x - 2.0 * z * z;
	double xy = sqrt( m00 * m00 + m10 * m10 );
	if ( xy > 0.001 )
	{
		out[2] = atan2( m10, m00 ); out[1] = atan2( -m20, xy ); out[0] = atan2( m21, m22 );
	}
	else
	{
		out[2] = atan2( -m01, m11 ); out[1] = atan2( -m20, xy ); out[0] = 0.0;
	}
}

Q4 QNorm( Q4 q )
{
	double l = sqrt( q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w );
	if ( l <= 0.0 ) l = 1.0;
	q.x /= l; q.y /= l; q.z /= l; q.w /= l;
	return q;
}

//-----------------------------------------------------------------------------
// The equivalent sequence/animation flag is declared in public/studio.h. Keep
// this converter independent of the live studio structs it is constructing.
const int LEGACY2003_ANIMATION_MARKER = 0x40000000;

// Both 2003 procedural types have the same payload layout in v48. Bone records
// moved and grew, so their procedure pointer and quaternion trigger pointer
// still need rebasing. Preserve these weighted shoulder/wrist/elbow helpers
// rather than replacing their authored corrective transforms with raw poses.
bool WriteLegacyProcedure( Src &s, Out &w, int sourceBone, int destBone, int boneCount, int &control )
{
	control = -1;
	int type = s.I( sourceBone + 148 );
	if ( type == 0 ) return true;
	int rel = s.I( sourceBone + 152 );
	if ( rel == 0 || rel < -sourceBone || rel > s.n - sourceBone ) return false;
	int source = sourceBone + rel;
	if ( !s.At( source, type == 1 ? 176 : 12 ) ) return false;
	control = s.I( source );
	if ( control < 0 || control >= boneCount || s.I( sourceBone + 4 ) < 0 ) return false;
	int dest;
	if ( type == 1 ) // mstudioaxisinterpbone_t: control/axis, six positions, six quaternions
	{
		int axis = s.I( source + 4 );
		if ( axis < 0 || axis > 2 ) return false;
		for ( int k = 0; k < 42; ++k ) if ( !IsFinite( s.F( source + 8 + 4 * k ) ) ) return false;
		dest = w.Alloc( 176 );
		w.Copy( dest, s.At( source, 176 ), 176 );
	}
	else if ( type == 2 ) // mstudioquatinterpbone_t + mstudioquatinterpinfo_t[32 maximum]
	{
		int count = s.I( source + 4 ), triggerRel = s.I( source + 8 );
		// Both runtime solvers have a fixed weight[32] stack array.
		if ( count < 1 || count > 32 || triggerRel < -source || triggerRel > s.n - source ) return false;
		int triggers = source + triggerRel;
		if ( !s.At( triggers, 48 * count ) ) return false;
		for ( int k = 0; k < 12 * count; ++k ) if ( !IsFinite( s.F( triggers + 4 * k ) ) ) return false;
		for ( int k = 0; k < count; ++k ) if ( s.F( triggers + 48 * k ) < 0 ) return false;
		dest = w.Alloc( 12 );
		w.PutI( dest, control ); w.PutI( dest + 4, count );
		int triggerOut = w.Alloc( 48 * count );
		w.Copy( triggerOut, s.At( triggers, 48 * count ), 48 * count );
		w.PutI( dest + 8, triggerOut - dest );
	}
	else return false; // the original format defines only axis and quaternion interpolation
	w.PutI( destBone + 164, type );
	w.PutI( destBone + 168, dest - destBone );
	return !s.bad;
}

bool LegacyIKChainValid( Src &s, int chainTable, int chainCount, int chain, int boneCount )
{
	if ( chain < 0 || chain >= chainCount ) return false;
	int c = chainTable + 16 * chain;
	if ( !s.At( c, 16 ) || s.I( c + 8 ) != 3 ) return false;
	int rel = s.I( c + 12 );
	if ( rel <= 0 || rel > s.n - c || !s.At( c + rel, 3 * 28 ) ) return false;
	for ( int k = 0; k < 3; ++k )
	{
		int bone = s.I( c + rel + 28 * k );
		if ( bone < 0 || bone >= boneCount ) return false;
	}
	return true;
}

// A 2003 lock has just these three fields (12 bytes). Modern flags and reserved
// words expand it to 32 bytes and must be initialized rather than byte-copied.
bool WriteLegacyIKLocks( Src &s, Out &w, int source, int count, int owner, int countField, int indexField,
	int chainTable, int chainCount, int boneCount )
{
	if ( count == 0 ) return true;
	if ( count < 0 || count > 256 || !s.At( source, count * 12 ) ) return false;
	int dest = w.Alloc( count * 32 );
	for ( int k = 0; k < count; ++k )
	{
		int from = source + 12 * k;
		float p = s.F( from + 4 ), q = s.F( from + 8 );
		if ( !LegacyIKChainValid( s, chainTable, chainCount, s.I( from ), boneCount ) ||
			!IsFinite( p ) || !IsFinite( q ) || p < 0 || p > 1 || q < 0 || q > 1 ) return false;
		w.Copy( dest + 32 * k, s.At( from, 12 ), 12 );
	}
	w.PutI( owner + countField, count );
	w.PutI( owner + indexField, dest - owner );
	return !s.bad;
}

// 2003 rules are 108 bytes; modern rules are 152. Only corresponding fields are
// copied. The original uncompressed position/quaternion errors remain 28 bytes.
bool WriteLegacyIKRules( Src &s, Out &w, int sourceAnim, int destAnim, int frames,
	int chainTable, int chainCount, int boneCount )
{
	int count = s.I( sourceAnim + 52 );
	if ( count == 0 ) return true;
	int rel = s.I( sourceAnim + 56 );
	if ( count < 0 || count > 64 || frames < 1 || rel <= 0 || rel > s.n - sourceAnim ) return false;
	int rules = sourceAnim + rel;
	if ( !s.At( rules, 108 * count ) ) return false;
	int dest = w.Alloc( 152 * count );
	for ( int k = 0; k < count; ++k )
	{
		int from = rules + 108 * k, to = dest + 152 * k;
		int type = s.I( from + 4 ), bone = s.I( from + 12 ), slot = s.I( from + 16 );
		if ( type < 0 || type > 4 || bone < -1 || bone >= boneCount || slot < 0 || slot >= 12 ||
			!LegacyIKChainValid( s, chainTable, chainCount, s.I( from + 8 ), boneCount ) ) return false;
		for ( int field = 20; field < 60; field += 4 ) if ( !IsFinite( s.F( from + field ) ) ) return false;
		const bool staticEnvelopeNotFinite = frames == 1 &&
			( !IsFinite( s.F( from + 76 ) ) || !IsFinite( s.F( from + 80 ) ) ||
			  !IsFinite( s.F( from + 84 ) ) || !IsFinite( s.F( from + 88 ) ) || !IsFinite( s.F( from + 96 ) ) );
		if ( staticEnvelopeNotFinite )
		{
			// The old compiler divided the envelope by numframes-1 even for
			// static aim/couch poses, writing NaN/Infinity (ElitePolice, monk).
			// Those comparisons produced zero influence in the 2003 player.
			// Keep rule positions and chain references, but encode that inactive
			// envelope explicitly so one bad normalization cannot reject a model.
			int errorRel = s.I( from + 72 );
			if ( errorRel <= 0 || errorRel > s.n - from || !s.At( from + errorRel, 28 ) ) return false;
			for ( int e = 0; e < 7; ++e ) if ( !IsFinite( s.F( from + errorRel + 4 * e ) ) ) return false;
			w.Copy( to, s.At( from, 60 ), 60 );
			int errorOut = w.Alloc( 28 );
			w.Copy( errorOut, s.At( from + errorRel, 28 ), 28 );
			w.PutI( to + 72, errorOut - to );
			continue;
		}
		float start = s.F( from + 76 ), peak = s.F( from + 80 ), tail = s.F( from + 84 ), end = s.F( from + 88 );
		if ( !IsFinite( start ) || !IsFinite( peak ) || !IsFinite( tail ) || !IsFinite( end ) ||
			start > peak || peak > tail || tail > end || start < -1.0f || end > 3.0f ) return false;
		int first = s.I( from + 68 );
		double endFrame = (double)end * ( frames - 1 );
		int last = (int)floor( endFrame + 0.5 );
		// This is the original studiomdl allocation formula, including the two
		// guard samples for a rule which wraps beyond the final frame.
		int errors = last - first + 1 + ( last >= frames ? 2 : 0 );
		int errorRel = s.I( from + 72 );
		if ( fabs( endFrame - last ) > 0.01 || errors < 1 || errors > 3 * frames + 4 ||
			errorRel <= 0 || errorRel > s.n - from || !s.At( from + errorRel, errors * 28 ) ) return false;
		for ( int e = 0; e < errors * 7; ++e ) if ( !IsFinite( s.F( from + errorRel + 4 * e ) ) ) return false;
		w.Copy( to, s.At( from, 60 ), 60 ); // index/type/chain/bone/slot, bounds, position and quaternion
		w.PutI( to + 68, first );
		w.Copy( to + 76, s.At( from + 76, 16 ), 16 ); // influence envelope
		if ( !IsFinite( s.F( from + 96 ) ) ) return false;
		w.PutF( to + 96, s.F( from + 96 ) ); // contact; old commit/pivot/release were unused
		int errorOut = w.Alloc( errors * 28 );
		w.Copy( errorOut, s.At( from + errorRel, errors * 28 ), errors * 28 );
		w.PutI( to + 72, errorOut - to );
		// compressed-error pointer, obsolete group and modern-only drop/top,
		// attachment and reserved fields stay zero from Alloc().
	}
	w.PutI( destAnim + 60, count );
	w.PutI( destAnim + 64, dest - destAnim );
	return !s.bad;
}

// v37 -> v36 (in memory). Payload keeps its byte positions; a v36 header and an
// expanded v36 sequence table are written in front of / after it.
//-----------------------------------------------------------------------------
bool ConvertV37ToV36( const byte_t *pData, int nSize, CUtlVector<byte_t> &out )
{
	Src s = { pData, nSize, false };
	if ( nSize < 388 || s.I( 4 ) != 37 )
		return false;
	int nb = s.I( 156 ), bo = s.I( 160 ), na = s.I( 180 ), ng = s.I( 188 ), go = s.I( 192 );
	int ns = s.I( 204 ), so = s.I( 208 );
	if ( nb < 0 || nb > 128 || na < 0 || na > 10000 || ns < 0 || ns > 10000 || na != ng || s.I( 216 ) != 1 )
		return false;
	for ( int k = 0; k < ng; k++ )
	{
		if ( s.I( go + 8 * k ) != 0 || s.I( go + 8 * k + 4 ) != k )
			return false;	// external animation groups are not supported
	}
	(void)bo;

	out.SetCount( nSize );
	memcpy( out.Base(), pData, nSize );
	memcpy( out.Base(), pData, 188 );
	memcpy( out.Base() + 188, pData + 204, 184 );
	while ( out.Count() % 4 ) out.AddToTail( 0 );
	int newSeq = out.Count();
	out.AddMultipleToTail( 4284 * ns );
	memset( out.Base() + newSeq, 0, 4284 * ns );

	for ( int k = 0; k < ns; k++ )
	{
		int old = so + 188 * k, nw = newSeq + 4284 * k;
		int gx = s.I( old + 64 ), gy = s.I( old + 68 );
		if ( gx < 1 || gx > 32 || gy < 1 || gy > 32 )
			return false;
		int blend = old + s.I( old + 56 );
		const byte_t *pOld = s.At( old, 188 );
		if ( !pOld || !s.At( blend, 2 * gx * gy ) )
			return false;
		memcpy( out.Base() + nw, pOld, 56 );
		for ( int x = 0; x < gx; x++ )
			for ( int y = 0; y < gy; y++ )
			{
				int id = s.U16( blend + 2 * ( y * gx + x ) );
				memcpy( out.Base() + nw + 56 + 4 * ( x * 32 + y ), &id, 4 );
			}
		memset( out.Base() + nw + 4152, 0, 4 );
		memcpy( out.Base() + nw + 4156, pOld + 64, 36 );
		memcpy( out.Base() + nw + 4192, pOld + 60, 4 );
		memcpy( out.Base() + nw + 4196, pOld + 100, 88 );
		static const int remap[][2] = { { 0, 0 }, { 4, 4 }, { 24, 24 }, { 148, 4244 }, { 152, 4248 }, { 156, 4252 }, { 164, 4260 }, { 168, 4264 } };
		for ( int r = 0; r < ARRAYSIZE( remap ); r++ )
		{
			int rel = s.I( old + remap[r][0] );
			if ( rel )
			{
				int v = old + rel - nw;
				memcpy( out.Base() + nw + remap[r][1], &v, 4 );
			}
		}
	}
	int v36 = 36, len = out.Count();
	memcpy( out.Base() + 4, &v36, 4 );
	memcpy( out.Base() + 76, &len, 4 );
	memcpy( out.Base() + 192, &newSeq, 4 );
	return !s.bad;
}

//-----------------------------------------------------------------------------
// v36 animation decode
//-----------------------------------------------------------------------------
struct Bone36
{
	CUtlString name;
	int parent;
	int ctrl[6];
	float value[6], scale[6], poseToBone[12], qAlign[4], quat[4];
	int flags, proctype, physicsbone, contents;
	CUtlString surfaceprop;
};

struct Frame { double p[3]; Q4 q; };

int AnimValueAt( Src &s, int ptr, int frame )
{
	int k = frame;
	int valid = s.B( ptr ), total = s.B( ptr + 1 );
	if ( total < valid ) k = 0;
	int guard = 0;
	while ( total <= k )
	{
		k -= total;
		ptr += ( valid + 1 ) * 2;
		valid = s.B( ptr ); total = s.B( ptr + 1 );
		if ( total < valid ) k = 0;
		if ( ++guard > 100000 || s.bad ) { s.bad = true; return 0; }
	}
	if ( valid > k )
		return s.S( ptr + ( k + 1 ) * 2 );
	return s.S( ptr + valid * 2 );
}

//-----------------------------------------------------------------------------
// v48 RLE channel encode
//-----------------------------------------------------------------------------
void RleChannel( const CUtlVector<short> &v, Out &w )
{
	int n = v.Count(), i = 0;
	while ( i < n )
	{
		int j = i;
		while ( j + 1 < n && v[j + 1] == v[i] && j - i < 254 ) j++;
		int run = j - i + 1;
		if ( run >= 3 )
		{
			byte_t hdr[2] = { 1, (byte_t)run };
			w.Append( hdr, 2 ); w.Append( &v[i], 2 );
			i = j + 1;
			continue;
		}
		j = i;
		CUtlVector<short> lit;
		while ( j < n && lit.Count() < 255 )
		{
			int k = j;
			while ( k + 1 < n && v[k + 1] == v[j] && k - j < 254 ) k++;
			if ( k - j + 1 >= 3 ) break;
			lit.AddToTail( v[j] ); j++;
		}
		byte_t hdr[2] = { (byte_t)lit.Count(), (byte_t)lit.Count() };
		w.Append( hdr, 2 ); w.Append( lit.Base(), 2 * lit.Count() );
		i = j;
	}
}

//-----------------------------------------------------------------------------
// Which of a vertex's four weights the VVD dropped (-1 = none), and how to find a
// VTX vertex's model-wide index: meshVertBase[modelMeshStart[model] + mesh] + origMeshVertID
//-----------------------------------------------------------------------------
struct VtxRemap_t
{
	CUtlVector<signed char> vertDrop;
	CUtlVector<int> modelMeshStart;
	CUtlVector<int> meshVertBase;
};

//-----------------------------------------------------------------------------
// VTX6 -> VTX7
//-----------------------------------------------------------------------------
bool Vtx6To7( const byte_t *pData, int nSize, int checksum, const VtxRemap_t &remap, CUtlVector<byte_t> &result )
{
	int modelIndex = 0;
	Src d = { pData, nSize, false };
	if ( nSize < 36 || d.I( 0 ) != 6 )
		return false;
	int vcache = d.I( 4 );
	unsigned short mbs = d.U16( 8 ), mbf = d.U16( 10 );
	int mbv = d.I( 12 ), nlod = d.I( 20 ), repl = d.I( 24 ), nbp = d.I( 28 ), bpo = d.I( 32 );
	if ( nlod < 0 || nlod > 8 || nbp < 0 || nbp > 256 )
		return false;

	Out o;
	o.Alloc( 36 );
	o.PutI( 0, 7 ); o.PutI( 4, vcache ); o.PutS( 8, (short)mbs ); o.PutS( 10, (short)mbf );
	o.PutI( 12, mbv < 3 ? mbv : 3 ); o.PutI( 16, checksum ); o.PutI( 20, nlod ); o.PutI( 24, 0 );
	o.PutI( 28, nbp ); o.PutI( 32, 36 );

	int bpNew = o.Size(); o.Append( NULL, 8 * nbp );
	for ( int b = 0; b < nbp && !d.bad; b++ )
	{
		int ob = bpo + 8 * b;
		int nm = d.I( ob ), mo = ob + d.I( ob + 4 );
		if ( nm < 0 || nm > 1024 ) return false;
		int mNew = o.Size(); o.Append( NULL, 8 * nm );
		o.PutI( bpNew + 8 * b, nm ); o.PutI( bpNew + 8 * b + 4, mNew - ( bpNew + 8 * b ) );
		for ( int m = 0; m < nm && !d.bad; m++, modelIndex++ )
		{
			int om = mo + 8 * m;
			int nl = d.I( om ), lo = om + d.I( om + 4 );
			int meshStart = modelIndex < remap.modelMeshStart.Count() ? remap.modelMeshStart[modelIndex] : -1;
			if ( nl < 0 || nl > 8 ) return false;
			int lNew = o.Size(); o.Append( NULL, 12 * nl );
			o.PutI( mNew + 8 * m, nl ); o.PutI( mNew + 8 * m + 4, lNew - ( mNew + 8 * m ) );
			for ( int l = 0; l < nl && !d.bad; l++ )
			{
				int ol = lo + 12 * l;
				int nme = d.I( ol ), meo = ol + d.I( ol + 4 );
				float sw = d.F( ol + 8 );
				if ( nme < 0 || nme > 8192 ) return false;
				int meNew = o.Size(); o.Append( NULL, 9 * nme );
				o.PutI( lNew + 12 * l, nme ); o.PutI( lNew + 12 * l + 4, meNew - ( lNew + 12 * l ) ); o.PutF( lNew + 12 * l + 8, sw );
				for ( int me = 0; me < nme && !d.bad; me++ )
				{
					int ome = meo + 9 * me;
					int ng = d.I( ome ), go = ome + d.I( ome + 4 );
					byte_t mflags = d.B( ome + 8 );
					if ( ng < 0 || ng > 8192 ) return false;
					int gNew = o.Size(); o.Append( NULL, 25 * ng );
					o.PutI( meNew + 9 * me, ng ); o.PutI( meNew + 9 * me + 4, gNew - ( meNew + 9 * me ) ); o.b[meNew + 9 * me + 8] = mflags;
					for ( int g = 0; g < ng && !d.bad; g++ )
					{
						int og = go + 25 * g;
						int nv = d.I( og ), vo = d.I( og + 4 ), ni = d.I( og + 8 ), io = d.I( og + 12 );
						int nst = d.I( og + 16 ), so = d.I( og + 20 );
						byte_t gflags = d.B( og + 24 );
						if ( nv < 0 || nv > 65535 || ni < 0 || nst < 0 || nst > 65535 || !d.At( og + vo, 15 * nv ) || !d.At( og + io, 2 * ni ) )
							return false;
						int gn = gNew + 25 * g;
						int vNew = o.Size();
						int meshFlat = meshStart >= 0 ? meshStart + me : -1;
						int vertBase = ( meshFlat >= 0 && meshFlat < remap.meshVertBase.Count() ) ? remap.meshVertBase[meshFlat] : -1;
						for ( int v = 0; v < nv; v++ )
						{
							int vp = og + vo + 15 * v;
							unsigned short orig = d.U16( vp + 12 );
							int nbones = d.B( vp + 14 );
							if ( nbones > 4 ) nbones = 4;
							int g = vertBase >= 0 ? vertBase + orig : -1;
							int drop = ( g >= 0 && g < remap.vertDrop.Count() ) ? remap.vertDrop[g] : -1;
							byte_t vert[9] = { 0 };
							int n = 0;
							for ( int k = 0; k < nbones && n < 3; k++ )
							{
								int bw = d.B( vp + k );
								if ( drop >= 0 && bw == drop ) continue;
								if ( drop >= 0 && bw > drop ) bw--;
								int id = d.S( vp + 4 + 2 * k );
								vert[n] = (byte_t)bw;
								vert[6 + n] = (byte_t)(signed char)( id < -128 ? -128 : ( id > 127 ? 127 : id ) );
								n++;
							}
							vert[3] = (byte_t)n;
							memcpy( vert + 4, &orig, 2 );
							o.Append( vert, 9 );
						}
						int iNew = o.Size(); o.Append( d.At( og + io, 2 * ni ), 2 * ni );
						int sNew = o.Size(); o.Append( NULL, 27 * nst );
						for ( int st = 0; st < nst; st++ )
						{
							int os = og + so + 27 * st;
							const byte_t *pStrip = d.At( os, 27 );
							if ( !pStrip ) return false;
							int nchg = d.I( os + 19 ), chgo = d.I( os + 23 );
							if ( nchg < 0 || nchg > 1024 || !d.At( os + chgo, 8 * nchg ) ) return false;
							int cNew = o.Size(); o.Append( d.At( os + chgo, 8 * nchg ), 8 * nchg );
							o.Copy( sNew + 27 * st, pStrip, 27 );
							o.PutI( sNew + 27 * st + 23, cNew - ( sNew + 27 * st ) );
						}
						o.PutI( gn, nv ); o.PutI( gn + 4, vNew - gn ); o.PutI( gn + 8, ni ); o.PutI( gn + 12, iNew - gn );
						o.PutI( gn + 16, nst ); o.PutI( gn + 20, sNew - gn ); o.b[gn + 24] = gflags;
					}
				}
			}
		}
	}
	// material replacement lists
	int mrNew = o.Size(); o.Append( NULL, 8 * nlod );
	o.PutI( 24, mrNew );
	for ( int l = 0; l < nlod && !d.bad; l++ )
	{
		int ol = repl + 8 * l;
		int n = d.I( ol ), ro = ol + d.I( ol + 4 );
		if ( n < 0 || n > 2048 ) return false;
		int rNew = o.Size(); o.Append( NULL, 6 * n );
		o.PutI( mrNew + 8 * l, n ); o.PutI( mrNew + 8 * l + 4, rNew - ( mrNew + 8 * l ) );
		for ( int r = 0; r < n; r++ )
		{
			int orr = ro + 6 * r;
			short mid = d.S( orr );
			const char *name = d.Str( orr + d.I( orr + 2 ) );
			int sNew = o.Size(); o.Append( name, V_strlen( name ) + 1 );
			o.PutS( rNew + 6 * r, mid ); o.PutI( rNew + 6 * r + 2, sNew - ( rNew + 6 * r ) );
		}
	}
	if ( d.bad )
		return false;
	result.SetCount( o.Size() );
	memcpy( result.Base(), o.b.Base(), o.Size() );
	return true;
}

} // namespace

//-----------------------------------------------------------------------------
// Public entry
//-----------------------------------------------------------------------------
bool LegacyMDL_IsLegacy( const void *pData, int nSize )
{
	if ( nSize < 8 ) return false;
	const byte_t *p = (const byte_t *)pData;
	int v; memcpy( &v, p + 4, 4 );
	return p[0] == 'I' && p[1] == 'D' && p[2] == 'S' && p[3] == 'T' && ( v == 36 || v == 37 );
}

bool LegacyMDL_Convert( const void *pMdl, int nMdlSize, const LegacyVtxInput_t *pVtx, int nVtx,
	CUtlBuffer &mdlOut, CUtlBuffer &vvdOut, CUtlVector<LegacyVtxOutput_t> &vtxOut, CUtlString &report,
	const CUtlVector<CUtlString> *pPhysicsSolidNames )
{
	CUtlVector<byte_t> v36buf;
	const byte_t *pSrc = (const byte_t *)pMdl;
	int nSrc = nMdlSize;
	{
		int ver; memcpy( &ver, pSrc + 4, 4 );
		if ( ver == 37 )
		{
			if ( !ConvertV37ToV36( pSrc, nSrc, v36buf ) )
			{
				report = "MDL v37 layout not supported (external animation groups?)";
				return false;
			}
			pSrc = v36buf.Base(); nSrc = v36buf.Count();
		}
	}
	Src s = { pSrc, nSrc, false };
	if ( s.I( 4 ) != 36 || nSrc < 336 )
	{
		report = "not an MDL v36/v37 model";
		return false;
	}
	CUtlVector<CUtlString> notes;
	const int checksum = s.I( 8 );
	const int legacyChainCount = s.I( 284 ), legacyChainTable = s.I( 288 );
	if ( legacyChainCount < 0 || legacyChainCount > 64 || !s.At( legacyChainTable, 16 * legacyChainCount ) )
	{
		report = "bad IK chain table";
		return false;
	}

	// --- bones
	int nb = s.I( 156 ), bi = s.I( 160 );
	if ( nb < 0 || nb > 128 || !s.At( bi, 196 * nb ) ) { report = "bad bone table"; return false; }
	CUtlVector<Bone36> bones; bones.SetCount( nb );
	for ( int k = 0; k < nb; k++ )
	{
		int o = bi + 196 * k;
		Bone36 &b = bones[k];
		b.name = s.RStr( o, s.I( o ) );
		b.parent = s.I( o + 4 );
		for ( int j = 0; j < 6; j++ ) b.ctrl[j] = s.I( o + 8 + 4 * j );
		for ( int j = 0; j < 6; j++ ) { b.value[j] = s.F( o + 32 + 4 * j ); b.scale[j] = s.F( o + 56 + 4 * j ); }
		for ( int j = 0; j < 12; j++ ) b.poseToBone[j] = s.F( o + 80 + 4 * j );
		for ( int j = 0; j < 4; j++ ) { b.qAlign[j] = s.F( o + 128 + 4 * j ); b.quat[j] = s.F( o + 164 + 4 * j ); }
		b.flags = s.I( o + 144 ); b.proctype = s.I( o + 148 );
		b.physicsbone = s.I( o + 156 );
		b.surfaceprop = s.RStr( o, s.I( o + 160 ) );
		b.contents = s.I( o + 180 );
	}

	// --- physics bone: the ragdoll element each bone drives. Traces copy it into
	// trace_t::physicsbone and the game indexes ragdoll lists with it, so it must be
	// a valid element. With the .phy at hand, rebuild it exactly as studiomdl does
	// (solid order, else the nearest parent's element, else the root). Otherwise
	// keep the 2003 studiomdl value when it is in range.
	CUtlVector<int> physicsBone; physicsBone.SetCount( nb );
	if ( pPhysicsSolidNames && pPhysicsSolidNames->Count() )
	{
		for ( int k = 0; k < nb; k++ ) physicsBone[k] = -1;
		for ( int i = 0; i < pPhysicsSolidNames->Count(); i++ )
			for ( int k = 0; k < nb; k++ )
				if ( !V_stricmp( bones[k].name.Get(), ( *pPhysicsSolidNames )[i].Get() ) ) { physicsBone[k] = i; break; }
		for ( int k = 0; k < nb; k++ )
		{
			if ( physicsBone[k] >= 0 ) continue;
			int bone = -1, guard = 0;
			for ( int p = bones[k].parent; p >= 0 && p < nb && guard++ < 256; p = bones[p].parent )
				if ( physicsBone[p] >= 0 ) { bone = physicsBone[p]; break; }
			physicsBone[k] = bone >= 0 ? bone : 0;
		}
	}
	else
	{
		for ( int k = 0; k < nb; k++ ) physicsBone[k] = ( bones[k].physicsbone >= 0 && bones[k].physicsbone < nb ) ? bones[k].physicsbone : 0;
	}

	// --- animations, decoded per frame
	int na = s.I( 180 ), ai = s.I( 184 );
	if ( na < 0 || na > 10000 || !s.At( ai, 92 * na ) ) { report = "bad animation table"; return false; }
	struct Anim_t { CUtlString name; float fps; int flags, numframes, numIK; CUtlVector<const byte_t *> movements; CUtlVector<Frame> frames; /* [bone * numframes + frame] */ };
	CUtlVector<Anim_t> anims; anims.SetCount( na );
	for ( int k = 0; k < na; k++ )
	{
		int o = ai + 92 * k;
		Anim_t &an = anims[k];
		int nf = s.I( o + 12 ); if ( nf < 1 ) nf = 1; if ( nf > 65535 ) { report = "bad frame count"; return false; }
		an.name = s.RStr( o, s.I( o ) ); an.fps = s.F( o + 4 ); an.flags = s.I( o + 8 ); an.numframes = nf;
		int nmv = s.I( o + 16 ), mvo = o + s.I( o + 20 );
		if ( nmv < 0 || nmv > 1024 || !s.At( mvo, 44 * nmv ) ) { report = "bad movement table"; return false; }
		for ( int m = 0; m < nmv; m++ ) an.movements.AddToTail( s.At( mvo + 44 * m, 44 ) );
		int animbase = o + s.I( o + 48 );
		an.frames.SetCount( nb * nf );
		for ( int b = 0; b < nb; b++ )
		{
			int a = animbase + 32 * b;
			int flags = s.I( a );
			const Bone36 &bone = bones[b];
			for ( int fr = 0; fr < nf; fr++ )
			{
				Frame &f = an.frames[b * nf + fr];
				if ( flags & 2 )
				{
					double ang[3];
					for ( int j = 0; j < 3; j++ )
					{
						int off = s.I( a + 4 + 4 * ( j + 3 ) );
						ang[j] = bone.value[j + 3] + ( off ? AnimValueAt( s, a + off, fr ) * (double)bone.scale[j + 3] : 0.0 );
					}
					f.q = AngleQuaternion( ang );
				}
				else
				{
					Q4 q = { s.F( a + 16 ), s.F( a + 20 ), s.F( a + 24 ), s.F( a + 28 ) };
					f.q = QNorm( q );
				}
				if ( flags & 1 )
				{
					for ( int j = 0; j < 3; j++ )
					{
						int off = s.I( a + 4 + 4 * j );
						f.p[j] = bone.value[j] + ( off ? AnimValueAt( s, a + off, fr ) * (double)bone.scale[j] : 0.0 );
					}
				}
				else
				{
					for ( int j = 0; j < 3; j++ ) f.p[j] = s.F( a + 4 + 4 * j );
				}
			}
		}
		if ( s.bad ) { report = "corrupt animation data"; return false; }
	}

	// per-bone scales for the new encoding (shared by every animation of that bone)
	CUtlVector<double> posscale, rotscale; posscale.SetCount( 3 * nb ); rotscale.SetCount( 3 * nb );
	for ( int i = 0; i < 3 * nb; i++ ) { posscale[i] = 0; rotscale[i] = 0; }
	CUtlVector< CUtlVector<double> > euler; euler.SetCount( na );	// [anim][(bone * nf + frame) * 3 + j]
	for ( int k = 0; k < na; k++ )
	{
		Anim_t &an = anims[k];
		int nf = an.numframes;
		const bool delta = ( an.flags & 0x0004 ) != 0;
		euler[k].SetCount( nb * nf * 3 );
		for ( int b = 0; b < nb; b++ )
		{
			double prev[3]; bool hasPrev = false;
			for ( int fr = 0; fr < nf; fr++ )
			{
				const Frame &f = an.frames[b * nf + fr];
				double e[3]; QuaternionAngles( f.q, e );
				if ( hasPrev )
				{
					for ( int j = 0; j < 3; j++ )
					{
						while ( e[j] - prev[j] > M_PI ) e[j] -= 2 * M_PI;
						while ( e[j] - prev[j] < -M_PI ) e[j] += 2 * M_PI;
					}
				}
				for ( int j = 0; j < 3; j++ )
				{
					prev[j] = e[j];
					euler[k][( b * nf + fr ) * 3 + j] = e[j];
					// v48 delta channels decode around zero, not the bind pose.
					// Include both kinds in the shared per-bone quantization scale.
					double dp = fabs( f.p[j] - ( delta ? 0.0 : bones[b].value[j] ) ) / 32767.0;
					if ( dp > posscale[3 * b + j] ) posscale[3 * b + j] = dp;
					double dr = fabs( e[j] - ( delta ? 0.0 : bones[b].value[3 + j] ) ) / 32767.0;
					if ( dr > rotscale[3 * b + j] ) rotscale[3 * b + j] = dr;
				}
				hasPrev = true;
			}
		}
	}

	// --- vertices (inline in v36) -> VVD
	CUtlVector<byte_t> vvdVerts, vvdTan;
	VtxRemap_t remap;
	CUtlVector<signed char> &vertDrop = remap.vertDrop;
	CUtlVector<int> &modelMeshStart = remap.modelMeshStart;
	CUtlVector<int> &meshVertBase = remap.meshVertBase;
	CUtlVector<bool> vertBoneUsed; vertBoneUsed.SetCount( nb ); for ( int i = 0; i < nb; i++ ) vertBoneUsed[i] = false;
	int nbp = s.I( 236 ), bpi = s.I( 240 );
	if ( nbp < 0 || nbp > 256 || !s.At( bpi, 16 * nbp ) ) { report = "bad bodypart table"; return false; }
	int totalVerts = 0;
	for ( int p = 0; p < nbp; p++ )
	{
		int o = bpi + 16 * p;
		int nm = s.I( o + 4 ), mi = o + s.I( o + 12 );
		if ( nm < 0 || nm > 1024 || !s.At( mi, 140 * nm ) ) { report = "bad model table"; return false; }
		for ( int m = 0; m < nm; m++ )
		{
			int mo = mi + 140 * m;
			int nv = s.I( mo + 80 );
			int vbase = mo + s.I( mo + 84 ), hasTan = s.I( mo + 88 ), tbase = mo + hasTan;
			if ( nv < 0 || nv > 65536 * 4 || !s.At( vbase, 64 * nv ) || ( hasTan && !s.At( tbase, 16 * nv ) ) ) { report = "bad vertex data"; return false; }
			for ( int v = 0; v < nv; v++ )
			{
				int vo = vbase + 64 * v;
				int cnt = s.S( vo + 24 );
				int use = cnt < 0 ? 0 : ( cnt > 4 ? 4 : cnt );
				float w[4]; int bn[4];
				for ( int j = 0; j < 4; j++ ) { w[j] = s.F( vo + 4 * j ); bn[j] = s.S( vo + 16 + 2 * j ); }
				// v48 holds three weights: drop the lightest of four, keeping the original
				// order so the VTX boneWeightIndex values still line up (see Vtx6To7)
				int order[4] = { 0, 1, 2, 3 };
				signed char drop = -1;
				if ( use == 4 )
				{
					drop = 0;
					for ( int j = 1; j < 4; j++ ) if ( w[j] < w[drop] ) drop = (signed char)j;
					for ( int j = drop; j < 3; j++ ) order[j] = j + 1;
				}
				vertDrop.AddToTail( drop );
				int keep = use < 3 ? use : 3;
				float sum = 0; for ( int j = 0; j < keep; j++ ) sum += w[order[j]];
				if ( sum == 0 ) sum = 1.0f;
				float ow[3] = { 0, 0, 0 }; signed char ob[3] = { 0, 0, 0 };
				for ( int j = 0; j < keep; j++ ) { ow[j] = w[order[j]] / sum; ob[j] = (signed char)bn[order[j]]; }
				int numbones = cnt < 1 ? 1 : ( cnt > 3 ? 3 : cnt );
				for ( int j = 0; j < numbones; j++ ) { int bb = ( j < keep ) ? bn[order[j]] : 0; if ( bb >= 0 && bb < nb ) vertBoneUsed[bb] = true; }
				int at = vvdVerts.AddMultipleToTail( 48 );
				memcpy( vvdVerts.Base() + at, ow, 12 );
				memcpy( vvdVerts.Base() + at + 12, ob, 3 );
				vvdVerts[at + 15] = (byte_t)numbones;
				memcpy( vvdVerts.Base() + at + 16, s.At( vo + 32, 32 ), 32 );
				int ta = vvdTan.AddMultipleToTail( 16 );
				if ( hasTan )
					memcpy( vvdTan.Base() + ta, s.At( tbase + 16 * v, 16 ), 16 );
				else
				{
					float t[4] = { 1, 0, 0, 1 }; memcpy( vvdTan.Base() + ta, t, 16 );
				}
			}
			modelMeshStart.AddToTail( meshVertBase.Count() );
			{
				int nme = s.I( mo + 72 ), mei = mo + s.I( mo + 76 );
				if ( nme < 0 || nme > 4096 || !s.At( mei, 68 * nme ) ) { report = "bad meshes"; return false; }
				for ( int q = 0; q < nme; q++ ) meshVertBase.AddToTail( totalVerts + s.I( mei + 68 * q + 12 ) );
			}
			totalVerts += nv;
		}
	}

	// --- bone usage flags (v48 needs them to set up bones at all)
	const int BONE_USED_BY_HITBOX = 0x100, BONE_USED_BY_ATTACHMENT = 0x200, LODMASK = 0x3fc00;
	// Fixed quaternion alignment has the same meaning in both formats. Three
	// antlion-guard bones rely on it; usage masks below still come from geometry.
	// BONE_ALWAYS_PROCEDURAL (0x4) also retains its meaning in v48; without it
	// CalcProceduralBone ignores the payload even when the pointer is preserved.
	CUtlVector<int> boneFlags; boneFlags.SetCount( nb ); for ( int i = 0; i < nb; i++ ) boneFlags[i] = bones[i].flags & 0x00100004;
	struct Up { static void Mark( CUtlVector<int> &fl, const CUtlVector<Bone36> &bn, int b, int f ) { int guard = 0; while ( b >= 0 && b < bn.Count() && guard++ < 256 ) { fl[b] |= f; b = bn[b].parent; } } };
	for ( int b = 0; b < nb; b++ ) if ( vertBoneUsed[b] ) Up::Mark( boneFlags, bones, b, LODMASK );

	int nhs = s.I( 172 ), hsi = s.I( 176 );
	if ( nhs < 0 || nhs > 256 || !s.At( hsi, 12 * nhs ) ) { report = "bad hitbox sets"; return false; }
	for ( int h = 0; h < nhs; h++ )
	{
		int o = hsi + 12 * h, nbox = s.I( o + 4 ), bxo = o + s.I( o + 8 );
		if ( nbox < 0 || nbox > 4096 || !s.At( bxo, 68 * nbox ) ) { report = "bad hitboxes"; return false; }
		for ( int x = 0; x < nbox; x++ ) Up::Mark( boneFlags, bones, s.I( bxo + 68 * x ), BONE_USED_BY_HITBOX );
	}
	int natt = s.I( 244 ), atti = s.I( 248 );
	if ( natt < 0 || natt > 4096 || !s.At( atti, 60 * natt ) ) { report = "bad attachments"; return false; }
	for ( int x = 0; x < natt; x++ ) Up::Mark( boneFlags, bones, s.I( atti + 60 * x + 8 ), BONE_USED_BY_ATTACHMENT );

	const char *surfaceprop = s.Str( s.I( 308 ) );

	// ================================================================ write v48
	Out w;
	w.Alloc( 408 );
	w.Copy( 0, "IDST", 4 ); w.PutI( 4, 48 ); w.PutI( 8, checksum );
	w.Copy( 12, s.At( 12, 64 ), 64 );
	w.Copy( 80, s.At( 80, 76 ), 76 );
	w.PutI( 152, s.I( 152 ) & 0x1ff );	// only the flag bits that mean the same in 2003 and v48

	// bones
	int bo = w.Alloc( 216 * nb ); w.PutI( 156, nb ); w.PutI( 160, bo );
	CUtlVector<int> proceduralControl; proceduralControl.SetCount( nb );
	for ( int k = 0; k < nb; k++ )
	{
		int o = bo + 216 * k;
		const Bone36 &b = bones[k];
		w.String( o, o, b.name.Get() );
		w.PutI( o + 4, b.parent );
		for ( int j = 0; j < 6; j++ ) w.PutI( o + 8 + 4 * j, b.ctrl[j] );
		w.PutFv( o + 32, b.value, 3 ); w.PutFv( o + 44, b.quat, 4 ); w.PutFv( o + 60, b.value + 3, 3 );
		for ( int j = 0; j < 3; j++ ) { w.PutF( o + 72 + 4 * j, (float)posscale[3 * k + j] ); w.PutF( o + 84 + 4 * j, (float)rotscale[3 * k + j] ); }
		w.PutFv( o + 96, b.poseToBone, 12 ); w.PutFv( o + 144, b.qAlign, 4 );
		w.PutI( o + 160, boneFlags[k] ); w.PutI( o + 172, physicsBone[k] );
		const char *sp = b.surfaceprop.Length() ? b.surfaceprop.Get() : ( *surfaceprop ? surfaceprop : "default" );
		w.String( o, o + 176, sp ); w.PutI( o + 180, b.contents );
		if ( !WriteLegacyProcedure( s, w, bi + 196 * k, o, nb, proceduralControl[k] ) )
		{
			report = "bad legacy procedural bone data";
			return false;
		}
	}
	// A helper's controlling bone can have no direct vertex/attachment use.
	// Include that dependency and its parents in every mask needing the helper,
	// propagating through procedural dependencies without relying on bone order.
	for ( int pass = 0; pass < nb; ++pass )
		for ( int k = 0; k < nb; ++k )
			if ( proceduralControl[k] >= 0 )
				Up::Mark( boneFlags, bones, proceduralControl[k], boneFlags[k] & ( LODMASK | BONE_USED_BY_HITBOX | BONE_USED_BY_ATTACHMENT ) );
	for ( int k = 0; k < nb; ++k ) w.PutI( bo + 216 * k + 160, boneFlags[k] );
	// bone table by name (insertion sort on lowercase name)
	{
		int btn = w.Alloc( nb, 1 ); w.PutI( 364, btn );
		CUtlVector<int> order; for ( int k = 0; k < nb; k++ ) order.AddToTail( k );
		for ( int x = 1; x < nb; x++ )
			for ( int y = x; y > 0 && V_stricmp( bones[order[y - 1]].name.Get(), bones[order[y]].name.Get() ) > 0; y-- )
			{ int t = order[y]; order[y] = order[y - 1]; order[y - 1] = t; }
		for ( int x = 0; x < nb; x++ ) w.b[btn + x] = (byte_t)order[x];
	}
	// controllers
	{
		int nc = s.I( 164 ), ci = s.I( 168 );
		if ( nc < 0 || nc > 64 || !s.At( ci, 56 * nc ) ) { report = "bad controllers"; return false; }
		int co = w.Alloc( 56 * nc ); w.PutI( 164, nc ); w.PutI( 168, co );
		w.Copy( co, s.At( ci, 56 * nc ), 56 * nc );
	}
	// hitbox sets
	{
		int hs = w.Alloc( 12 * nhs ); w.PutI( 172, nhs ); w.PutI( 176, hs );
		for ( int h = 0; h < nhs; h++ )
		{
			int so = hsi + 12 * h, o = hs + 12 * h;
			w.String( o, o, s.RStr( so, s.I( so ) ) );
			int nbox = s.I( so + 4 ), bxo = so + s.I( so + 8 );
			int bx = w.Alloc( 68 * nbox ); w.PutI( o + 4, nbox ); w.PutI( o + 8, bx - o );
			for ( int x = 0; x < nbox; x++ )
			{
				int sb = bxo + 68 * x, db = bx + 68 * x;
				w.Copy( db, s.At( sb, 40 ), 40 );
				const char *bname = s.RStr( sb, s.I( sb + 40 ) );
				if ( *bname ) w.String( db, db + 40, bname );
			}
		}
	}
	// animations
	{
		int ad = w.Alloc( 100 * na ); w.PutI( 180, na ); w.PutI( 184, ad );
		for ( int k = 0; k < na; k++ )
		{
			int o = ad + 100 * k;
			Anim_t &an = anims[k];
			int nf = an.numframes;
			w.PutI( o, -o ); w.String( o, o + 4, an.name.Get() );
			w.PutF( o + 8, an.fps ); w.PutI( o + 12, an.flags | LEGACY2003_ANIMATION_MARKER ); w.PutI( o + 16, nf );
			if ( an.movements.Count() )
			{
				int mv = w.Alloc( 44 * an.movements.Count() );
				for ( int j = 0; j < an.movements.Count(); j++ ) w.Copy( mv + 44 * j, an.movements[j], 44 );
				w.PutI( o + 20, an.movements.Count() ); w.PutI( o + 24, mv - o );
			}
			if ( !WriteLegacyIKRules( s, w, ai + 92 * k, o, nf, legacyChainTable, legacyChainCount, nb ) )
			{
				report = "bad legacy IK rule/error data";
				return false;
			}
			byte_t delta = ( an.flags & 0x0004 ) ? 0x10 : 0;
			int start = -1, prev = -1;
			for ( int b = 0; b < nb; b++ )
			{
				const Frame *fr = &an.frames[b * nf];
				bool isConst = true;
				for ( int f = 1; f < nf && isConst; f++ )
				{
					for ( int j = 0; j < 3; j++ ) if ( fabs( fr[f].p[j] - fr[0].p[j] ) >= 1e-4 ) isConst = false;
					if ( fabs( fr[f].q.x - fr[0].q.x ) >= 1e-6 || fabs( fr[f].q.y - fr[0].q.y ) >= 1e-6 ||
						fabs( fr[f].q.z - fr[0].q.z ) >= 1e-6 || fabs( fr[f].q.w - fr[0].q.w ) >= 1e-6 ) isConst = false;
				}
				w.Align( 2 );
				int ao = w.Size();
				if ( isConst )
				{
					byte_t hdr[4] = { (byte_t)b, (byte_t)( 0x20 | 0x01 | delta ), 0, 0 };
					w.Append( hdr, 4 );
					Q4 qn = QNorm( fr[0].q );
					Quaternion64 q64; q64 = Quaternion( (float)qn.x, (float)qn.y, (float)qn.z, (float)qn.w );
					w.Append( &q64, 8 );
					Vector48 v48;
					Vector pos( (float)fr[0].p[0], (float)fr[0].p[1], (float)fr[0].p[2] );
					for ( int j = 0; j < 3; j++ ) pos[j] = clamp( pos[j], -65504.0f, 65504.0f );
					v48 = pos;
					w.Append( &v48, 6 );
				}
				else
				{
					byte_t hdr[4] = { (byte_t)b, (byte_t)( 0x08 | 0x04 | delta ), 0, 0 };
					w.Append( hdr, 4 );
					int vpRot = w.Size(); w.Append( NULL, 6 );
					int vpPos = w.Size(); w.Append( NULL, 6 );
					CUtlVector<short> vals; vals.SetCount( nf );
					for ( int j = 0; j < 3; j++ )
					{
						double sc = rotscale[3 * b + j];
						if ( sc <= 0 ) continue;
						bool any = false;
						for ( int f = 0; f < nf; f++ )
						{
							double e = euler[k][( b * nf + f ) * 3 + j];
							double q = floor( ( e - ( delta ? 0.0 : bones[b].value[3 + j] ) ) / sc + 0.5 );
							vals[f] = (short)( q < -32768 ? -32768 : ( q > 32767 ? 32767 : q ) );
							any |= vals[f] != 0;
						}
						if ( any ) { w.PutS( vpRot + 2 * j, (short)( w.Size() - vpRot ) ); RleChannel( vals, w ); }
					}
					for ( int j = 0; j < 3; j++ )
					{
						double sc = posscale[3 * b + j];
						if ( sc <= 0 ) continue;
						bool any = false;
						for ( int f = 0; f < nf; f++ )
						{
							double q = floor( ( fr[f].p[j] - ( delta ? 0.0 : bones[b].value[j] ) ) / sc + 0.5 );
							vals[f] = (short)( q < -32768 ? -32768 : ( q > 32767 ? 32767 : q ) );
							any |= vals[f] != 0;
						}
						if ( any ) { w.PutS( vpPos + 2 * j, (short)( w.Size() - vpPos ) ); RleChannel( vals, w ); }
					}
				}
				if ( start < 0 ) start = ao;
				if ( prev >= 0 ) w.PutS( prev + 2, (short)( ao - prev ) );
				prev = ao;
			}
			w.PutI( o + 52, 0 ); w.PutI( o + 56, start >= 0 ? start - o : 0 );
		}
	}
	// sequences
	{
		int ns = s.I( 188 ), si = s.I( 192 );
		if ( ns < 0 || ns > 10000 || !s.At( si, 4284 * ns ) ) { report = "bad sequence table"; return false; }
		int sq = w.Alloc( 212 * ns ); w.PutI( 188, ns ); w.PutI( 192, sq );
		for ( int k = 0; k < ns; k++ )
		{
			int so = si + 4284 * k, o = sq + 212 * k;
			int gx = s.I( so + 4156 ), gy = s.I( so + 4160 );
			if ( gx < 1 ) gx = 1; if ( gy < 1 ) gy = 1; if ( gx > 32 ) gx = 32; if ( gy > 32 ) gy = 32;
			w.PutI( o, -o );
			w.String( o, o + 4, s.RStr( so, s.I( so ) ) );
			w.String( o, o + 8, s.RStr( so, s.I( so + 4 ) ) );
			w.PutI( o + 12, s.I( so + 8 ) | LEGACY2003_ANIMATION_MARKER ); w.PutI( o + 16, 0 ); w.PutI( o + 20, s.I( so + 16 ) );
			// v36 stores a 32-bit sequence ID. v48 splits that word into a
			// short sequence ID and pose ID, and changes the layer flag meanings.
			const int nLayers = s.I( so + 4240 );
			const int layerSource = so + s.I( so + 4244 );
			if ( nLayers < 0 || nLayers > 4096 || ( nLayers && !s.At( layerSource, 24 * nLayers ) ) )
			{
				report = "bad autolayer table";
				return false;
			}
			if ( nLayers )
			{
				const int layers = w.Alloc( 24 * nLayers );
				w.PutI( o + 148, nLayers );
				w.PutI( o + 152, layers - o );
				for ( int layer = 0; layer < nLayers; ++layer )
				{
					const int from = layerSource + 24 * layer, to = layers + 24 * layer;
					const int sequence = s.I( from ), flags = s.I( from + 4 );
					if ( sequence < 0 || sequence >= ns || sequence == k )
					{
						report = "bad autolayer sequence";
						return false;
					}
					w.PutS( to, (short)sequence );
					w.PutS( to + 2, -1 ); // v36 has no pose-driven layers
					// v36 STUDIO_WEIGHT=0x40 enables a linear ramp and
					// STUDIO_SPLINE=0x80 smooths it. v48 always ramps, with
					// STUDIO_AL_SPLINE=0x40; 0x80 now means cross-fade.
					w.PutI( to + 4, ( flags & 0x10 ) | ( ( flags & 0x80 ) ? 0x40 : 0 ) );
					const float start = s.F( from + 8 ), end = s.F( from + 20 );
					w.PutF( to + 8, start );
					w.PutF( to + 12, ( flags & 0x40 ) ? s.F( from + 12 ) : start );
					w.PutF( to + 16, ( flags & 0x40 ) ? s.F( from + 16 ) : end );
					w.PutF( to + 20, end );
				}
			}
			int nev = s.I( so + 20 ), evo = so + s.I( so + 24 );
			if ( nev < 0 || nev > 4096 || ( nev && !s.At( evo, 76 * nev ) ) ) { report = "bad events"; return false; }
			if ( nev )
			{
				int ev = w.Alloc( 80 * nev ); w.PutI( o + 24, nev ); w.PutI( o + 28, ev - o );
				for ( int e = 0; e < nev; e++ ) w.Copy( ev + 80 * e, s.At( evo + 76 * e, 76 ), 76 );
			}
			for ( int j = 0; j < 6; j++ ) w.PutF( o + 32 + 4 * j, s.F( so + 28 + 4 * j ) );
			int nblends = s.I( so + 52 );
			w.PutI( o + 56, nblends < 1 ? 1 : nblends );
			int bl = w.Alloc( 2 * gx * gy, 2 );
			for ( int y = 0; y < gy; y++ )
				for ( int x = 0; x < gx; x++ )
					w.PutS( bl + 2 * ( y * gx + x ), (short)s.I( so + 56 + 4 * ( x * 32 + y ) ) );
			w.PutI( o + 60, bl - o );
			w.PutI( o + 68, gx ); w.PutI( o + 72, gy );
			w.PutI( o + 76, s.I( so + 4164 ) ); w.PutI( o + 80, s.I( so + 4168 ) );
			for ( int j = 0; j < 4; j++ ) w.PutF( o + 84 + 4 * j, s.F( so + 4172 + 4 * j ) );
			w.PutI( o + 100, s.I( so + 4188 ) );
			float fin = s.F( so + 4196 ), fout = s.F( so + 4200 );
			w.PutF( o + 104, fin != 0 ? fin : 0.2f ); w.PutF( o + 108, fout != 0 ? fout : 0.2f );
			w.PutI( o + 112, s.I( so + 4204 ) ); w.PutI( o + 116, s.I( so + 4208 ) ); w.PutI( o + 120, s.I( so + 4212 ) );
			w.PutF( o + 124, s.F( so + 4216 ) ); w.PutF( o + 128, s.F( so + 4220 ) ); w.PutF( o + 132, s.F( so + 4224 ) );
			w.PutI( o + 136, s.I( so + 4228 ) ); w.PutI( o + 140, s.I( so + 4232 ) );
			int nSeqRules = s.I( so + 4236 );
			if ( nSeqRules < 0 || nSeqRules > 64 ) { report = "bad sequence IK rule count"; return false; }
			w.PutI( o + 144, nSeqRules );
			int nLocks = s.I( so + 4256 ), lockRel = s.I( so + 4260 );
			// v37 normalization appends the expanded sequence table after its
			// existing payload. Its rebased lock pointers are therefore negative.
			// Validate the resolved address, not the sign of the relative offset.
			if ( nLocks && ( lockRel < -so || lockRel > s.n - so ) ) { report = "bad sequence IK lock offset"; return false; }
			if ( !WriteLegacyIKLocks( s, w, so + lockRel, nLocks, o, 164, 168, legacyChainTable, legacyChainCount, nb ) )
			{
				report = "bad sequence IK locks";
				return false;
			}
			int wl = w.Alloc( 4 * nb );
			int swl = s.I( so + 4248 );
			for ( int b = 0; b < nb; b++ ) w.PutF( wl + 4 * b, swl ? s.F( so + swl + 4 * b ) : 1.0f );
			w.PutI( o + 156, wl - o );
			int pk = s.I( so + 4252 );
			if ( pk && s.At( so + pk, 4 * ( gx + gy ) ) )
			{
				int pko = w.Alloc( 4 * ( gx + gy ) );
				w.Copy( pko, s.At( so + pk, 4 * ( gx + gy ) ), 4 * ( gx + gy ) );
				w.PutI( o + 160, pko - o );
			}
			// Original faceposer metadata distinguishes a body posture from an
			// arm gesture. Dropping it makes both look like gestures in the newer
			// scene player, allowing a full-body posture to deform locomotion.
			// Normalized v37 sequence tables follow their payload, so the owner-
			// relative source offset can legitimately be negative.
			int kvRel = s.I( so + 4264 ), kvSize = s.I( so + 4268 );
			if ( kvSize < 0 || kvSize > s.n || ( kvSize &&
				( kvRel < -so || kvRel > s.n - so || !s.At( so + kvRel, kvSize ) ) ) )
			{
				report = "bad legacy sequence key values";
				return false;
			}
			if ( kvSize )
			{
				int kvOut = w.Alloc( kvSize + 1, 1 );
				w.Copy( kvOut, s.At( so + kvRel, kvSize ), kvSize );
				w.PutI( o + 172, kvOut - o ); w.PutI( o + 176, kvSize );
			}
		}
	}
	// textures, cd paths, skins
	{
		int nt = s.I( 208 ), ti = s.I( 212 );
		if ( nt < 0 || nt > 1024 || !s.At( ti, 32 * nt ) ) { report = "bad textures"; return false; }
		int tx = w.Alloc( 64 * nt ); w.PutI( 204, nt ); w.PutI( 208, tx );
		for ( int k = 0; k < nt; k++ )
		{
			int so = ti + 32 * k, o = tx + 64 * k;
			w.String( o, o, s.RStr( so, s.I( so ) ) ); w.PutI( o + 4, s.I( so + 4 ) );
		}
		int ncd = s.I( 216 ), cdi = s.I( 220 );
		if ( ncd < 0 || ncd > 64 || !s.At( cdi, 4 * ncd ) ) { report = "bad cd textures"; return false; }
		int cd = w.Alloc( 4 * ncd ); w.PutI( 212, ncd ); w.PutI( 216, cd );
		for ( int k = 0; k < ncd; k++ ) w.String( 0, cd + 4 * k, s.Str( s.I( cdi + 4 * k ) ) );
		int nskinref = s.I( 224 ), nfam = s.I( 228 ), skinidx = s.I( 232 );
		int skinBytes = 2 * nskinref * nfam;
		if ( nskinref < 0 || nfam < 0 || skinBytes > 1 << 20 || !s.At( skinidx, skinBytes ) ) { report = "bad skins"; return false; }
		int sk = w.Alloc( skinBytes, 2 ); w.Copy( sk, s.At( skinidx, skinBytes ), skinBytes );
		w.PutI( 220, nskinref ); w.PutI( 224, nfam ); w.PutI( 228, sk );
	}
	// bodyparts / models / meshes
	{
		int bp = w.Alloc( 16 * nbp ); w.PutI( 232, nbp ); w.PutI( 236, bp );
		int start = 0, nextMeshId = 0;
		for ( int p = 0; p < nbp; p++ )
		{
			int so = bpi + 16 * p, o = bp + 16 * p;
			w.String( o, o, s.RStr( so, s.I( so ) ) );
			int nm = s.I( so + 4 ), mi = so + s.I( so + 12 );
			int mo = w.Alloc( 148 * nm ); w.PutI( o + 4, nm ); w.PutI( o + 8, s.I( so + 8 ) ); w.PutI( o + 12, mo - o );
			for ( int m = 0; m < nm; m++ )
			{
				int sm = mi + 140 * m, dm = mo + 148 * m;
				w.Copy( dm, s.At( sm, 64 ), 64 );
				w.PutI( dm + 64, s.I( sm + 64 ) ); w.PutF( dm + 68, s.F( sm + 68 ) );
				int nme = s.I( sm + 72 ), mei = sm + s.I( sm + 76 );
				if ( nme < 0 || nme > 4096 || !s.At( mei, 68 * nme ) ) { report = "bad meshes"; return false; }
				int nv = s.I( sm + 80 );
				int me = w.Alloc( 116 * nme );
				w.PutI( dm + 72, nme ); w.PutI( dm + 76, me - dm );
				w.PutI( dm + 80, nv ); w.PutI( dm + 84, 48 * start ); w.PutI( dm + 88, 16 * start );
				for ( int q = 0; q < nme; q++ )
				{
					int sme = mei + 68 * q, mm = me + 116 * q;
					int mnv = s.I( sme + 8 );
					w.PutI( mm, s.I( sme ) ); w.PutI( mm + 4, dm - mm ); w.PutI( mm + 8, mnv ); w.PutI( mm + 12, s.I( sme + 12 ) );
					w.PutI( mm + 24, s.I( sme + 24 ) ); w.PutI( mm + 28, s.I( sme + 28 ) );
					w.PutI( mm + 32, nextMeshId++ );	// v48 indexes hardware mesh data by a unique ordinal; 2003 models often leave it 0
					for ( int j = 0; j < 3; j++ ) w.PutF( mm + 36 + 4 * j, s.F( sme + 36 + 4 * j ) );
					for ( int j = 0; j < 8; j++ ) w.PutI( mm + 52 + 4 * j, mnv );

					// mesh flexes: 28-byte v36 flex / 28-byte float vertanim -> 60-byte v48 flex / 16-byte float16 vertanim
					int nfl = s.I( sme + 16 ), fli = sme + s.I( sme + 20 );
					if ( nfl > 0 && nfl < 4096 && s.At( fli, 28 * nfl ) )
					{
						int fo = w.Alloc( 60 * nfl );
						w.PutI( mm + 16, nfl ); w.PutI( mm + 20, fo - mm );
						for ( int f = 0; f < nfl; f++ )
						{
							int sf = fli + 28 * f, df = fo + 60 * f;
							w.Copy( df, s.At( sf, 24 ), 24 );	// flexdesc, target0-3, numverts
							int nva = s.I( sf + 20 ), vai = sf + s.I( sf + 24 );
							if ( nva < 0 || nva > 65536 || !s.At( vai, 28 * nva ) ) { w.PutI( df + 20, 0 ); continue; }
							int va = w.Alloc( 16 * nva );
							w.PutI( df + 24, va - df );
							for ( int k = 0; k < nva; k++ )
							{
								int sv = vai + 28 * k, dv = va + 16 * k;
								w.PutS( dv, (short)s.I( sv ) );
								w.b[dv + 2] = 255;	// speed
								w.b[dv + 3] = 127;	// side
								for ( int j = 0; j < 3; j++ )
								{
									float16 d, n;
									d.SetFloat( s.F( sv + 4 + 4 * j ) );
									n.SetFloat( s.F( sv + 16 + 4 * j ) );
									w.Copy( dv + 4 + 2 * j, &d, 2 );
									w.Copy( dv + 10 + 2 * j, &n, 2 );
								}
							}
						}
					}
				}
				// eyeballs: the 2003 struct is the first 124 bytes of the v48 one
				int ney = s.I( sm + 100 ), eyi = sm + s.I( sm + 104 );
				if ( ney > 0 && ney < 16 && s.At( eyi, 140 * ney ) )
				{
					int eo = w.Alloc( 172 * ney );
					w.PutI( dm + 100, ney ); w.PutI( dm + 104, eo - dm );
					for ( int e = 0; e < ney; e++ )
					{
						int se = eyi + 140 * e, de = eo + 172 * e;
						w.Copy( de, s.At( se, 124 ), 124 );
						w.String( de, de, s.RStr( se, s.I( se ) ) );
						// Only the old material slots became unused. Preserve the iris
						// projection scale at 60 and the upper eyelid descriptor at 68.
						w.PutI( de + 56, 0 ); w.PutI( de + 64, 0 );
					}
				}
				start += nv;
			}
		}
	}
	// attachments
	{
		int at = w.Alloc( 92 * natt ); w.PutI( 240, natt ); w.PutI( 244, at );
		for ( int k = 0; k < natt; k++ )
		{
			int so = atti + 60 * k, o = at + 92 * k;
			w.String( o, o, s.RStr( so, s.I( so ) ) );
			w.PutI( o + 4, s.I( so + 4 ) ); w.PutI( o + 8, s.I( so + 8 ) );
			w.Copy( o + 12, s.At( so + 12, 48 ), 48 );
		}
	}
	// node transitions
	{
		int ntrans = s.I( 252 ), transidx = s.I( 256 );
		if ( ntrans > 0 && ntrans < 256 && s.At( transidx, ntrans * ntrans ) )
		{
			int nn = w.Alloc( 4 * ntrans );
			for ( int k = 0; k < ntrans; k++ ) w.String( 0, nn + 4 * k, "" );
			int tr = w.Alloc( ntrans * ntrans, 1 ); w.Copy( tr, s.At( transidx, ntrans * ntrans ), ntrans * ntrans );
			w.PutI( 248, ntrans ); w.PutI( 252, tr ); w.PutI( 256, nn );
		}
	}
	// IK chains
	{
		int nik = legacyChainCount, iki = legacyChainTable;
		if ( nik > 0 )
		{
			int ik = w.Alloc( 16 * nik ); w.PutI( 284, nik ); w.PutI( 288, ik );
			for ( int k = 0; k < nik; k++ )
			{
				int so = iki + 16 * k, o = ik + 16 * k;
				int nl = s.I( so + 8 ), lo = so + s.I( so + 12 );
				if ( nl < 0 || nl > 128 || !s.At( lo, 28 * nl ) ) { report = "bad IK chain links"; return false; }
				w.String( o, o, s.RStr( so, s.I( so ) ) );
				int lk = w.Alloc( 28 * nl );
				// Old contact/limit vectors were unused (old studiomdl only wrote
				// bone IDs). They are not modern ideal knee directions.
				for ( int link = 0; link < nl; ++link )
				{
					int bone = s.I( lo + 28 * link );
					if ( bone < 0 || bone >= nb ) { report = "bad IK chain bone"; return false; }
					w.PutI( lk + 28 * link, bone );
				}
				w.PutI( o + 4, s.I( so + 4 ) ); w.PutI( o + 8, nl ); w.PutI( o + 12, lk - o );
			}
		}
	}
	if ( !WriteLegacyIKLocks( s, w, s.I( 324 ), s.I( 320 ), 0, 320, 324, legacyChainTable, legacyChainCount, nb ) )
	{
		report = "bad autoplay IK locks";
		return false;
	}
	// flex descriptors, controllers and rules (same layout in v36 and v48)
	{
		int nfd = s.I( 260 ), fdi = s.I( 264 );
		if ( nfd > 0 && nfd < 4096 && s.At( fdi, 4 * nfd ) )
		{
			int fd = w.Alloc( 4 * nfd ); w.PutI( 260, nfd ); w.PutI( 264, fd );
			for ( int k = 0; k < nfd; k++ )
				w.String( fd + 4 * k, fd + 4 * k, s.RStr( fdi + 4 * k, s.I( fdi + 4 * k ) ) );
		}
		int nfc = s.I( 268 ), fci = s.I( 272 );
		if ( nfc > 0 && nfc < 4096 && s.At( fci, 20 * nfc ) )
		{
			int fc = w.Alloc( 20 * nfc ); w.PutI( 268, nfc ); w.PutI( 272, fc );
			for ( int k = 0; k < nfc; k++ )
			{
				int so = fci + 20 * k, o = fc + 20 * k;
				w.String( o, o, s.RStr( so, s.I( so ) ) );
				w.String( o, o + 4, s.RStr( so, s.I( so + 4 ) ) );
				// The client resolves each named controller only when this sentinel
				// is -1. Zero aliases every controller to global controller zero,
				// corrupting faces (including a permanently open mouth).
				w.PutI( o + 8, -1 ); w.PutF( o + 12, s.F( so + 12 ) ); w.PutF( o + 16, s.F( so + 16 ) );
			}
		}
		int nfr = s.I( 276 ), fri = s.I( 280 );
		if ( nfr > 0 && nfr < 4096 && s.At( fri, 12 * nfr ) )
		{
			int fr = w.Alloc( 12 * nfr ); w.PutI( 276, nfr ); w.PutI( 280, fr );
			for ( int k = 0; k < nfr; k++ )
			{
				int so = fri + 12 * k, o = fr + 12 * k;
				int nops = s.I( so + 4 ), opi = so + s.I( so + 8 );
				w.PutI( o, s.I( so ) ); w.PutI( o + 4, nops );
				if ( nops > 0 && nops < 4096 && s.At( opi, 8 * nops ) )
				{
					int op = w.Alloc( 8 * nops ); w.Copy( op, s.At( opi, 8 * nops ), 8 * nops );
					w.PutI( o + 8, op - o );
				}
				else
					w.PutI( o + 4, 0 );
			}
		}
	}
	// pose parameters
	{
		int npp = s.I( 300 ), ppi = s.I( 304 );
		if ( npp < 0 || npp > 64 || !s.At( ppi, 20 * npp ) ) { report = "bad pose parameters"; return false; }
		int pp = w.Alloc( 20 * npp ); w.PutI( 300, npp ); w.PutI( 304, pp );
		for ( int k = 0; k < npp; k++ )
		{
			int so = ppi + 20 * k, o = pp + 20 * k;
			w.String( o, o, s.RStr( so, s.I( so ) ) ); w.Copy( o + 4, s.At( so + 4, 16 ), 16 );
		}
	}
	// surface prop, key values, mass/contents
	w.String( 0, 308, *surfaceprop ? surfaceprop : "default" );
	{
		int kvi = s.I( 312 ), kvs = s.I( 316 );
		if ( kvs > 0 && s.At( kvi, kvs ) )
		{
			int kv = w.Alloc( kvs + 1, 1 ); w.Copy( kv, s.At( kvi, kvs ), kvs );
			w.PutI( 312, kv ); w.PutI( 316, kvs );
		}
	}
	w.Copy( 328, s.At( 328, 8 ), 8 );
	w.b[377] = 0; w.b[378] = 0;
	w.FlushStrings();
	w.Align( 4 );
	w.PutI( 76, w.Size() );
	if ( s.bad ) { report = "corrupt model data"; return false; }

	mdlOut.Purge();
	mdlOut.Put( w.b.Base(), w.Size() );

	// ------------------------------------------------------------ VVD
	{
		int hdr[16] = { 0 };
		memcpy( &hdr[0], "IDSV", 4 );
		hdr[1] = 4; hdr[2] = checksum; hdr[3] = 1;
		for ( int j = 0; j < 8; j++ ) hdr[4 + j] = totalVerts;
		hdr[12] = 0; hdr[13] = 0; hdr[14] = 64; hdr[15] = 64 + vvdVerts.Count();
		vvdOut.Purge();
		vvdOut.Put( hdr, sizeof( hdr ) );
		vvdOut.Put( vvdVerts.Base(), vvdVerts.Count() );
		vvdOut.Put( vvdTan.Base(), vvdTan.Count() );
	}

	// ------------------------------------------------------------ VTX6 -> VTX7
	vtxOut.Purge();
	int dx80 = -1, dx90 = -1;
	for ( int i = 0; i < nVtx; i++ )
	{
		CUtlVector<byte_t> data;
		if ( !Vtx6To7( (const byte_t *)pVtx[i].pData, pVtx[i].nSize, checksum, remap, data ) )
		{
			notes.AddToTail( CUtlString( pVtx[i].pSuffix ) + " not converted" );
			continue;
		}
		int k = vtxOut.AddToTail();
		V_strncpy( vtxOut[k].suffix, pVtx[i].pSuffix, sizeof( vtxOut[k].suffix ) );
		vtxOut[k].data.Put( data.Base(), data.Count() );
		if ( !V_stricmp( pVtx[i].pSuffix, ".dx80.vtx" ) ) dx80 = k;
		if ( !V_stricmp( pVtx[i].pSuffix, ".dx90.vtx" ) ) dx90 = k;
	}
	if ( dx90 < 0 && dx80 >= 0 )
	{
		int k = vtxOut.AddToTail();
		V_strncpy( vtxOut[k].suffix, ".dx90.vtx", sizeof( vtxOut[k].suffix ) );
		vtxOut[k].data.Put( vtxOut[dx80].data.Base(), vtxOut[dx80].data.TellMaxPut() );
	}
	if ( !vtxOut.Count() )
	{
		report = "no usable VTX file";
		return false;
	}

	report = "";
	for ( int i = 0; i < notes.Count(); i++ )
	{
		if ( V_strstr( report.Get(), notes[i].Get() ) ) continue;
		if ( report.Length() ) report += "; ";
		report += notes[i];
	}
	return true;
}
