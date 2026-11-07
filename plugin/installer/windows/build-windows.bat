@echo off
REM Builds Murmur (VST3 + standalone) and the Windows installer.
REM Needs: Visual Studio 2022 with "Desktop development with C++", CMake, Git, Inno Setup 6.
setlocal
cd /d "%~dp0\..\.."

where cmake >nul 2>nul || (echo Install CMake from https://cmake.org/download/ & pause & exit /b 1)

set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" set "ISCC=%ProgramFiles%\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" (echo Install Inno Setup 6 from https://jrsoftware.org/isdl.php & pause & exit /b 1)

for /f "tokens=3" %%v in ('findstr /r /c:"^project(Murmur VERSION" CMakeLists.txt') do set "VER=%%v"
set "VER=%VER:)=%"
echo === Building Murmur %VER% ===

cmake -B build-win -G "Visual Studio 17 2022" -A x64 || goto :fail
cmake --build build-win --config Release --parallel || goto :fail

"%ISCC%" /DMyAppVersion=%VER% installer\windows\Murmur.iss || goto :fail

echo.
echo === DONE: dist\Murmur-%VER%-windows-setup.exe ===
pause
exit /b 0

:fail
echo.
echo BUILD FAILED
pause
exit /b 1
