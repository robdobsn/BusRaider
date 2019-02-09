#define UC(c)   ((unsigned char)c)

char rdisspace (unsigned char c) 
{
  if ( c == ' '
    || c == '\f'
    || c == '\n'
    || c == '\r'
    || c == '\t'
    || c == '\v' ) 
      return 1;

  return 0;
}

char rdisupper (unsigned char c)
{
    if ( c >= UC('A') && c <= UC('Z') )
        return 1;
    return 0;
}

char rdtolower(char c)
{
    if (rdisupper(c))
    {
        return c + ('a' - 'A');
    }
    return c;
}

bool matches(const char* s1, const char* s2, int maxLen)
{
    const char* p1 = s1;
    const char* p2 = s2;
    // Skip whitespace at start of received string
    while (*p1 == ' ')
        p1++;
    // Check match from start of received string
    for (int i = 0; i < maxLen; i++)
    {
        if (*p2 == 0)
        {
            while (rdisspace(*p1))
                p1++;
            return *p1 == 0;
        }
        if (*p1 == 0)
            return false;
        if (rdtolower(*p1++) != rdtolower(*p2++))
            return false;
    }
    return false;
}

#include "stdio.h"

int main()
{
    printf("Matches %d", matches("\n", "\n", 10));
    return 0;
}