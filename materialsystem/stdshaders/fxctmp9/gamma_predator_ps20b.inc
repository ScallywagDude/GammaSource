#include "shaderlib/cshader.h"
class gamma_predator_ps20b_Static_Index
{
private:
	int m_nMODE;
public:
	void SetMODE( int i ) { Assert( i >= 0 && i <= 3 ); m_nMODE = i; }
	void SetMODE( bool i ) { m_nMODE = i ? 1 : 0; }
public:
	gamma_predator_ps20b_Static_Index() { m_nMODE = 0; }
	int GetIndex() { return ( 1 * m_nMODE ) + 0; }
};
#define shaderStaticTest_gamma_predator_ps20b psh_forgot_to_set_static_MODE + 0
class gamma_predator_ps20b_Dynamic_Index
{
public:
	gamma_predator_ps20b_Dynamic_Index() {  }
	int GetIndex() { return 0; }
};
#define shaderDynamicTest_gamma_predator_ps20b 0
