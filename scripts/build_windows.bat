@echo off
REM RogueByte Stego Pro — Windows x64 Build Script
REM Author: SidoDev-Dz (RogueByte)

echo ========================================================
echo  Building RogueByte Stego Pro on Windows 10/11 x64
echo  Author: SidoDev-Dz ^| Organization: RogueByte
echo ========================================================

set BUILD_DIR=build\windows-release
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

cmake -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A x64
if %ERRORLEVEL% neq 0 (
    echo [-] CMake configuration failed. Ensure MSVC and OpenSSL are installed.
    exit /b %ERRORLEVEL%
)

cmake --build "%BUILD_DIR%" --config Release
if %ERRORLEVEL% neq 0 (
    echo [-] Build failed.
    exit /b %ERRORLEVEL%
)

echo [+] Build succeeded!
"%BUILD_DIR%\Release\rb-stego.exe" version
