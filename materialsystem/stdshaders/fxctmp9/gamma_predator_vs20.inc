#include "shaderlib/cshader.h"
class gamma_predator_vs20_Static_Index
{
public:
	gamma_predator_vs20_Static_Index() {  }
	int GetIndex() { return 0; }
};
#define shaderStaticTest_gamma_predator_vs20 0
class gamma_predator_vs20_Dynamic_Index
{
private:
	int m_nCOMPRESSED_VERTS;
public:
	void SetCOMPRESSED_VERTS( int i ) { Assert( i >= 0 && i <= 1 ); m_nCOMPRESSED_VERTS = i; }
	void SetCOMPRESSED_VERTS( bool i ) { m_nCOMPRESSED_VERTS = i ? 1 : 0; }
private:
	int m_nSKINNING;
public:
	void SetSKINNING( int i ) { Assert( i >= 0 && i <= 1 ); m_nSKINNING = i; }
	void SetSKINNING( bool i ) { m_nSKINNING = i ? 1 : 0; }
public:
	gamma_predator_vs20_Dynamic_Index() { m_nCOMPRESSED_VERTS = 0; m_nSKINNING = 0; }
	int GetIndex() { return ( 1 * m_nCOMPRESSED_VERTS ) + ( 2 * m_nSKINNING ) + 0; }
};
#define shaderDynamicTest_gamma_predator_vs20 vsh_forgot_to_set_dynamic_COMPRESSED_VERTS + vsh_forgot_to_set_dynamic_SKINNING + 0
