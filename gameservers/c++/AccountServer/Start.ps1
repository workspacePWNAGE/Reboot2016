$ErrorActionPreference='Stop'
$python=Join-Path $PSScriptRoot '.venv\Scripts\python.exe'
if (!(Test-Path -LiteralPath $python)) { throw 'Run Setup.ps1 first (Python 3.12 is required).' }
Push-Location $PSScriptRoot
try {
    & $python -m uvicorn account_server.app:create_app --factory --host 127.0.0.1 --port 8765 --workers 1 --limit-concurrency 32 --timeout-keep-alive 5 --no-access-log --no-proxy-headers
    exit $LASTEXITCODE
} finally { Pop-Location }
