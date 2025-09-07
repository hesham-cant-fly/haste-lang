#ifndef __ERROR_H
#define __ERROR_H

#include <stdint.h>

typedef enum Error : uint8_t {
  OK = true,
  ERROR = false,
} error_t;

#endif // !__ERROR_H
