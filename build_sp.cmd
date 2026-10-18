@echo off
setlocal

where cmake >nul 2>nul
if errorlevel 1 (
  echo ERROR: CMake no esta instalado o no esta en PATH.
  exit /b 1
)

cmake -S SPBuild -B build-sp -A Win32
if errorlevel 1 exit /b 1

cmake --build build-sp --config Release --parallel
if errorlevel 1 exit /b 1

ctest --test-dir build-sp -C Release --output-on-failure
if errorlevel 1 exit /b 1

echo.
echo SP core compilado y validado correctamente.
endlocal
