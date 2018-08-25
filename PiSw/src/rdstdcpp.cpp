
#include "rdstdcpp.h"
#include "nmalloc.h"

void* operator new(size_t sz) {
    return nmalloc_malloc(sz);
}
void operator delete(void* ptr) noexcept
{
    nmalloc_free(&ptr);
}
