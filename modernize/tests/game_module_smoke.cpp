#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

typedef void *(__cdecl *InterfaceFactory)(const char *, int *);

int main(int argc, char **argv)
{
    if (argc < 3) return 2;
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    HMODULE module = LoadLibraryExA(argv[1], NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!module) {
        fprintf(stderr, "LoadLibrary failed with Windows error %lu\n", GetLastError());
        return 3;
    }
    InterfaceFactory factory = (InterfaceFactory)GetProcAddress(module, "CreateInterface");
    if (!factory) return 4;
    for (int i = 2; i < argc; ++i) {
        int status = -1;
        if (!factory(argv[i], &status) || status != 0) {
            fprintf(stderr, "Missing interface %s (status %d)\n", argv[i], status);
            return 5;
        }
        printf("Loaded interface %s\n", argv[i]);
    }
    int status = -1;
    if (factory("HL2ModernMissingInterface", &status) || status == 0) return 6;
    FreeLibrary(module);
    puts("Game DLL loaded, interfaces instantiated and module unloaded.");
    return 0;
}
