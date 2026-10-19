@echo off
setlocal
cd /d "%~dp0"

where cmake >nul 2>nul
if errorlevel 1 (
  echo ERROR: CMake no esta instalado o no esta en PATH.
  exit /b 1
)

if not exist "ExternalAssets\PolyHaven\manifest.csv" (
  echo.
  echo Descargando texturas CC0 del terreno...
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\fetch_cc0_assets.ps1"
  if errorlevel 1 exit /b 1
)

if not exist "ExternalAssets\KayKitDungeon\crates_stacked.obj" (
  echo.
  echo Preparando escenario KayKit Dungeon...
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\fetch_kaykit_dungeon.ps1"
  if errorlevel 1 exit /b 1
)

if not exist "ExternalAssets\KayKitAdventurers\Knight.glb" goto fetch_adventurers
if not exist "ExternalAssets\KayKitAdventurers\sword_1handed.obj" goto fetch_adventurers
if not exist "ExternalAssets\KayKitAdventurers\sword_2handed.obj" goto fetch_adventurers
if not exist "ExternalAssets\KayKitAdventurers\shield_round.obj" goto fetch_adventurers
if not exist "ExternalAssets\KayKitAdventurers\Rig_Medium_CombatMelee.glb" goto fetch_adventurers
if not exist "ExternalAssets\KayKitAdventurers\Rig_Medium_MovementAdvanced.glb" goto fetch_adventurers
goto adventurers_ready

:fetch_adventurers
echo.
echo Preparando personajes, armas y animaciones KayKit...
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\fetch_kaykit_adventurers.ps1"
if errorlevel 1 exit /b 1

:adventurers_ready
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
if not exist "ExternalAssets\KenneyUI\panel_blue.png" (
  echo.
  echo Preparando interfaz Kenney UI...
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\fetch_kenney_ui.ps1"
  if errorlevel 1 exit /b 1
)

tasklist /FI "IMAGENAME eq sp_demo.exe" 2>nul | find /I "sp_demo.exe" >nul
if not errorlevel 1 taskkill /F /IM sp_demo.exe >nul 2>nul

cmake -S SPBuild -B build-sp -A Win32
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\patch_sp_release.ps1" -SourcePath "%~dp0build-sp\demo_d3d11_human_release.cpp"
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\patch_animation_release.ps1" -SourcePath "%~dp0build-sp\demo_d3d11_human_release.cpp"
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\patch_world_release.ps1" -SourcePath "%~dp0build-sp\demo_d3d11_human_release.cpp"
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
echo SPVersion: mapa principal de cuatro packs compilado, validado y empaquetado.
echo Ejecutable: dist\SPVersion-demo\SPVersion.exe
echo Paquete:    dist\SPVersion-demo.zip
endlocal
