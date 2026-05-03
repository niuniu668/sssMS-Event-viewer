@echo off
setlocal

set "PROJECT_DIR=%~dp0"
call "%PROJECT_DIR%scripts\build_env.bat"

if not exist "%QTDIR%\bin\qmake.exe" (
  echo [ERROR] qmake not found: %QTDIR%\bin\qmake.exe
  echo Please update QTDIR in build_release.bat.
  exit /b 1
)

if not exist "%MINGW_DIR%\mingw32-make.exe" (
  echo [ERROR] mingw32-make not found: %MINGW_DIR%\mingw32-make.exe
  echo Please update MINGW_DIR in build_release.bat.
  exit /b 1
)

set "PATH=%MINGW_DIR%;%QTDIR%\bin;%PATH%"
cd /d "%PROJECT_DIR%"

for /f "tokens=2 delims=," %%a in ('tasklist /fi "imagename eq QtWaveformViewer.exe" /fo csv /nh 2^>nul') do (
  if not "%%~a"=="" taskkill /f /pid %%~a >nul 2>nul
)

echo [1/3] qmake...
"%QTDIR%\bin\qmake.exe" qt_waveform_viewer.pro
if errorlevel 1 (
  echo [ERROR] qmake failed.
  exit /b 1
)

echo [2/3] clean...
"%MINGW_DIR%\mingw32-make.exe" -f Makefile.Release clean
if errorlevel 1 (
  echo [WARN] clean step had errors. Continue...
)

echo [3/3] build release...
"%MINGW_DIR%\mingw32-make.exe" -f Makefile.Release -j4
if errorlevel 1 (
  echo [ERROR] build failed.
  exit /b 1
)

echo [OK] Build succeeded: %PROJECT_DIR%release\QtWaveformViewer.exe
exit /b 0
