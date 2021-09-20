/*
* Example code to demostrate error code translation
*
* Supported devices are xMap, Saturn, STJ, Mercury / Mercury4, microDXP, FalconXn
*
* Copyright (c) 2008-2020, XIA LLC
* All rights reserved
*
*
*/

#include <stdio.h>
#include <stdlib.h>


#include "handel.h"
#include "handel_errors.h"
#include "handel_constants.h"


int main()
{

    printf("--- Checking Handel error code translationg.\n");

    printf("XIA_SIZE_MISMATCH       -- %s\n", xiaGetErrorText(XIA_SIZE_MISMATCH));
    printf("XIA_PRESET_VALUE_OOR    -- %s\n", xiaGetErrorText(XIA_PRESET_VALUE_OOR));
    printf("XIA_LIVEOUTPUT_OOR      -- %s\n", xiaGetErrorText(XIA_LIVEOUTPUT_OOR));

    printf("None existing error     -- %s\n", xiaGetErrorText(2048));

    return 0;
}


