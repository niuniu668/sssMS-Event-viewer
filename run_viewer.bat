@echo off
setlocal

set "REPO_DIR=%~dp0"
call "%REPO_DIR%scripts\build_env.bat"

rem Prefer release build if present, otherwise use deployed dist
if exist "%REPO_DIR%release\QtWaveformViewer.exe" (
  set "APP_EXE=%REPO_DIR%release\QtWaveformViewer.exe"
) else if exist "%REPO_DIR%dist\QtWaveformViewer\QtWaveformViewer.exe" (
  set "APP_EXE=%REPO_DIR%dist\QtWaveformViewer\QtWaveformViewer.exe"
) else (
  echo [ERROR] Cannot find QtWaveformViewer.exe in release/ or dist/QtWaveformViewer/
  pause
  exit /b 1
)

rem Ensure runtime paths prefer pinned MinGW/Qt
set "PATH=%MINGW_DIR%;%QTDIR%\bin;%PATH%"

echo Launching: %APP_EXE%
cd /d "%~dp0"
"%APP_EXE%"
echo.
echo Process exited with code: %ERRORLEVEL%
pause
