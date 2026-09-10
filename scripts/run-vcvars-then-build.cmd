@echo off
setlocal enabledelayedexpansion

set "VS2022_ROOT=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
set "VCVARS=%VS2022_ROOT%\VC\Auxiliary\Build\vcvarsall.bat"
set "MSVC=%VS2022_ROOT%\VC\Tools\MSVC\14.44.35207"
set "CL=%MSVC%\bin\Hostx64\x64\cl.exe"
set "NINJA=%USERPROFILE%\Desktop\ninja_bin\ninja.exe"
set "CMAKE=%USERPROFILE%\Desktop\cmake_bin\cmake.exe"
set "REPO=C:\Users\kesha\KytyPlus-fix"
set "BUILD=%REPO%\build"

echo === VCVARS START ===
call "%VCVARS%" amd64
echo === VCVARS DONE ===
where cl
cl
echo === CL VERSION ===

if not exist "%BUILD%" (
    echo === CMAKE CONFIGURE ===
    "%CMAKE%" -S "%REPO%" -B "%BUILD%" -G Ninja -DCMAKE_BUILD_TYPE=Debug
    if errorlevel 1 (
        echo CMAKE CONFIGURE FAILED
        exit /b 1
    )
)

echo === BUILD ===
"%NINJA%" -C "%BUILD%"
set BUILD_EXIT=%ERRORLEVEL%

echo === BUILD EXIT CODE ===
exit /b %BUILD_EXIT%
