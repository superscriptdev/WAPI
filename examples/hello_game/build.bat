@echo off
setlocal enableextensions
set "PATH=C:\Program Files\Microsoft Visual Studio\Installer;%PATH%"
cd /d "%~dp0"

set "ROOT=%~dp0..\.."
set "WASI=C:\Dev\wasi-sdk"
set "CLANG=%WASI%\bin\clang.exe"
set "LD=%WASI%\bin\wasm-ld.exe"
set "SYSROOT=%WASI%\share\wasi-sysroot\include\wasm32-wasi"

if not exist assets.h (
    echo [hello_game] Generating assets.h ...
    call "%~dp0gen_atlas.bat" || exit /b 1
)

echo [hello_game] Compiling ai.c
"%CLANG%" --target=wasm32-unknown-unknown -nostdlib -ffreestanding ^
  -isystem "%SYSROOT%" ^
  -O2 -I "%ROOT%\include" ^
  -c ai.c -o ai.o || exit /b 1

echo [hello_game] Compiling reactor shim
"%CLANG%" --target=wasm32-unknown-unknown -nostdlib -ffreestanding ^
  -isystem "%SYSROOT%" ^
  -O2 -I "%ROOT%\include" ^
  -c "%ROOT%\bindings\c\wapi_reactor.c" -o wapi_reactor.o || exit /b 1

echo [hello_game] Linking hello_game_ai.wasm
"%LD%" ai.o wapi_reactor.o ^
  --no-entry --export=ai_tick ^
  --export-memory --export-table --growable-table ^
  --allow-undefined ^
  -o hello_game_ai.wasm || exit /b 1

rem Compute SHA-256 of the AI wasm and emit ai_hash.h via a .ps1 file
powershell -NoProfile -ExecutionPolicy Bypass -File emit_ai_hash.ps1 || exit /b 1

echo [hello_game] Compiling game.c
"%CLANG%" --target=wasm32-unknown-unknown -nostdlib -ffreestanding ^
  -isystem "%SYSROOT%" ^
  -O2 -I "%ROOT%\include" -I . ^
  -c game.c -o game.o || exit /b 1

echo [hello_game] Linking hello_game.wasm
"%LD%" game.o wapi_reactor.o ^
  --no-entry --export=wapi_main --export=wapi_frame ^
  --export-memory --export-table --growable-table ^
  --allow-undefined ^
  -o hello_game.wasm || exit /b 1

echo.
echo === Built ===
dir /b hello_game.wasm hello_game_ai.wasm
echo.
echo Run with:
echo   ..\..\runtime\desktop\build\wapi_runtime.exe hello_game.wasm --module ^<hash^>=hello_game_ai.wasm
echo   (hash baked into ai_hash.h, copy from that file or read via a helper)
