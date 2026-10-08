# Copyright 2026 Esri
#
# Licensed under the Apache License Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Builds libUnrealGeometryEncoder.so with the same toolchain family as the Linux PRT SDK (RHEL 8, GCC 14).
# The build context is the plugin directory (see Dockerfile.dockerignore). From the Extras directory:
#   docker build --output type=local,dest=../Source/ThirdParty/UnrealGeometryEncoderLib/lib/Linux/Release -f UnrealGeometryEncoder/Dockerfile ..
FROM rockylinux:8 AS build
RUN dnf -y install gcc-toolset-14-gcc-c++ make && dnf clean all
ARG CMAKE_VERSION=3.31.6
ARG CMAKE_SHA256=5a1133ff103c71eb5120e2cc3de922733e7d8a26a98ae716397e8676adb367bf
RUN curl -fsSL -o /tmp/cmake.tar.gz https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz \
	&& echo "${CMAKE_SHA256}  /tmp/cmake.tar.gz" | sha256sum -c - \
	&& tar xzf /tmp/cmake.tar.gz -C /opt && rm /tmp/cmake.tar.gz
ENV PATH=/opt/cmake-${CMAKE_VERSION}-linux-x86_64/bin:$PATH

COPY Source/ThirdParty/PRT/PRT.version.json /plugin/Source/ThirdParty/PRT/PRT.version.json
COPY Extras/UnrealGeometryEncoder /plugin/Extras/UnrealGeometryEncoder
RUN . /opt/rh/gcc-toolset-14/enable \
	&& cmake -S /plugin/Extras/UnrealGeometryEncoder -B /build -DCMAKE_BUILD_TYPE=Release \
	&& cmake --build /build -j \
	&& cmake --install /build --prefix /out

FROM scratch
COPY --from=build /out/lib/Linux/Release/libUnrealGeometryEncoder.so /
