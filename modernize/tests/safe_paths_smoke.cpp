#include "../safe_paths.h"
#include <cassert>
#include <cstring>
#include <cstdio>
int main()
{
    using HL2ModernPaths::CopyDirectory;
    char output[16];
    assert(CopyDirectory("maps/", output, sizeof(output)));
    assert(std::strcmp(output, "maps") == 0);
    assert(CopyDirectory("C:\\", output, sizeof(output)));
    assert(std::strcmp(output, "C:\\") == 0);
    assert(CopyDirectory("/", output, sizeof(output)));
    assert(std::strcmp(output, "/") == 0);
    assert(CopyDirectory("C:\\games\\", output, sizeof(output)));
    assert(std::strcmp(output, "C:\\games") == 0);
    assert(CopyDirectory("a\\\\", output, sizeof(output)));
    assert(std::strcmp(output, "a") == 0);
    assert(!CopyDirectory("", output, sizeof(output)));
    assert(!CopyDirectory(NULL, output, sizeof(output)));
    assert(!CopyDirectory("aaaaaaaaaaaaaaaa", output, sizeof(output)));
    assert(!CopyDirectory("b", output, 0));
    std::puts("path handling smoke tests passed");
}
