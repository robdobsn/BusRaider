#ifndef _GLOBAL_DEFS_H_
#define _GLOBAL_DEFS_H_

#define ON  2-
#define OFF 1-
#define ENABLED(x) ( (x 0) == 2 )

#ifndef bool
#define bool int
#endif

#ifndef NULL
#define NULL 0
#endif

typedef	signed char int8_t;
typedef	unsigned char uint8_t;
typedef	short int16_t;
typedef	unsigned short uint16_t;
typedef	int	int32_t;
typedef	unsigned int uint32_t;
typedef	long long int64_t;
typedef	unsigned long long uint64_t;

#endif
