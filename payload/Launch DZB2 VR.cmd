@echo off
setlocal
set "BF2VR_GAME=%~dp0"
if exist "%BF2VR_GAME%BF2-VR-DZB2.exe" goto beside
for /f "tokens=2,*" %%A in ('reg query "HKCU\Software\BF2-VR" /v GameFolder 2^>nul') do set "BF2VR_GAME=%%B"
if not exist "%BF2VR_GAME%\BF2-VR-DZB2.exe" goto missing
"%BF2VR_GAME%\BF2-VR-DZB2.exe"
goto result
:beside
"%BF2VR_GAME%BF2-VR-DZB2.exe"
:result
set "result=%errorlevel%"
if not "%result%"=="0" pause
exit /b %result%
:missing
echo Run BF2-VR Setup first, then use Launch DZB2 VR.cmd in your Battlefield 2 folder.
pause
exit /b 1
