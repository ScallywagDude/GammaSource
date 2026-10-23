// Reconstructed from Tracker/TrackerUI/TrackerUser.cpp (CTrackerUser).
// Source-level interface only. The original binary interface identifier and
// interoperability with external Tracker binaries have not been verified.
#ifndef GAMMASOURCE_COMMON_ITRACKERUSER_H
#define GAMMASOURCE_COMMON_ITRACKERUSER_H
#ifdef _WIN32
#pragma once
#endif
#include "interface.h"

class ITrackerUser : public IBaseInterface
{
public:
    virtual bool IsValid() = 0;
    virtual int GetTrackerID() = 0;
    virtual const char *GetUserName(int userID) = 0;
    virtual const char *GetFirstName(int userID) = 0;
    virtual const char *GetLastName(int userID) = 0;
    virtual const char *GetEmail(int userID) = 0;
    virtual bool IsFriend(int friendID) = 0;
    virtual void RequestAuthorizationFromUser(int potentialFriendID) = 0;
    virtual int GetUserStatus(int friendID) = 0;
    virtual bool GetUserGameAddress(int friendID, int *ip, int *port) = 0;
    virtual int GetNumberOfFriends() = 0;
    virtual int GetFriendTrackerID(int friendIndex) = 0;
    virtual void SetCanReceiveMessages(bool state) = 0;
};
// Both the reconstructed provider and consumer use this identifier. Do not
// assume it matches a separately compiled historical Tracker component.
#define TRACKERUSER_INTERFACE_VERSION "TrackerUser001"
#endif
