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

#pragma once

#include "HAL/Platform.h"

#if PLATFORM_LINUX

#include <string>

// On Linux PRT is loaded at runtime instead of being linked. PRT and its extensions are built against libstdc++, while
// Unreal links its own C++ runtime (libc++abi) and allocator and exports them from the executable. PRT is therefore
// loaded with RTLD_DEEPBIND through the UnrealGeometryEncoder library, which links all PRT libraries. This makes PRT
// resolve C++ runtime symbols (e.g. __dynamic_cast, __cxa_throw) from libstdc++ instead of Unreal, while the encoder
// forwards operator new/delete to the allocator used by the rest of the process (see OperatorNewForwarding.cpp).
// Objects passed to PRT are wrapped by adapters from the encoder library (see PrtAdapters.h).
namespace PrtLinuxLoader
{
// Loads PRT through the encoder library. PRT stays loaded until the process exits. OutError is also set to a warning
// if loading succeeded but the environment is likely to cause problems.
bool Load(const std::string& EncoderLibraryPath, const std::string& CoreLibraryPath, std::string& OutError);
} // namespace PrtLinuxLoader

#endif
