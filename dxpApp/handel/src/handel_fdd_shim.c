
#include "xia_common.h"

#include "xia_handel.h"

#include "handeldef.h"
#include "handel_generic.h"
#include "handel_errors.h"


HANDEL_SHARED int xiaFddInitialize(void)
{
    return XIA_SUCCESS;
}


HANDEL_SHARED int xiaFddGetFirmware(const char *filename, char *path,
                                    const char *ftype, double pt,
                                    unsigned int nother, char **others,
                                    const char *detectorType,
                                    char newFilename[MAXFILENAME_LEN],
                                    char rawFilename[MAXFILENAME_LEN])
{
    UNUSED(filename);
    UNUSED(path);
    UNUSED(ftype);
    UNUSED(pt);
    UNUSED(nother);
    UNUSED(others);
    UNUSED(detectorType);
    UNUSED(newFilename);
    UNUSED(rawFilename);


    return XIA_SUCCESS;
}
