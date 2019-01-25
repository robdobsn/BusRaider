
#pragma once

#include <stddef.h>

void* operator new(size_t sz);
void operator delete(void* ptr) noexcept;
