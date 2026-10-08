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

#include "PrtLinuxLoader.h"

#if PLATFORM_LINUX

#include "prt/API.h"
#include "prt/AttributeMap.h"
#include "prt/Cache.h"
#include "prt/Callbacks.h"
#include "prt/InitialShape.h"
#include "prt/LogHandler.h"
#include "prt/OcclusionSet.h"
#include "prt/StringUtils.h"

#include "Codec/Encoder/PrtAdapters.h"

#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <locale.h>

namespace PrtLinuxLoader::Private
{
void* EncoderHandle = nullptr;
void* CoreHandle = nullptr;

// Unreal sets LC_NUMERIC to "en_US", which is not installed on many systems. PRT creates std::locale("") (through
// boost::filesystem), which fails if a locale environment variable names a missing locale, so these are reset to "C".
void SanitizeLocaleEnvironment()
{
	if (locale_t Locale = newlocale(LC_ALL_MASK, "", nullptr))
	{
		freelocale(Locale);
		return;
	}

	for (const char* Name : {"LC_ALL", "LC_CTYPE", "LC_NUMERIC", "LC_TIME", "LC_COLLATE", "LC_MONETARY", "LC_MESSAGES", "LC_PAPER",
							 "LC_NAME", "LC_ADDRESS", "LC_TELEPHONE", "LC_MEASUREMENT", "LC_IDENTIFICATION", "LANG"})
	{
		const char* Value = std::getenv(Name);
		if (Value == nullptr || *Value == '\0')
		{
			continue;
		}
		if (locale_t Locale = newlocale(LC_ALL_MASK, Value, nullptr))
		{
			freelocale(Locale);
			continue;
		}
		setenv(Name, "C", 1);
	}
}

template <typename TFunction>
TFunction Resolve(void* Handle, const char* Name)
{
	void* Symbol = Handle != nullptr ? dlsym(Handle, Name) : nullptr;
	if (Symbol == nullptr)
	{
		std::fprintf(stderr, "PRT symbol %s is not available (PRT not loaded or version mismatch)\n", Name);
		std::abort();
	}
	return reinterpret_cast<TFunction>(Symbol);
}

template <typename TFunction>
TFunction Resolve(const char* MangledName)
{
	return Resolve<TFunction>(CoreHandle, MangledName);
}

// The adapter functions are exported by the UnrealGeometryEncoder library.
template <typename TFunction>
TFunction ResolveAdapter(const char* Name)
{
	return Resolve<TFunction>(EncoderHandle, Name);
}

// Wraps Unreal callbacks for the duration of a (synchronous) generate call.
class FCallbacksAdapterScope
{
public:
	explicit FCallbacksAdapterScope(prt::Callbacks* Callbacks)
	{
		static const auto Create = ResolveAdapter<CreateCallbacksAdapterFunction>(VITRUVIO_CREATE_CALLBACKS_ADAPTER);
		// Vitruvio only passes IUnrealCallbacks implementations to PRT
		Adapter = Callbacks != nullptr ? Create(static_cast<IUnrealCallbacks*>(Callbacks)) : nullptr;
	}

	~FCallbacksAdapterScope()
	{
		static const auto Destroy = ResolveAdapter<DestroyCallbacksAdapterFunction>(VITRUVIO_DESTROY_CALLBACKS_ADAPTER);
		if (Adapter != nullptr)
		{
			Destroy(Adapter);
		}
	}

	prt::Callbacks* Get() const
	{
		return Adapter;
	}

private:
	IUnrealCallbacks* Adapter;
};
} // namespace PrtLinuxLoader::Private

namespace PrtLinuxLoader
{
bool Load(const std::string& EncoderLibraryPath, const std::string& CoreLibraryPath, std::string& OutError)
{
	if (Private::CoreHandle != nullptr)
	{
		return true;
	}

	Private::SanitizeLocaleEnvironment();

	// If libstdc++ was already loaded by another library, it resolved parts of the C++ runtime against Unreal and
	// PRT will most likely fail (e.g. exceptions during initialization). Loading continues but reports a warning.
	if (void* SharedStdCpp = dlopen("libstdc++.so.6", RTLD_LAZY | RTLD_NOLOAD))
	{
		dlclose(SharedStdCpp);
		OutError = "libstdc++ was loaded before PRT by another library (e.g. the NNERuntimeORT plugin or a Mesa graphics driver), "
				   "PRT may not work correctly";
	}

	// The encoder links all PRT libraries, which are therefore loaded with the encoder as the root of their lookup scope
	Private::EncoderHandle = dlopen(EncoderLibraryPath.c_str(), RTLD_NOW | RTLD_LOCAL | RTLD_DEEPBIND);
	if (Private::EncoderHandle == nullptr)
	{
		OutError = dlerror();
		return false;
	}

	Private::CoreHandle = dlopen(CoreLibraryPath.c_str(), RTLD_NOW | RTLD_LOCAL | RTLD_NOLOAD);
	if (Private::CoreHandle == nullptr)
	{
		OutError = dlerror();
		return false;
	}
	return true;
}
} // namespace PrtLinuxLoader

// Definitions of the PRT functions used by Vitruvio, forwarding to the dynamically loaded PRT core library.
namespace prt
{
const Object* init(const wchar_t* const* prtPlugins, size_t prtPluginsCount, LogLevel logLevel, Status* stat)
{
	static const auto Function =
		PrtLinuxLoader::Private::Resolve<const Object* (*)(const wchar_t* const*, size_t, LogLevel, Status*)>("_ZN3prt4initEPKPKwmNS_8LogLevelEPNS_6StatusE");
	return Function(prtPlugins, prtPluginsCount, logLevel, stat);
}

Status generate(const InitialShape* const* initialShapes, size_t initialShapeCount, const OcclusionSet::Handle* occlusionHandles,
				const wchar_t* const* encoders, size_t encodersCount, const AttributeMap* const* encoderOptions, Callbacks* callbacks,
				Cache* cache, const OcclusionSet* occlSet, const AttributeMap* generateOptions)
{
	static const auto Function = PrtLinuxLoader::Private::Resolve<Status (*)(const InitialShape* const*, size_t, const OcclusionSet::Handle*, const wchar_t* const*, size_t,
													const AttributeMap* const*, Callbacks*, Cache*, const OcclusionSet*, const AttributeMap*)>(
		"_ZN3prt8generateEPKPKNS_12InitialShapeEmPKmPKPKwmPKPKNS_12AttributeMapEPNS_9CallbacksEPNS_5CacheEPKNS_12OcclusionSetESD_");
	PrtLinuxLoader::Private::FCallbacksAdapterScope Adapter(callbacks);
	return Function(initialShapes, initialShapeCount, occlusionHandles, encoders, encodersCount, encoderOptions, Adapter.Get(), cache, occlSet,
					generateOptions);
}

Status generateOccluders(const InitialShape* const* initialShapes, size_t initialShapeCount, OcclusionSet::Handle* occlusionHandles,
						 const wchar_t* const* encoders, size_t encodersCount, const AttributeMap* const* encoderOptions, Callbacks* callbacks,
						 Cache* cache, OcclusionSet* occlSet, const AttributeMap* generateOptions)
{
	static const auto Function = PrtLinuxLoader::Private::Resolve<Status (*)(const InitialShape* const*, size_t, OcclusionSet::Handle*, const wchar_t* const*, size_t,
													const AttributeMap* const*, Callbacks*, Cache*, OcclusionSet*, const AttributeMap*)>(
		"_ZN3prt17generateOccludersEPKPKNS_12InitialShapeEmPmPKPKwmPKPKNS_12AttributeMapEPNS_9CallbacksEPNS_5CacheEPNS_12OcclusionSetESC_");
	PrtLinuxLoader::Private::FCallbacksAdapterScope Adapter(callbacks);
	return Function(initialShapes, initialShapeCount, occlusionHandles, encoders, encodersCount, encoderOptions, Adapter.Get(), cache, occlSet,
					generateOptions);
}

const ResolveMap* createResolveMap(const wchar_t* rpkOrResURI, const wchar_t* unpackFileSystemPath, Status* stat)
{
	static const auto Function =
		PrtLinuxLoader::Private::Resolve<const ResolveMap* (*)(const wchar_t*, const wchar_t*, Status*)>("_ZN3prt16createResolveMapEPKwS1_PNS_6StatusE");
	return Function(rpkOrResURI, unpackFileSystemPath, stat);
}

const RuleFileInfo* createRuleFileInfo(const wchar_t* ruleFileURI, Cache* cache, Status* stat)
{
	static const auto Function =
		PrtLinuxLoader::Private::Resolve<const RuleFileInfo* (*)(const wchar_t*, Cache*, Status*)>("_ZN3prt18createRuleFileInfoEPKwPNS_5CacheEPNS_6StatusE");
	return Function(ruleFileURI, cache, stat);
}

const EncoderInfo* createEncoderInfo(const wchar_t* encoderId, Status* stat)
{
	static const auto Function = PrtLinuxLoader::Private::Resolve<const EncoderInfo* (*)(const wchar_t*, Status*)>("_ZN3prt17createEncoderInfoEPKwPNS_6StatusE");
	return Function(encoderId, stat);
}

const AttributeMap* createTextureMetadata(const wchar_t* uri, Cache* cache, Status* stat)
{
	static const auto Function =
		PrtLinuxLoader::Private::Resolve<const AttributeMap* (*)(const wchar_t*, Cache*, Status*)>("_ZN3prt21createTextureMetadataEPKwPNS_5CacheEPNS_6StatusE");
	return Function(uri, cache, stat);
}

Status getTexturePixeldata(const wchar_t* uri, uint8_t* buffer, size_t bufferSize, Cache* cache)
{
	static const auto Function = PrtLinuxLoader::Private::Resolve<Status (*)(const wchar_t*, uint8_t*, size_t, Cache*)>("_ZN3prt19getTexturePixeldataEPKwPhmPNS_5CacheE");
	return Function(uri, buffer, bufferSize, cache);
}

const char* getStatusDescription(Status stat)
{
	static const auto Function = PrtLinuxLoader::Private::Resolve<const char* (*)(Status)>("_ZN3prt20getStatusDescriptionENS_6StatusE");
	return Function(stat);
}

Status addLogHandler(LogHandler* logHandler)
{
	static const auto Function = PrtLinuxLoader::Private::Resolve<Status (*)(LogHandler*)>("_ZN3prt13addLogHandlerEPNS_10LogHandlerE");
	static const auto CreateAdapter = PrtLinuxLoader::Private::ResolveAdapter<CreateLogHandlerAdapterFunction>(VITRUVIO_CREATE_LOG_HANDLER_ADAPTER);
	// The adapter is kept for the lifetime of the process, as PRT stays loaded until the process exits
	return Function(CreateAdapter(logHandler));
}

AttributeMapBuilder* AttributeMapBuilder::create(Status* status)
{
	static const auto Function = PrtLinuxLoader::Private::Resolve<AttributeMapBuilder* (*)(Status*)>("_ZN3prt19AttributeMapBuilder6createEPNS_6StatusE");
	return Function(status);
}

InitialShapeBuilder* InitialShapeBuilder::create(Status* status)
{
	static const auto Function = PrtLinuxLoader::Private::Resolve<InitialShapeBuilder* (*)(Status*)>("_ZN3prt19InitialShapeBuilder6createEPNS_6StatusE");
	return Function(status);
}

OcclusionSet* OcclusionSet::create(Status* stat)
{
	static const auto Function = PrtLinuxLoader::Private::Resolve<OcclusionSet* (*)(Status*)>("_ZN3prt12OcclusionSet6createEPNS_6StatusE");
	return Function(stat);
}

CacheObject* CacheObject::create(CacheType type)
{
	static const auto Function = PrtLinuxLoader::Private::Resolve<CacheObject* (*)(CacheType)>("_ZN3prt11CacheObject6createENS0_9CacheTypeE");
	return Function(type);
}

void Object::destroy() const
{
	static const auto Function = PrtLinuxLoader::Private::Resolve<void (*)(const Object*)>("_ZNK3prt6Object7destroyEv");
	Function(this);
}

// Pure interfaces without data members, so construction and destruction need no PRT code.
LogHandler::LogHandler() {}
LogHandler::~LogHandler() {}
Callbacks::Callbacks() {}
Callbacks::~Callbacks() {}

double Callbacks::cgaGetCoord(size_t isIndex, CoordSelector sel, double x, double y, double z, Status* stat)
{
	static const auto Function = PrtLinuxLoader::Private::Resolve<double (*)(Callbacks*, size_t, CoordSelector, double, double, double, Status*)>(
		"_ZN3prt9Callbacks11cgaGetCoordEmNS0_13CoordSelectorEdddPNS_6StatusE");
	return Function(this, isIndex, sel, x, y, z, stat);
}

Callbacks::Continuation Callbacks::progress(float percentageCompleted)
{
	static const auto Function = PrtLinuxLoader::Private::Resolve<Continuation (*)(Callbacks*, float)>("_ZN3prt9Callbacks8progressEf");
	return Function(this, percentageCompleted);
}

namespace StringUtils
{
wchar_t* toUTF16FromOSNarrow(const char* osString, wchar_t* result, size_t* resultSize, Status* stat)
{
	static const auto Function =
		PrtLinuxLoader::Private::Resolve<wchar_t* (*)(const char*, wchar_t*, size_t*, Status*)>("_ZN3prt11StringUtils19toUTF16FromOSNarrowEPKcPwPmPNS_6StatusE");
	return Function(osString, result, resultSize, stat);
}

wchar_t* toUTF16FromUTF8(const char* utf8String, wchar_t* result, size_t* resultSize, Status* stat)
{
	static const auto Function =
		PrtLinuxLoader::Private::Resolve<wchar_t* (*)(const char*, wchar_t*, size_t*, Status*)>("_ZN3prt11StringUtils15toUTF16FromUTF8EPKcPwPmPNS_6StatusE");
	return Function(utf8String, result, resultSize, stat);
}

char* toUTF8FromUTF16(const wchar_t* utf16String, char* result, size_t* resultSize, Status* stat)
{
	static const auto Function =
		PrtLinuxLoader::Private::Resolve<char* (*)(const wchar_t*, char*, size_t*, Status*)>("_ZN3prt11StringUtils15toUTF8FromUTF16EPKwPcPmPNS_6StatusE");
	return Function(utf16String, result, resultSize, stat);
}

char* percentEncode(const char* utf8String, char* result, size_t* resultSize, Status* stat)
{
	static const auto Function =
		PrtLinuxLoader::Private::Resolve<char* (*)(const char*, char*, size_t*, Status*)>("_ZN3prt11StringUtils13percentEncodeEPKcPcPmPNS_6StatusE");
	return Function(utf8String, result, resultSize, stat);
}
} // namespace StringUtils
} // namespace prt

#endif
