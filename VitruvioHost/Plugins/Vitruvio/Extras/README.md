# Building UnrealGeometryEncoder

UnrealGeometryEncoder is a PRT extension DLL. Building it requires **no Unreal installation or custom engine**. PRT itself is downloaded precompiled, not built from source.

## Prerequisites

- Windows x64 supported by the [CityEngine SDK](https://github.com/Esri/cityengine-sdk#general-software-requirements).
- Visual Studio 2022 with **Desktop development with C++**, **MSVC v143 14.44** (Individual components), and a Windows SDK.
- CMake 3.27 or later on PATH.
- Internet access to GitHub for the first SDK download.

## Build and install

Open a **Developer Command Prompt for VS 2022**, select the matching x64 compiler, and change to this `Extras` directory:

```bat
call "%VSINSTALLDIR%VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=14.44
cmake -S UnrealGeometryEncoder -B UnrealGeometryEncoder\Build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build UnrealGeometryEncoder\Build
```

CMake downloads the official SDK into `UnrealGeometryEncoder\Build\_deps`, verifies its SHA-256, and reuses that cache on subsequent builds. It does not use or modify the plugin's installed SDK. `PRT.Build.cs` independently downloads and verifies the plugin's copy against the same SHA-256 before extraction.

Close Unreal before installing the rebuilt encoder:

```bat
cmake --install UnrealGeometryEncoder\Build
```

Installation updates `Source\ThirdParty\UnrealGeometryEncoderLib` with the DLL, import LIB, EXP, PDB, and all public headers. Building alone does not overwrite the bundled encoder. To inspect a staged installation first:

```bat
cmake --install UnrealGeometryEncoder\Build --prefix "C:\temp\UnrealGeometryEncoder"
```

## Linux

The Linux library is built from the same CMake project inside a Rocky Linux 8 container, so it uses the same compiler family and C++ standard library as the Linux PRT SDK (RHEL 8, GCC 14). This only requires Docker. From this `Extras` directory:

```sh
docker build --output type=local,dest=../Source/ThirdParty/UnrealGeometryEncoderLib/lib/Linux/Release UnrealGeometryEncoder
```

This replaces `libUnrealGeometryEncoder.so`. The public headers are shared with Windows and are installed by the Windows build.
