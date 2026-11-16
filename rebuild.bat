@echo off
rem Quick incremental rebuild (after build-ar.bat ran once).
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
cd /d "%~dp0build" || exit /b 1
nmake || exit /b 1
echo BUILD OK
