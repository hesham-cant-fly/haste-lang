#ifndef __ERROR_H
#define __ERROR_H

#include "common.h"
#include <stdbool.h>

defenum(error_t, bool,
        {
            OK = true,
            ERROR = false,
        });

#endif // !__ERROR_H
