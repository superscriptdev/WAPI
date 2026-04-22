@echo off
setlocal
set "PATH=C:\Program Files\Microsoft Visual Studio\Installer;%PATH%"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
cd /d "%~dp0"
cl /nologo /I"..\..\\" /I"..\..\runtime\desktop\src" gen_atlas.c ..\..\runtime\desktop\src\third_party\miniz\miniz.c /Fegen_atlas.exe
if errorlevel 1 exit /b 1
.\gen_atlas.exe > assets.h
if errorlevel 1 exit /b 1
echo assets.h generated.
