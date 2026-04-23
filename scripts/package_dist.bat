@echo off
setlocal

set SCRIPT_DIR=%~dp0
powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%package_dist.ps1" %*

if errorlevel 1 (
  echo.
  echo Packaging failed.
  exit /b 1
)

echo.
echo Packaging finished.
exit /b 0
