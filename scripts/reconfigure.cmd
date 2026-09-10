@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" amd64 >nul 2>&1
if errorlevel 1 (
  echo VCVARS FAILED
  exit /b 1
)
set "CMAKE=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "NINJA=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
set "REPO=C:\Users\kesha\KytyPlus-fix"
set "BUILD=%REPO%\build"
set "QT=C:\Qt\6.10.3\msvc2022_64"

echo === CMAKE CONFIGURE ===
"%CMAKE%" -S "%REPO%" -B "%BUILD%" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DQt6_DIR="%QT%\lib\cmake\Qt6" -DCMAKE_PREFIX_PATH="%QT%"
if errorlevel 1 (
  echo CMAKE CONFIGURE FAILED
  exit /b 1
)
echo === CONFIGURE OK ===
exit /b 0
