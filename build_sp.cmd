@echo off
setlocal

where cmake >nul 2>nul
if errorlevel 1 (
  echo ERROR: CMake no esta instalado o no esta en PATH.
  exit /b 1
)

rem sp_demo.exe cannot be relinked while a previous demo instance is still running.
tasklist /FI "IMAGENAME eq sp_demo.exe" 2>nul | find /I "sp_demo.exe" >nul
if not errorlevel 1 (
  echo Cerrando instancia anterior de sp_demo.exe...
  taskkill /F /IM sp_demo.exe >nul 2>nul
  for /L %%I in (1,1,20) do (
    tasklist /FI "IMAGENAME eq sp_demo.exe" 2>nul | find /I "sp_demo.exe" >nul
    if errorlevel 1 goto demo_unlocked
    >nul 2>&1 ping 127.0.0.1 -n 2
  )
  echo ERROR: sp_demo.exe sigue bloqueado. Cierra manualmente el proceso e intenta de nuevo.
  exit /b 1
)

:demo_unlocked
cmake -S SPBuild -B build-sp -A Win32
if errorlevel 1 exit /b 1

cmake --build build-sp --config Release --parallel
if errorlevel 1 exit /b 1

ctest --test-dir build-sp -C Release --output-on-failure
if errorlevel 1 exit /b 1

echo.
echo SP core compilado y validado correctamente.
endlocal
