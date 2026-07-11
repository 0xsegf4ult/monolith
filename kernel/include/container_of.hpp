#pragma once

#include <stddef.h>

template <typename T, typename P>
inline T* container_of_tmp(P* ptr, size_t offset)
{
	if(!ptr) return nullptr;
	return reinterpret_cast<T*>(reinterpret_cast<char*>(ptr) - offset);
}

#define container_of(ptr, type, member) \
	container_of_tmp<type>(ptr, offsetof(type, member))

