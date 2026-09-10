@echo off
setlocal

set "VCV=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
set "MSVC=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe"

echo === CALL VCVARS ===
call "%VCV%" amd64
echo === VCVARS DONE ===
where cl
"%MSVC%"
echo === CL EXIT CODE ===
echo %ERRORLEVEL%
