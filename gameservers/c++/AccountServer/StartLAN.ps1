param(
    [Parameter(Mandatory=$true)][string]$Certificate,
    [Parameter(Mandatory=$true)][string]$PrivateKey,
    [string]$BindAddress='0.0.0.0',
    [ValidateRange(1,65535)][int]$Port=8765
)
$ErrorActionPreference='Stop'
$python=Join-Path $PSScriptRoot '.venv\Scripts\python.exe'
$certificatePath=(Resolve-Path -LiteralPath $Certificate).Path
$privateKeyPath=(Resolve-Path -LiteralPath $PrivateKey).Path
Push-Location $PSScriptRoot
try {
    & $python -m uvicorn account_server.app:create_app --factory --host $BindAddress --port $Port --workers 1 --limit-concurrency 32 --timeout-keep-alive 5 --no-access-log --no-proxy-headers --ssl-certfile $certificatePath --ssl-keyfile $privateKeyPath
    exit $LASTEXITCODE
} finally { Pop-Location }
