$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Definition
Set-Location $here

$aiHash      = (Get-FileHash hello_game_ai.wasm      -Algorithm SHA256).Hash.ToLower()
$trackerHash = (Get-FileHash hello_game_tracker.wasm -Algorithm SHA256).Hash.ToLower()
$runtime     = Join-Path $here "..\..\runtime\desktop\build\wapi_runtime.exe"

& $runtime hello_game.wasm `
    --module "$aiHash=hello_game_ai.wasm" `
    --module "$trackerHash=hello_game_tracker.wasm"
