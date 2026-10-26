#ifndef GAMMASOURCE_COMMON_INFO_H
#define GAMMASOURCE_COMMON_INFO_H

// Parse Source/GoldSrc backslash-separated key-value server information.
// Return value is a temporary buffer overwritten after four further calls.
const char *Info_ValueForKey(const char *info, const char *key);

#endif
