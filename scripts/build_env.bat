@echo off
rem Unified build environment for this repository.
rem Adjust the defaults below if you need a different Qt or MinGW installation.

rem Allow overrides via environment variables REPO_QTDIR and REPO_MINGW_DIR
if not defined REPO_QTDIR set "REPO_QTDIR=C:\Qt\6.7.3\mingw_64"
if not defined REPO_MINGW_DIR set "REPO_MINGW_DIR=C:\Qt\Tools\mingw1120_64\bin"

set "QTDIR=%REPO_QTDIR%"
set "MINGW_DIR=%REPO_MINGW_DIR%"

echo [env] Using QTDIR=%QTDIR%
echo [env] Using MINGW_DIR=%MINGW_DIR%

rem Persist these variables for caller (this script is intended to be called via "call")
