# Build notes for SPVersion

## Modern SP core build

The SP prototype no longer requires the original proprietary/obsolete SDK set just to compile and validate its new game logic.

Requirements:

- Windows 10/11
- Visual Studio 2022 Build Tools or Visual Studio 2022 with **Desktop development with C++**
- CMake 3.24+

Build from the repository root:

```bat
build_sp.cmd
```

That command configures a 32-bit MSVC build, compiles the SP core and runs the smoke test.

The modern target currently compiles these systems directly from the Zero Hour fork:

- `SporeVoxelWorld` — voxel terrain/resource simulation
- `SporeCreature` — modular creature stats and locomotion
- `SporeEvolution` — visceroid-to-organ evolution progression

The target intentionally bypasses the original `PreRTS.h` dependency with a standalone shim, so it does **not** require STLport, Miles, Bink, GameSpy, SafeDisk, 3DS Max 4 SDK, NVASM or BYTEmark.

CI also builds and tests this target on `windows-latest` through `.github/workflows/sp-modern-build.yml`.

## Legacy Zero Hour engine

The original engine remains in `GeneralsMD/Code/RTS.dsw`. Its historical build expects a number of obsolete SDKs. Those are no longer prerequisites for working on or validating the SP core.

The migration strategy for the full executable is to remove those legacy dependencies subsystem by subsystem instead of asking contributors to locate them:

- Direct3D: use the modern Windows SDK / replacement renderer integration;
- STLport: migrate to the standard MSVC C++ library;
- 3DS Max SDK, NVASM and BYTEmark: exclude old content/tool/benchmark projects from the runtime build;
- Miles / Asimp3: replace with a modern audio backend;
- Bink: remove proprietary video dependency from the SP runtime;
- SafeDisk: remove obsolete DRM hooks;
- GameSpy: remove legacy online services from this single-player fork;
- ZLib/LZH: replace through maintained source/package equivalents where archive compatibility is required.

## Runtime integration status

The SP core now has an independent modern build and test path. The next engine-migration work is connecting that validated core to Zero Hour's W3D rendering, input, camera and object lifecycle while keeping the modern build free of the old proprietary SDK requirements.
