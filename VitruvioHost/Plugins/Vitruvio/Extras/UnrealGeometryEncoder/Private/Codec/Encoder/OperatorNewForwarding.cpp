/* Copyright 2026 Esri
 *
 * Licensed under the Apache License Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// On Linux, Vitruvio loads this library with RTLD_DEEPBIND and it links all PRT libraries, so symbols are looked up in
// this library first for all of them. The libstdc++ shared with other libraries in the Unreal process uses the
// allocator of the main program (Unreal exports its own operator new/delete). These definitions make PRT use the same
// allocator, otherwise memory allocated by PRT and released by libstdc++ (or vice versa) would mix allocators.

#if defined(__linux__)

#include <cstdlib>
#include <dlfcn.h>
#include <new>

#define VITRUVIO_EXPORT __attribute__((visibility("default")))

namespace
{
void* MainProgramSymbol(const char* name)
{
	// The main program scope contains the executable and libraries loaded at startup or with RTLD_GLOBAL, but not this
	// library (loaded with RTLD_LOCAL). If the host does not provide the operator, use the next definition (libstdc++).
	static void* mainProgram = dlopen(nullptr, RTLD_NOW);
	void* symbol = dlsym(mainProgram, name);
	if (symbol == nullptr)
	{
		symbol = dlsym(RTLD_NEXT, name);
	}
	if (symbol == nullptr)
	{
		std::abort();
	}
	return symbol;
}
} // namespace

#define FORWARD(Signature, Name) static const auto forward = reinterpret_cast<Signature>(MainProgramSymbol(Name))

VITRUVIO_EXPORT void* operator new(std::size_t size)
{
	FORWARD(void* (*)(std::size_t), "_Znwm");
	return forward(size);
}

VITRUVIO_EXPORT void* operator new[](std::size_t size)
{
	FORWARD(void* (*)(std::size_t), "_Znam");
	return forward(size);
}

VITRUVIO_EXPORT void* operator new(std::size_t size, const std::nothrow_t& tag) noexcept
{
	FORWARD(void* (*)(std::size_t, const std::nothrow_t&), "_ZnwmRKSt9nothrow_t");
	return forward(size, tag);
}

VITRUVIO_EXPORT void* operator new[](std::size_t size, const std::nothrow_t& tag) noexcept
{
	FORWARD(void* (*)(std::size_t, const std::nothrow_t&), "_ZnamRKSt9nothrow_t");
	return forward(size, tag);
}

VITRUVIO_EXPORT void* operator new(std::size_t size, std::align_val_t alignment)
{
	FORWARD(void* (*)(std::size_t, std::align_val_t), "_ZnwmSt11align_val_t");
	return forward(size, alignment);
}

VITRUVIO_EXPORT void* operator new[](std::size_t size, std::align_val_t alignment)
{
	FORWARD(void* (*)(std::size_t, std::align_val_t), "_ZnamSt11align_val_t");
	return forward(size, alignment);
}

VITRUVIO_EXPORT void* operator new(std::size_t size, std::align_val_t alignment, const std::nothrow_t& tag) noexcept
{
	FORWARD(void* (*)(std::size_t, std::align_val_t, const std::nothrow_t&), "_ZnwmSt11align_val_tRKSt9nothrow_t");
	return forward(size, alignment, tag);
}

VITRUVIO_EXPORT void* operator new[](std::size_t size, std::align_val_t alignment, const std::nothrow_t& tag) noexcept
{
	FORWARD(void* (*)(std::size_t, std::align_val_t, const std::nothrow_t&), "_ZnamSt11align_val_tRKSt9nothrow_t");
	return forward(size, alignment, tag);
}

VITRUVIO_EXPORT void operator delete(void* ptr) noexcept
{
	FORWARD(void (*)(void*), "_ZdlPv");
	forward(ptr);
}

VITRUVIO_EXPORT void operator delete[](void* ptr) noexcept
{
	FORWARD(void (*)(void*), "_ZdaPv");
	forward(ptr);
}

VITRUVIO_EXPORT void operator delete(void* ptr, std::size_t size) noexcept
{
	FORWARD(void (*)(void*, std::size_t), "_ZdlPvm");
	forward(ptr, size);
}

VITRUVIO_EXPORT void operator delete[](void* ptr, std::size_t size) noexcept
{
	FORWARD(void (*)(void*, std::size_t), "_ZdaPvm");
	forward(ptr, size);
}

VITRUVIO_EXPORT void operator delete(void* ptr, const std::nothrow_t& tag) noexcept
{
	FORWARD(void (*)(void*, const std::nothrow_t&), "_ZdlPvRKSt9nothrow_t");
	forward(ptr, tag);
}

VITRUVIO_EXPORT void operator delete[](void* ptr, const std::nothrow_t& tag) noexcept
{
	FORWARD(void (*)(void*, const std::nothrow_t&), "_ZdaPvRKSt9nothrow_t");
	forward(ptr, tag);
}

VITRUVIO_EXPORT void operator delete(void* ptr, std::align_val_t alignment) noexcept
{
	FORWARD(void (*)(void*, std::align_val_t), "_ZdlPvSt11align_val_t");
	forward(ptr, alignment);
}

VITRUVIO_EXPORT void operator delete[](void* ptr, std::align_val_t alignment) noexcept
{
	FORWARD(void (*)(void*, std::align_val_t), "_ZdaPvSt11align_val_t");
	forward(ptr, alignment);
}

VITRUVIO_EXPORT void operator delete(void* ptr, std::size_t size, std::align_val_t alignment) noexcept
{
	FORWARD(void (*)(void*, std::size_t, std::align_val_t), "_ZdlPvmSt11align_val_t");
	forward(ptr, size, alignment);
}

VITRUVIO_EXPORT void operator delete[](void* ptr, std::size_t size, std::align_val_t alignment) noexcept
{
	FORWARD(void (*)(void*, std::size_t, std::align_val_t), "_ZdaPvmSt11align_val_t");
	forward(ptr, size, alignment);
}

VITRUVIO_EXPORT void operator delete(void* ptr, std::align_val_t alignment, const std::nothrow_t& tag) noexcept
{
	FORWARD(void (*)(void*, std::align_val_t, const std::nothrow_t&), "_ZdlPvSt11align_val_tRKSt9nothrow_t");
	forward(ptr, alignment, tag);
}

VITRUVIO_EXPORT void operator delete[](void* ptr, std::align_val_t alignment, const std::nothrow_t& tag) noexcept
{
	FORWARD(void (*)(void*, std::align_val_t, const std::nothrow_t&), "_ZdaPvSt11align_val_tRKSt9nothrow_t");
	forward(ptr, alignment, tag);
}

#endif
