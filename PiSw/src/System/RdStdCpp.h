
#pragma once

#include <stddef.h>

void* operator new(size_t sz);
void *operator new[] (size_t nSize);
void operator delete(void* ptr) noexcept;
void operator delete[](void* ptr) noexcept;
void operator delete(void* ptr, size_t nSize) noexcept;
void operator delete[](void* ptr, size_t nSize) noexcept;
