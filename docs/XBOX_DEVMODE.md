# Xbox Dev Mode UWP

This project includes an x64 UWP build for Xbox Dev Mode. Game media is never
included in the package.

The required UWP PsyCross source is included under `external/PsyCross`; a
separate PsyCross checkout is not required for this build.

## Requirements

- Visual Studio 2022 with **Universal Windows Platform development** and the
  Windows 10 SDK 10.0.19041 or newer.
- [WorleyDL UWP dependencies](https://github.com/worleydl/uwp-dep), including
  the x64 SDL2, Mesa OpenGL, Gallium, OpenAL, DXIL, and zlib runtime files.
- OpenAL headers containing `al.h`, `alc.h`, `alext.h`, and `efx.h`.
- UWP dynamic FFmpeg libraries, headers, and runtime DLLs. They can be built
  with:

  ```powershell
  vcpkg install --classic --triplet x64-uwp ffmpeg[avcodec,avformat,swresample,swscale]
  ```

## Build

Run the following from the repository root, replacing each dependency path
with its location on your system:

```powershell
cmake --preset xbox-devmode-uwp `
  -DSF_UWP_DEPENDENCY_ROOT='D:/ports/uwp-deps' `
  -DSF_UWP_OPENAL_INCLUDE_ROOT='D:/deps/openal/include' `
  -DSF_UWP_FFMPEG_INCLUDE_ROOT='D:/deps/ffmpeg/include' `
  -DSF_UWP_FFMPEG_LIBRARY_ROOT='D:/deps/ffmpeg/lib' `
  -DSF_UWP_FFMPEG_RUNTIME_ROOT='D:/deps/ffmpeg/bin'
cmake --build --preset xbox-devmode-uwp-release
```

The AppX package is written under
`build/xbox-devmode-uwp/AppPackages/syphon_filter/`.

## Install on Xbox Dev Mode

Sign the generated package, then create an AppX copy for Xbox Device Portal:

```powershell
$package = Get-ChildItem .\build\xbox-devmode-uwp\AppPackages\syphon_filter -Recurse -Filter *.msix |
  Select-Object -First 1
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\sign_xbox_msix.ps1 `
  -PackagePath $package.FullName
Copy-Item $package.FullName ([System.IO.Path]::ChangeExtension($package.FullName, '.appx'))
```

## Game media

The game accepts a legally dumped Syphon Filter USA v1.1 (`SCUS-94240`)
single-BIN/CUE image. On first launch, select the folder containing the CUE
and BIN. The game imports both files into its private app storage.

For an internal transfer, deploy and launch the app once to create its
LocalState folders. In Xbox Device Portal, open **File Explorer**, select **LocalAppData**,
choose the `SyphonFilterUWP` package, and open `LocalState/Game`.
Upload your BIN and CUE game files into `LocalState/Game`, and the app will
automatically prepare them for you.

The source contains no game files, dependency packages, certificates, or built
AppX artifacts.
