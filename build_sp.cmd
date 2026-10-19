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
  if errorlevel 1 (
    echo ERROR: No pude preparar KayKit Dungeon.
    exit /b 1
  )
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
if errorlevel 1 (
  echo ERROR: No pude preparar personajes/armas/animaciones KayKit.
  exit /b 1
)

:adventurers_ready
if not exist "ExternalAssets\KenneyUI\panel_blue.png" (
  echo.
  echo Preparando interfaz Kenney UI...
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\fetch_kenney_ui.ps1"
  if errorlevel 1 (
    echo ERROR: No pude preparar Kenney UI.
    exit /b 1
  )
)

tasklist /FI "IMAGENAME eq sp_demo.exe" 2>nul | find /I "sp_demo.exe" >nul
if not errorlevel 1 (
  echo Cerrando instancia anterior de sp_demo.exe...
  taskkill /F /IM sp_demo.exe >nul 2>nul
  for /L %%I in (1,1,20) do (
    tasklist /FI "IMAGENAME eq sp_demo.exe" 2>nul | find /I "sp_demo.exe" >nul
    if errorlevel 1 goto demo_unlocked
    >nul 2>&1 ping 127.0.0.1 -n 2
  )
  echo ERROR: sp_demo.exe sigue bloqueado.
  exit /b 1
)

:demo_unlocked
cmake -S SPBuild -B build-sp -A Win32
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\patch_sp_release.ps1" -SourcePath "%~dp0build-sp\demo_d3d11_human_release.cpp"
if errorlevel 1 exit /b 1

cmake --build build-sp --config Release --parallel
if errorlevel 1 exit /b 1
ctest --test-dir build-sp -C Release --output-on-failure
if errorlevel 1 exit /b 1

set "BUILD_ASSETS=%~dp0build-sp\Release\ExternalAssets"
if exist "%BUILD_ASSETS%" rmdir /S /Q "%BUILD_ASSETS%"
mkdir "%BUILD_ASSETS%"
xcopy /E /I /Y /Q "ExternalAssets\KayKitDungeon" "%BUILD_ASSETS%\KayKitDungeon" >nul
if errorlevel 1 exit /b 1
xcopy /E /I /Y /Q "ExternalAssets\KayKitAdventurers" "%BUILD_ASSETS%\KayKitAdventurers" >nul
if errorlevel 1 exit /b 1
xcopy /E /I /Y /Q "ExternalAssets\KenneyUI" "%BUILD_ASSETS%\KenneyUI" >nul
if errorlevel 1 exit /b 1
xcopy /E /I /Y /Q "ExternalAssets\PolyHaven" "%BUILD_ASSETS%\PolyHaven" >nul
if errorlevel 1 exit /b 1

set "DIST=%~dp0dist\SPVersion-demo"
if exist "%DIST%" rmdir /S /Q "%DIST%"
mkdir "%DIST%"
mkdir "%DIST%\ExternalAssets"

copy /Y "build-sp\Release\sp_demo.exe" "%DIST%\SPVersion.exe" >nul
copy /Y "SPBuild\DEMO_README.txt" "%DIST%\README.txt" >nul

xcopy /E /I /Y /Q "ExternalAssets\KayKitDungeon" "%DIST%\ExternalAssets\KayKitDungeon" >nul
if errorlevel 1 exit /b 1
xcopy /E /I /Y /Q "ExternalAssets\KayKitAdventurers" "%DIST%\ExternalAssets\KayKitAdventurers" >nul
if errorlevel 1 exit /b 1
xcopy /E /I /Y /Q "ExternalAssets\KenneyUI" "%DIST%\ExternalAssets\KenneyUI" >nul
if errorlevel 1 exit /b 1
xcopy /E /I /Y /Q "ExternalAssets\PolyHaven" "%DIST%\ExternalAssets\PolyHaven" >nul
if errorlevel 1 exit /b 1

if exist "dist\SPVersion-demo.zip" del /Q "dist\SPVersion-demo.zip"
powershell -NoProfile -ExecutionPolicy Bypass -Command "Compress-Archive -Path '%DIST%\*' -DestinationPath '%~dp0dist\SPVersion-demo.zip' -CompressionLevel Optimal -Force"
if errorlevel 1 exit /b 1

echo.
echo SPVersion demo compilado, validado y empaquetado.
echo Ejecutable directo: build-sp\Release\sp_demo.exe
echo Ejecutable entrega: dist\SPVersion-demo\SPVersion.exe
echo Paquete:           dist\SPVersion-demo.zip
endlocal
