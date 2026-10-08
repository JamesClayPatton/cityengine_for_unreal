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

#if defined(__linux__)

#include "Codec/Encoder/PrtAdapters.h"

#include "Codec/CodecMain.h"

namespace
{

class CallbacksAdapter final : public IUnrealCallbacks
{
public:
	explicit CallbacksAdapter(IUnrealCallbacks* target) : mTarget(target) {}

	// clang-format off
	void addMesh(const wchar_t* name, const wchar_t* meshId, int32_t prototypeId, const wchar_t* uri,
	             const double* vtx, size_t vtxSize, const double* nrm, size_t nrmSize,
	             const uint32_t* faceVertexCounts, size_t faceVertexCountsSize,
	             const uint32_t* vertexIndices, size_t vertexIndicesSize,
	             const uint32_t* normalIndices, size_t normalIndicesSize,
	             double const* const* uvs, size_t const* uvsSizes,
	             uint32_t const* const* uvCounts, size_t const* uvCountsSizes,
	             uint32_t const* const* uvIndices, size_t const* uvIndicesSizes,
	             size_t uvSets, const uint32_t* faceRanges, size_t faceRangesSize,
	             const prt::AttributeMap** materials) override
	{
		mTarget->addMesh(name, meshId, prototypeId, uri, vtx, vtxSize, nrm, nrmSize, faceVertexCounts, faceVertexCountsSize,
		                 vertexIndices, vertexIndicesSize, normalIndices, normalIndicesSize, uvs, uvsSizes, uvCounts, uvCountsSizes,
		                 uvIndices, uvIndicesSizes, uvSets, faceRanges, faceRangesSize, materials);
	}
	// clang-format on

	void addInstance(int32_t prototypeId, const wchar_t* meshId, const double* transform, const prt::AttributeMap** instanceMaterial,
	                 size_t numInstanceMaterials) override
	{
		mTarget->addInstance(prototypeId, meshId, transform, instanceMaterial, numInstanceMaterials);
	}

	void init() override { mTarget->init(); }
	void finish() override { mTarget->finish(); }
	void addReport(const prt::AttributeMap* reports) override { mTarget->addReport(reports); }

	prt::Status generateError(size_t isIndex, prt::Status status, const wchar_t* message) override
	{
		return mTarget->generateError(isIndex, status, message);
	}

	prt::Status assetError(size_t isIndex, prt::CGAErrorLevel level, const wchar_t* key, const wchar_t* uri, const wchar_t* message) override
	{
		return mTarget->assetError(isIndex, level, key, uri, message);
	}

	prt::Status cgaError(size_t isIndex, int32_t shapeID, prt::CGAErrorLevel level, int32_t methodId, int32_t pc, const wchar_t* message) override
	{
		return mTarget->cgaError(isIndex, shapeID, level, methodId, pc, message);
	}

	prt::Status cgaPrint(size_t isIndex, int32_t shapeID, const wchar_t* txt) override { return mTarget->cgaPrint(isIndex, shapeID, txt); }

	prt::Status cgaReportBool(size_t isIndex, int32_t shapeID, const wchar_t* key, bool value) override
	{
		return mTarget->cgaReportBool(isIndex, shapeID, key, value);
	}

	prt::Status cgaReportFloat(size_t isIndex, int32_t shapeID, const wchar_t* key, double value) override
	{
		return mTarget->cgaReportFloat(isIndex, shapeID, key, value);
	}

	prt::Status cgaReportString(size_t isIndex, int32_t shapeID, const wchar_t* key, const wchar_t* value) override
	{
		return mTarget->cgaReportString(isIndex, shapeID, key, value);
	}

	prt::Status attrBool(size_t isIndex, int32_t shapeID, const wchar_t* key, bool value) override
	{
		return mTarget->attrBool(isIndex, shapeID, key, value);
	}

	prt::Status attrFloat(size_t isIndex, int32_t shapeID, const wchar_t* key, double value) override
	{
		return mTarget->attrFloat(isIndex, shapeID, key, value);
	}

	prt::Status attrString(size_t isIndex, int32_t shapeID, const wchar_t* key, const wchar_t* value) override
	{
		return mTarget->attrString(isIndex, shapeID, key, value);
	}

	prt::Status attrBoolArray(size_t isIndex, int32_t shapeID, const wchar_t* key, const bool* ptr, size_t size, size_t nRows) override
	{
		return mTarget->attrBoolArray(isIndex, shapeID, key, ptr, size, nRows);
	}

	prt::Status attrFloatArray(size_t isIndex, int32_t shapeID, const wchar_t* key, const double* ptr, size_t size, size_t nRows) override
	{
		return mTarget->attrFloatArray(isIndex, shapeID, key, ptr, size, nRows);
	}

	prt::Status attrStringArray(size_t isIndex, int32_t shapeID, const wchar_t* key, const wchar_t* const* ptr, size_t size, size_t nRows) override
	{
		return mTarget->attrStringArray(isIndex, shapeID, key, ptr, size, nRows);
	}

	double cgaGetCoord(size_t isIndex, CoordSelector sel, double x, double y, double z, prt::Status* stat) override
	{
		return mTarget->cgaGetCoord(isIndex, sel, x, y, z, stat);
	}

	Continuation progress(float percentageCompleted) override { return mTarget->progress(percentageCompleted); }

private:
	IUnrealCallbacks* mTarget;
};

class LogHandlerAdapter final : public prt::LogHandler
{
public:
	explicit LogHandlerAdapter(prt::LogHandler* target) : mTarget(target) {}

	void handleLogEvent(const wchar_t* msg, prt::LogLevel level) override { mTarget->handleLogEvent(msg, level); }
	const prt::LogLevel* getLevels(size_t* count) override { return mTarget->getLevels(count); }
	void getFormat(bool* dateTime, bool* level) override { mTarget->getFormat(dateTime, level); }

private:
	prt::LogHandler* mTarget;
};

} // namespace

extern "C"
{
	CODEC_EXPORTS_API IUnrealCallbacks* vitruvioCreateCallbacksAdapter(IUnrealCallbacks* callbacks)
	{
		return new CallbacksAdapter(callbacks);
	}

	CODEC_EXPORTS_API void vitruvioDestroyCallbacksAdapter(IUnrealCallbacks* adapter)
	{
		delete adapter;
	}

	CODEC_EXPORTS_API prt::LogHandler* vitruvioCreateLogHandlerAdapter(prt::LogHandler* logHandler)
	{
		return new LogHandlerAdapter(logHandler);
	}

	CODEC_EXPORTS_API void vitruvioDestroyLogHandlerAdapter(prt::LogHandler* adapter)
	{
		delete static_cast<LogHandlerAdapter*>(adapter);
	}
}

#endif
