#pragma once

#include "vgui/VGUI.h"
namespace vgui { class Panel; }

struct GammaLauncherProfile
{
	char playerName[128];
	char playerModel[64];
	bool thirdPerson;
};

typedef bool ( *GammaLauncherPlayCallback )( const GammaLauncherProfile &profile, char *error, int errorBytes, void *context );
typedef void ( *GammaLauncherExitCallback )( void *context );

// The host provides a standalone VGUI/MaterialSystem root. Neither this UI nor
// the preview loads the game engine, client DLL, a map or a server.
vgui::Panel *GammaCreateLauncherUI( vgui::VPANEL parent, const GammaLauncherProfile &profile,
	GammaLauncherPlayCallback play, GammaLauncherExitCallback exit, void *context );
void GammaResizeLauncherUI( vgui::Panel *panel, int wide, int tall );

// Component integration checks used only by the host's explicit verification
// mode. These call the same controls and Play handler, without OS input hooks.
struct GammaLauncherVerificationState
{
	GammaLauncherProfile profile;
	bool previewAvailable;
	int dropdownIndex;
	char loadedModel[260];
	char sequence[128];
	float animationTime;
	int boneCount;
	int previewX, previewY, previewWide, previewTall;
	unsigned int framePaintCount, previewPaintCount;
	bool previewVisible, previewFullyVisible;
	int previewAlpha;
	int previewClipLeft, previewClipTop, previewClipRight, previewClipBottom;
	int schemeHandle, frameBgAlpha;
	float surfaceAlpha;
};

bool GammaLauncherVerifySelect( vgui::Panel *panel, const char *alias );
bool GammaLauncherVerifyProfileSet( vgui::Panel *panel, const GammaLauncherProfile &profile );
bool GammaLauncherVerifyReport( vgui::Panel *panel, GammaLauncherVerificationState &state );
void GammaLauncherVerifyPlay( vgui::Panel *panel );
