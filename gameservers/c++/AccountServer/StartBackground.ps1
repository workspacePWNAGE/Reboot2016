$ErrorActionPreference='Stop'
$python=Join-Path $PSScriptRoot '.venv\Scripts\python.exe'
if (!(Test-Path -LiteralPath $python)) { throw 'Run Setup.ps1 first.' }
if (Get-NetTCPConnection -LocalPort 8765 -State Listen -ErrorAction SilentlyContinue) { throw 'Port 8765 is already in use. Use the running service or stop it first.' }
$data=Join-Path $PSScriptRoot 'data'
New-Item -ItemType Directory -Force -Path $data | Out-Null
$process=Start-Process -FilePath $python -ArgumentList @('-m','uvicorn','account_server.app:create_app','--factory','--host','127.0.0.1','--port','8765','--workers','1','--limit-concurrency','32','--timeout-keep-alive','5','--no-access-log','--no-proxy-headers') -WorkingDirectory $PSScriptRoot -WindowStyle Hidden -RedirectStandardOutput "$data\server.stdout.log" -RedirectStandardError "$data\server.stderr.log" -PassThru
$listener=$null
for($attempt=0;$attempt -lt 20;$attempt++) {
    Start-Sleep -Milliseconds 250
    $listener=Get-NetTCPConnection -LocalPort 8765 -State Listen -ErrorAction SilentlyContinue | Select-Object -First 1
    if($listener){break}
    if($process.HasExited){throw "Account server exited. Read $data\server.stderr.log"}
}
if(!$listener){throw "Account server did not bind port 8765. Read $data\server.stderr.log"}
$server=Get-CimInstance Win32_Process -Filter "ProcessId=$($listener.OwningProcess)"
if ($server.ProcessId -ne $process.Id -and $server.ParentProcessId -ne $process.Id) { throw 'Port was taken by another process during startup.' }
[pscustomobject]@{server_id=$server.ProcessId; started=$server.CreationDate.ToString('o'); parent_id=$process.Id} | ConvertTo-Json | Set-Content "$data\service-process.json" -Encoding ascii
Write-Output 'Account service running at http://127.0.0.1:8765. Use Stop.ps1 to stop this background instance.'
