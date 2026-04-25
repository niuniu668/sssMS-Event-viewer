@echo off
setlocal

set APP_DIR=%~dp0dist\QtWaveformViewer
if not exist "%APP_DIR%\QtWaveformViewer.exe" (
  echo [ERROR] Cannot find executable: "%APP_DIR%\QtWaveformViewer.exe"
  pause
  exit /b 1
)

cd /d "%APP_DIR%"
echo Launching: %CD%\QtWaveformViewer.exe
QtWaveformViewer.exe
echo.
echo Process exited with code: %ERRORLEVEL%
pause
