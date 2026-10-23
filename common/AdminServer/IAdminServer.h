#ifndef GAMMASOURCE_COMMON_ADMINSERVER_IADMINSERVER_H
#define GAMMASOURCE_COMMON_ADMINSERVER_IADMINSERVER_H

// The missing AdminServer interface has the same public operations as the
// archived ServerBrowser interface, as implemented by CAdminServer.
#include "ServerBrowser/IServerBrowser.h"

class IAdminServer : public IServerBrowser
{
};

// Reconstructed interface ID: confirm against any original plugin consumer
// before claiming ABI compatibility with precompiled historical modules.
#define ADMINSERVER_INTERFACE_VERSION "AdminServer001"

#endif
