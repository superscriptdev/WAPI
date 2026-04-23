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

echo [hello_game] Compiling tracker.c
"%CLANG%" --target=wasm32-unknown-unknown -nostdlib -ffreestanding ^
  -isystem "%SYSROOT%" ^
  -O2 -I "%ROOT%\include" -I . ^
  -c tracker.c -o tracker.o || exit /b 1

echo [hello_game] Linking hello_game_tracker.wasm
"%LD%" tracker.o wapi_reactor.o ^
  --no-entry --export=tracker_render ^
  --export-memory --export-table --growable-table ^
  --allow-undefined ^
  -o hello_game_tracker.wasm || exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File emit_tracker_hash.ps1 || exit /b 1

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
dir /b hello_game.wasm hello_game_ai.wasm hello_game_tracker.wasm
echo.
echo Run with:
echo   powershell -NoProfile -ExecutionPolicy Bypass -File run.ps1
echo   (run.ps1 computes the hashes and passes both --module flags for
echo    hello_game_ai.wasm and hello_game_tracker.wasm)
