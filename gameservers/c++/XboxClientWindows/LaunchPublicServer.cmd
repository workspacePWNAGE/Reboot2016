@echo off
setlocal
cd /d "%~dp0"
set "XBOX_EXTERNAL_ADMISSION=1"
rem Keep bind address 127.0.0.1. playit and cloudflared run on this Windows host.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0StartPublicHost.ps1"
pause
endlocal
