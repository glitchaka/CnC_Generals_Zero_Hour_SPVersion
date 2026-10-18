# Build notes for SPVersion

This fork is still based on the original Command & Conquer: Generals / Zero Hour Visual C++ 6 project layout. The Zero Hour workspace is:

`GeneralsMD/Code/RTS.dsw`

## Fastest path to a build

1. Use Windows x86 tooling.
2. Install Visual C++ 6.0 SP6, or convert `RTS.dsw` through Visual Studio .NET 2003 before opening the converted solution in a newer Visual Studio.
3. Restore/provide the legacy dependencies documented in the root `README.md` under the exact paths expected below `GeneralsMD/Code/`.
4. Put an installed Steam build of Zero Hour in `GeneralsMD/Run/` as indicated by `place_steam_build_here.txt`.
5. Open `GeneralsMD/Code/RTS.dsw`.
6. Select the Zero Hour/RTS release configuration and run **Build -> Batch Build -> Rebuild All**.
7. The generated binaries are copied to `GeneralsMD/Run/`.

## Required legacy dependencies

The original release expects at least:

- DirectX SDK 9
- STLport 4.5.3 (apply the repository `stlport.diff`)
- 3DS Max 4 SDK
- NVASM
- BYTEmark
- Miles Sound System SDK / Asimp3
- Bink SDK
- SafeDisk API
- GameSpy SDK
- ZLib 1.1.4
- LZH-Light 1.0

The repository already contains part of `Code/Libraries`, but it does not contain every proprietary SDK required by the original project.

## Important: current SP prototype status

The new SP/voxel/evolution source has been added to the fork, but the project is not yet at the point where I consider the new prototype a validated runnable build. The original workspace can only compile the new code after the new `.cpp` files are wired into the relevant Visual C++ projects and the lifecycle hooks are connected to the actual Zero Hour game loop.

Do not expect the current branch to produce the new creature prototype merely by rebuilding the untouched original workspace yet.

## Intended end state

The target is to remove the need for Visual C++ 6 as a day-to-day workflow and provide a reproducible modern Windows build. Until that migration is complete, the legacy `RTS.dsw` route above is the authoritative build path for the original engine.
