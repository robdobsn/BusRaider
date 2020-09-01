
// Bus Raider
// Low-level library
// Rob Dobson 2019

#include "lowlib.h"
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <circle/timer.h>

#ifdef __cplusplus
extern "C" {
#endif

unsigned long micros()
{
    return CTimer::GetClockTicks();
}

unsigned long millis()
{
    return CTimer::GetClockTicks() / 1000;
}

void microsDelay(unsigned long us)
{
    unsigned long timeNow = micros();
    while (!isTimeout(micros(), timeNow, us)) {
        // Do nothing
    }
}

int isTimeout(unsigned long curTime, unsigned long lastTime, unsigned long maxDuration)
{
    if (curTime >= lastTime) {
        return curTime > lastTime + maxDuration;
    }
    return ((ULONG_MAX - lastTime) + curTime) > maxDuration;
}

unsigned long timeToTimeout(unsigned long curTime, unsigned long lastTime, unsigned long maxDuration)
{
    if (curTime >= lastTime)
    {
        if (curTime > lastTime + maxDuration)
        {
            return 0;
        }
        return maxDuration - (curTime - lastTime);
    }
    if (ULONG_MAX - (lastTime - curTime) > maxDuration)
    {
        return 0;
    }
    return maxDuration - (ULONG_MAX - (lastTime - curTime));
}

// Following Copyright (c) 2011 Apple, Inc. All rights reserved.

size_t
strlcpy(char * dst, const char * src, size_t maxlen) {
    const size_t srclen = strlen(src);
    if (srclen + 1 < maxlen) {
        memcpy(dst, src, srclen + 1);
    } else if (maxlen != 0) {
        memcpy(dst, src, maxlen - 1);
        dst[maxlen-1] = '\0';
    }
    return srclen;
}

size_t
strlcat(char * dst, const char * src, size_t maxlen) {
    const size_t srclen = strlen(src);
    const size_t dstlen = strnlen(dst, maxlen);
    if (dstlen == maxlen) return maxlen+srclen;
    if (srclen < maxlen-dstlen) {
        memcpy(dst+dstlen, src, srclen+1);
    } else {
        memcpy(dst+dstlen, src, maxlen-1);
        dst[dstlen+maxlen-1] = '\0';
    }
    return dstlen + srclen;
}

size_t
strnlen(const char *s, size_t maxlen)
{
	size_t len;

	for (len = 0; len < maxlen; len++, s++) {
		if (!*s)
			break;
	}
	return (len);
}

#if STDLIB_SUPPORT <= 1

int sprintf (char *buf, const char *fmt, ...)
{
	va_list var;
	va_start (var, fmt);

	CString Msg;
	Msg.FormatV (fmt, var);

	va_end (var);

	strcpy (buf, (const char *) Msg);

	return Msg.GetLength ();
}

int snprintf (char *buf, size_t size, const char *fmt, ...)
{
	va_list var;
	va_start (var, fmt);

	CString Msg;
	Msg.FormatV (fmt, var);

	va_end (var);

	size_t len = Msg.GetLength ();
	if (--size < len)
	{
		len = size;
	}

	memcpy (buf, (const char *) Msg, len);
	buf[len] = '\0';

	return len;
}

int vsnprintf (char *buf, size_t size, const char *fmt, va_list var)
{
	CString Msg;
	Msg.FormatV (fmt, var);

	size_t len = Msg.GetLength ();
	if (--size < len)
	{
		len = size;
	}

	memcpy (buf, (const char *) Msg, len);
	buf[len] = '\0';

	return len;
}

#endif

// // Following Copyright (C) 1991-2019 Free Software Foundation, Inc.

// /* Compare S1 and S2, ignoring case, returning less than, equal to or
//    greater than zero if S1 is lexicographically less than,
//    equal to or greater than S2.  */
// int
// __strcasecmp (const char *s1, const char *s2)
// {
//   const unsigned char *p1 = (const unsigned char *) s1;
//   const unsigned char *p2 = (const unsigned char *) s2;
//   int result;
//   if (p1 == p2)
//     return 0;
//   while ((result = tolower (*p1) - tolower (*p2++)) == 0)
//     if (*p1++ == '\0')
//       break;
//   return result;
// }

// /*
//  * Convert a string to a long integer.
//  *
//  * Ignores `locale' stuff.  Assumes that the upper and lower case
//  * alphabets and digits are each contiguous.
//  */
// long strtol(const char *nptr, char **endptr, register int base)
// {
// 	register const char *s = nptr;
// 	register unsigned long acc;
// 	register int c;
// 	register unsigned long cutoff;
// 	register int neg = 0, any, cutlim;

// 	/*
// 	 * Skip white space and pick up leading +/- sign if any.
// 	 * If base is 0, allow 0x for hex and 0 for octal, else
// 	 * assume decimal; if base is already 16, allow 0x.
// 	 */
// 	do {
// 		c = *s++;
// 	} while (isspace(c));
// 	if (c == '-') {
// 		neg = 1;
// 		c = *s++;
// 	} else if (c == '+')
// 		c = *s++;
// 	if ((base == 0 || base == 16) &&
// 	    c == '0' && (*s == 'x' || *s == 'X')) {
// 		c = s[1];
// 		s += 2;
// 		base = 16;
// 	} else if ((base == 0 || base == 2) &&
// 	    c == '0' && (*s == 'b' || *s == 'B')) {
// 		c = s[1];
// 		s += 2;
// 		base = 2;
// 	}
// 	if (base == 0)
// 		base = c == '0' ? 8 : 10;

// 	/*
// 	 * Compute the cutoff value between legal numbers and illegal
// 	 * numbers.  That is the largest legal value, divided by the
// 	 * base.  An input number that is greater than this value, if
// 	 * followed by a legal input character, is too big.  One that
// 	 * is equal to this value may be valid or not; the limit
// 	 * between valid and invalid numbers is then based on the last
// 	 * digit.  For instance, if the range for longs is
// 	 * [-2147483648..2147483647] and the input base is 10,
// 	 * cutoff will be set to 214748364 and cutlim to either
// 	 * 7 (neg==0) or 8 (neg==1), meaning that if we have accumulated
// 	 * a value > 214748364, or equal but the next digit is > 7 (or 8),
// 	 * the number is too big, and we will return a range error.
// 	 *
// 	 * Set any if any `digits' consumed; make it negative to indicate
// 	 * overflow.
// 	 */
// 	cutoff = neg ? -(unsigned long)LONG_MIN : LONG_MAX;
// 	cutlim = cutoff % (unsigned long)base;
// 	cutoff /= (unsigned long)base;
// 	for (acc = 0, any = 0;; c = *s++) {
// 		if (isdigit(c))
// 			c -= '0';
// 		else if (isalpha(c))
// 			c -= isupper(c) ? 'A' - 10 : 'a' - 10;
// 		else
// 			break;
// 		if (c >= base)
// 			break;
// 		if ((any < 0) || (acc > cutoff) || ((acc == cutoff) && (c > cutlim)))
// 			any = -1;
// 		else {
// 			any = 1;
// 			acc *= base;
// 			acc += c;
// 		}
// 	}
// 	if (any < 0) {
// 		acc = neg ? LONG_MIN : LONG_MAX;
// //		errno = ERANGE;
// 	} else if (neg)
// 		acc = -acc;
// 	if (endptr != 0)
// 		*endptr = (char *)(any ? s - 1 : nptr);
// 	return (acc);
// }

// /*
//  * Convert a string to an unsigned long integer.
//  *
//  * Ignores `locale' stuff.  Assumes that the upper and lower case
//  * alphabets and digits are each contiguous.
//  */
// unsigned long strtoul(const char *nptr, char **endptr, register int base)
// {
// 	register const char *s = nptr;
// 	register unsigned long acc;
// 	register int c;
// 	register unsigned long cutoff;
// 	register int neg = 0, any, cutlim;

// 	/*
// 	 * See strtol for comments as to the logic used.
// 	 */
// 	do {
// 		c = *s++;
// 	} while (isspace(c));
// 	if (c == '-') {
// 		neg = 1;
// 		c = *s++;
// 	} else if (c == '+')
// 		c = *s++;
// 	if ((base == 0 || base == 16) &&
// 	    c == '0' && (*s == 'x' || *s == 'X')) {
// 		c = s[1];
// 		s += 2;
// 		base = 16;
// 	} else if ((base == 0 || base == 2) &&
// 	    c == '0' && (*s == 'b' || *s == 'B')) {
// 		c = s[1];
// 		s += 2;
// 		base = 2;
// 	}
// 	if (base == 0)
// 		base = c == '0' ? 8 : 10;
// 	cutoff = (unsigned long)ULONG_MAX / (unsigned long)base;
// 	cutlim = (unsigned long)ULONG_MAX % (unsigned long)base;
// 	for (acc = 0, any = 0;; c = *s++) {
// 		if (isdigit(c))
// 			c -= '0';
// 		else if (isalpha(c))
// 			c -= isupper(c) ? 'A' - 10 : 'a' - 10;
// 		else
// 			break;
// 		if (c >= base)
// 			break;
// 		if ((any < 0) || (acc > cutoff) || ((acc == cutoff) && (c > cutlim)))
// 			any = -1;
// 		else {
// 			any = 1;
// 			acc *= base;
// 			acc += c;
// 		}
// 	}
// 	if (any < 0) {
// 		acc = ULONG_MAX;
// //		errno = ERANGE;
// 	} else if (neg)
// 		acc = -acc;
// 	if (endptr != 0)
// 		*endptr = (char *)(any ? s - 1 : nptr);
// 	return (acc);
// }

#ifdef __cplusplus
}
#endif
