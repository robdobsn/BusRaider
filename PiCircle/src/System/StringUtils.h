// String Utils
// Rob Dobson 2019

#pragma once

#include <stddef.h>
#include <circle/util.h>

#ifndef HAVE_STRLCPY
extern size_t strlcpy(char       *dst,        /* O - Destination string */
              const char *src,      /* I - Source string */
          size_t      size);
#endif

#ifndef HAVE_STRLCAT
extern size_t strlcat(char       *dst,        /* O - Destination string */
              const char *src,      /* I - Source string */
          size_t     size);
#endif

