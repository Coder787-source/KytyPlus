@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" amd64 >nul 2>&1
if errorlevel 1 (
  echo VCVARS FAILED
  exit /b 1
)
set "NINJA=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
set "BUILD=C:\Users\kesha\KytyPlus-fix\build"
echo === NINJA BUILD (all targets) ===
"%NINJA%" -C "%BUILD%"
exit /b %errorlevel%
