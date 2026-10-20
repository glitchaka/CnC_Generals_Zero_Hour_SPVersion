@echo off
setlocal
cd /d "%~dp0"

where cmake >nul 2>nul
if errorlevel 1 (
  echo ERROR: CMake no esta instalado o no esta en PATH.
  exit /b 1
)

if not exist "ExternalAssets\KayKitForest\Assets\obj\forest_texture.png" goto fetch_full_packs
if not exist "ExternalAssets\KayKitMedieval\Textures\hexagons_medieval.png" goto fetch_full_packs
if not exist "ExternalAssets\KayKitDungeonFull\Assets\textures\dungeon_texture.png" goto fetch_full_packs
if not exist "ExternalAssets\KayKitAdventurersFull\Assets\obj\sword_1handed.obj" goto fetch_full_packs
goto full_packs_ready

:fetch_full_packs
echo.
echo Preparando los cuatro packs completos para el mapa principal...
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\fetch_kaykit_full_packs.ps1"
if errorlevel 1 exit /b 1

:full_packs_ready
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\prepare_runtime_assets_from_full.ps1"
if errorlevel 1 exit /b 1

if not exist "ExternalAssets\PolyHaven\manifest.csv" (
  echo.
  echo Descargando texturas CC0 del terreno...
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\fetch_cc0_assets.ps1"
  if errorlevel 1 exit /b 1
)

if not exist "ExternalAssets\KenneyUI\panel_blue.png" (
  echo.
  echo Preparando interfaz Kenney UI...
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\fetch_kenney_ui.ps1"
  if errorlevel 1 exit /b 1
)

tasklist /FI "IMAGENAME eq sp_demo.exe" 2>nul | find /I "sp_demo.exe" >nul
if not errorlevel 1 taskkill /F /IM sp_demo.exe >nul 2>nul

if exist "build-sp\CMakeCache.txt" rmdir /S /Q "build-sp"
cmake -S SPBuild -B build-sp -A x64
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\patch_sp_release.ps1" -SourcePath "%~dp0build-sp\demo_d3d11_human_release.cpp"
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\patch_animation_release.ps1" -SourcePath "%~dp0build-sp\demo_d3d11_human_release.cpp"
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "Remove-Item Alias:R -ErrorAction SilentlyContinue; & '%~dp0tools\patch_world_release.ps1' -SourcePath '%~dp0build-sp\demo_d3d11_human_release.cpp'"
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\patch_map_v2_release.ps1" -SourcePath "%~dp0build-sp\demo_d3d11_human_release.cpp"
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\patch_map_v3_polish.ps1" -SourcePath "%~dp0build-sp\demo_d3d11_human_release.cpp"
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\patch_map_v4_asset_roles.ps1" -SourcePath "%~dp0build-sp\demo_d3d11_human_release.cpp"
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\patch_map_v5_bridges.ps1" -SourcePath "%~dp0build-sp\demo_d3d11_human_release.cpp"
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\patch_map_v6_density.ps1" -SourcePath "%~dp0build-sp\demo_d3d11_human_release.cpp"
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\patch_map_v7_gameplay.ps1" -SourcePath "%~dp0build-sp\demo_d3d11_human_release.cpp"
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\patch_map_v8_lane_surface.ps1" -SourcePath "%~dp0build-sp\demo_d3d11_human_release.cpp"
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\patch_map_v9_boundary.ps1" -SourcePath "%~dp0build-sp\demo_d3d11_human_release.cpp"
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\patch_map_v10_hex_floor.ps1" -SourcePath "%~dp0build-sp\demo_d3d11_human_release.cpp"
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\patch_map_v11_distribution.ps1" -SourcePath "%~dp0build-sp\demo_d3d11_human_release.cpp"
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\patch_map_v11_finish.ps1" -SourcePath "%~dp0build-sp\demo_d3d11_human_release.cpp"
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\patch_map_v12_requested_fixes.ps1" -SourcePath "%~dp0build-sp\demo_d3d11_human_release.cpp"
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\patch_map_v13_final_map.ps1" -SourcePath "%~dp0build-sp\demo_d3d11_human_release.cpp"
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\patch_map_v14_runtime_budget.ps1" -SourcePath "%~dp0build-sp\demo_d3d11_human_release.cpp"
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\patch_map_v15_inside_map.ps1" -SourcePath "%~dp0build-sp\demo_d3d11_human_release.cpp"
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\patch_map_v16_visibility_performance.ps1" -SourcePath "%~dp0build-sp\demo_d3d11_human_release.cpp"
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\patch_map_v17_scene_runtime.ps1" -SourcePath "%~dp0build-sp\demo_d3d11_human_release.cpp"
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\patch_equipment_release.ps1" -SourcePath "%~dp0build-sp\demo_d3d11_human_release.cpp"
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\patch_respawn_release.ps1" -SourcePath "%~dp0build-sp\demo_d3d11_human_release.cpp"
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\validate_release_source.ps1" -SourcePath "%~dp0build-sp\demo_d3d11_human_release.cpp"
if errorlevel 1 exit /b 1

cmake --build build-sp --config Release --parallel
if errorlevel 1 exit /b 1
ctest --test-dir build-sp -C Release --output-on-failure
if errorlevel 1 exit /b 1

set "BUILD_ASSETS=%~dp0build-sp\Release\ExternalAssets"
if exist "%BUILD_ASSETS%" rmdir /S /Q "%BUILD_ASSETS%"
mkdir "%BUILD_ASSETS%"
for %%P in (KayKitDungeon KayKitAdventurers KayKitDungeonFull KayKitForest KayKitMedieval KayKitAdventurersFull KenneyUI PolyHaven) do (
  xcopy /E /I /Y /Q "ExternalAssets\%%P" "%BUILD_ASSETS%\%%P" >nul
  if errorlevel 1 exit /b 1
)

set "DIST=%~dp0dist\SPVersion-demo"
if exist "%DIST%" rmdir /S /Q "%DIST%"
mkdir "%DIST%"
mkdir "%DIST%\ExternalAssets"
copy /Y "build-sp\Release\sp_demo.exe" "%DIST%\SPVersion.exe" >nul
copy /Y "SPBuild\DEMO_README.txt" "%DIST%\README.txt" >nul
for %%P in (KayKitDungeon KayKitAdventurers KayKitDungeonFull KayKitForest KayKitMedieval KayKitAdventurersFull KenneyUI PolyHaven) do (
  xcopy /E /I /Y /Q "ExternalAssets\%%P" "%DIST%\ExternalAssets\%%P" >nul
  if errorlevel 1 exit /b 1
)

if exist "dist\SPVersion-demo.zip" del /Q "dist\SPVersion-demo.zip"
powershell -NoProfile -ExecutionPolicy Bypass -Command "Compress-Archive -Path '%DIST%\*' -DestinationPath '%~dp0dist\SPVersion-demo.zip' -CompressionLevel Optimal -Force"
if errorlevel 1 exit /b 1

echo.
echo SPVersion x64: mapa principal MOBA compilado, validado y empaquetado.
echo Ejecutable: dist\SPVersion-demo\SPVersion.exe
echo Paquete:    dist\SPVersion-demo.zip
endlocal