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

#include "Codec/Encoder/IUnrealCallbacks.h"

#include "prt/LogHandler.h"

// On Linux, Unreal objects passed to PRT are wrapped by adapters created in this library. PRT uses dynamic_cast on the
// objects it receives, which needs type information from the same C++ runtime as PRT (libstdc++), while Unreal objects
// are compiled against libc++ without RTTI. The adapters forward all calls to the wrapped objects.
#define VITRUVIO_CREATE_CALLBACKS_ADAPTER "vitruvioCreateCallbacksAdapter"
#define VITRUVIO_DESTROY_CALLBACKS_ADAPTER "vitruvioDestroyCallbacksAdapter"
#define VITRUVIO_CREATE_LOG_HANDLER_ADAPTER "vitruvioCreateLogHandlerAdapter"
#define VITRUVIO_DESTROY_LOG_HANDLER_ADAPTER "vitruvioDestroyLogHandlerAdapter"

using CreateCallbacksAdapterFunction = IUnrealCallbacks* (*)(IUnrealCallbacks* callbacks);
using DestroyCallbacksAdapterFunction = void (*)(IUnrealCallbacks* adapter);
using CreateLogHandlerAdapterFunction = prt::LogHandler* (*)(prt::LogHandler* logHandler);
using DestroyLogHandlerAdapterFunction = void (*)(prt::LogHandler* adapter);
