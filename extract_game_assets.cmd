@echo off
setlocal
set SCRIPT=%~dp0tools\extract_zh_assets.ps1
if not exist "%SCRIPT%" (
  echo ERROR: extractor not found: %SCRIPT%
  exit /b 1
)
if "%~1"=="" (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT%"
) else (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT%" -GamePath "%~1"
)
exit /b %errorlevel%
