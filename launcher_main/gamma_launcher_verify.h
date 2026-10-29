#pragma once
#include <string>
#include <vector>
#include "gamma_launcher_ui.h"

class IMatRenderContext;

// Explicit component integration mode. It renders this application's own
// framebuffer to files and calls its own controls; it never reads the desktop
// or synthesizes operating-system input.
class CGammaLauncherVerification
{
public:
	CGammaLauncherVerification( const std::wstring &output, bool play, int canvasWide, int canvasTall );
	bool Enabled() const { return !m_Output.empty(); }
	bool Begin( vgui::Panel *panel, double now );
	void AfterPaint( IMatRenderContext *context, vgui::Panel *panel, double now );
	void RecordLaunch( unsigned long process, const std::wstring &command, const GammaLauncherProfile &profile );
	void RecordLaunchError( const char *error );
	void Closed();
	int ExitCode() const;
private:
	struct Sample
	{
		Sample() : state(), wide( 0 ), tall( 0 ), viewportX( 0 ), viewportY( 0 ), viewportWide( 0 ), viewportTall( 0 ), rootWide( 0 ), rootTall( 0 ), rootVisible( false ) {}
		GammaLauncherVerificationState state;
		std::string file, pixelHash;
		int wide, tall;
		int viewportX, viewportY, viewportWide, viewportTall, rootWide, rootTall;
		bool rootVisible;
	};
	struct Record
	{
		std::string alias, error;
		Sample first, second;
		bool passed;
	};
	bool Capture( IMatRenderContext *context, vgui::Panel *panel, const std::string &name, Sample &sample );
	void CheckModules();
	void SaveReport();
	void Complete( IMatRenderContext *context, vgui::Panel *panel, double now );
	std::wstring m_Output;
	bool m_Play, m_Passed, m_Completed, m_EngineAbsent, m_ClientAbsent, m_ServerAbsent;
	bool m_PlayInvoked, m_PlayFinished;
	int m_CanvasWide, m_CanvasTall, m_Index, m_Phase;
	double m_ChangedAt, m_FirstAt;
	unsigned long m_Process;
	std::wstring m_Command;
	GammaLauncherProfile m_LaunchProfile;
	Sample m_First;
	std::vector<Record> m_Records;
	std::vector<std::string> m_Errors;
};
