@echo off
setlocal
set "CONFIG=Debug"
if exist "%~dp0bin\x64\Release\Sample_14_01_DepthPrepass.exe" set "CONFIG=Release"
if not exist "%~dp0bin\x64\%CONFIG%\Sample_14_01_DepthPrepass.exe" (
    echo Build Sample_14_01_DepthPrepass.sln for x64 first.
    exit /b 1
)
pushd "%~dp0Sample_14_01"
"%~dp0bin\x64\%CONFIG%\Sample_14_01_DepthPrepass.exe" %*
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%
