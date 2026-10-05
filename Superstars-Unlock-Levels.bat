@echo off
setlocal
cd /d "%~dp0"
python scripts\sonic_superstars_unlock.py %*
set "unlock_exit=%errorlevel%"
exit /b %unlock_exit%
