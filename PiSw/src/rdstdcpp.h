
#pragma once

#include "globaldefs.h"

void* operator new(size_t sz);
void operator delete(void* ptr) noexcept;
