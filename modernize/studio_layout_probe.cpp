// Read-only native check using the same model validator as the rebuilt loader.
#include <stdio.h>
#include <stdlib.h>
#include "model_preview_studio_validation.h"

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    FILE *paths=fopen(argv[1],"rt");
    if (!paths) return 2;
    char path[4096];
    int failures=0;
    while (fgets(path,sizeof(path),paths))
    {
        path[strcspn(path,"\r\n")]=0;
        FILE *input=fopen(path,"rb");
        if (!input) { printf("file_error\t%s\n",path); ++failures; continue; }
        fseek(input,0,SEEK_END);
        const long length=ftell(input);
        rewind(input);
        if (length<(long)sizeof(studiohdr_t) || length>128*1024*1024)
        { fclose(input); printf("invalid_length\t%s\n",path); ++failures; continue; }
        void *bytes=malloc(length);
        if (!bytes) { fclose(input); fclose(paths); return 2; }
        const bool read=fread(bytes,1,length,input)==(size_t)length;
        fclose(input);
        studiohdr_t *header=(studiohdr_t *)bytes;
        const int version=header->version;
        HL2ModernModelPreview::StudioDataValidator validator(bytes,length);
        // The historical 35->36 converter writes the bone table; bound it first.
        if (read && version==35 && header->numbones>=0 && header->numbones<=MAXSTUDIOBONES &&
            validator.Range(header->boneindex,header->numbones,sizeof(mstudiobone_t)))
            Studio_ConvertStudioHdrToNewVersion(header);
        const bool valid=read && validator.Validate(true);
        printf("%s\t%d\t%s\n",valid?"layout_matches_loader":"layout_rejected",version,path);
        if (!valid) ++failures;
        free(bytes);
    }
    fclose(paths);
    return failures?1:0;
}
