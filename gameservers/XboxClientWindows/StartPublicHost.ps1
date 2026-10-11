$ErrorActionPreference='Stop'
$root=$PSScriptRoot
$config=Get-Content -LiteralPath "$root\RevivalServer.config.json" -Raw | ConvertFrom-Json
$exe=Join-Path $root 'bin\NoOpt\XboxLANServer.exe'
if($config.bind_address -ne '127.0.0.1'){throw 'Public tunnels require bind_address 127.0.0.1 in RevivalServer.config.json.'}
$port=[int]$config.port
if($port -lt 1 -or $port -gt 65534){throw 'Invalid server port.'}
if(Get-NetUDPEndpoint -LocalPort $port -ErrorAction SilentlyContinue){throw 'The game port is already occupied. Stop the existing host first.'}
if(!(Get-NetTCPConnection -LocalPort 8765 -State Listen -ErrorAction SilentlyContinue)){& "$root\..\AccountServer\StartBackground.ps1"}
$env:XBOX_EXTERNAL_ADMISSION='1'
$env:XBOX_R16_URL='https://r16.noli.vip'
$env:XBOX_ACCOUNT_URL='http://127.0.0.1:8765'
$env:LAN_SERVER_BIND='127.0.0.1'
$env:LAN_SERVER_PORT=[string]$port
$env:LAN_SERVER_GAME_ID=[string][int]$config.game_id
$env:LAN_SERVER_NAME=[string]$config.server_name
$env:LAN_SERVER_MAX_PLAYERS=[string][int]$config.max_players
$env:LAN_STOP_PATH=Join-Path $root 'bin\NoOpt\PublicServer.stop'
$env:LAN_STATUS_PATH=Join-Path $root 'bin\NoOpt\PublicServer.status.json'
if(Test-Path -LiteralPath $env:LAN_STOP_PATH){throw 'A previous public stop request exists; inspect it before starting.'}
$hostProcess=Start-Process -FilePath $exe -WorkingDirectory $root -WindowStyle Hidden -PassThru
for($attempt=0;$attempt -lt 120;$attempt++) {
    Start-Sleep -Milliseconds 250
    $hostProcess.Refresh()
    if($hostProcess.HasExited){throw "Public host exited with $($hostProcess.ExitCode); inspect bin\NoOpt\LANServer.log."}
    if(Test-Path -LiteralPath $env:LAN_STATUS_PATH) {
        $status=Get-Content -LiteralPath $env:LAN_STATUS_PATH -Raw | ConvertFrom-Json
        if($status.pid -eq $hostProcess.Id -and $status.state -eq 'running') {
            [pscustomobject]@{pid=$hostProcess.Id;started=$hostProcess.StartTime.ToUniversalTime().ToString('o')} | ConvertTo-Json | Set-Content "$root\bin\NoOpt\PublicServer.process.json" -Encoding ascii
            Write-Output "Public host ready: UDP 127.0.0.1:$port, catalog/admission HTTP 127.0.0.1:$($port+1). Existing HTTPS tunnel required. Public gameplay is not yet verified."
            return
        }
    }
}
Set-Content -LiteralPath $env:LAN_STOP_PATH -Value 'stop'
$hostProcess.WaitForExit(25000) | Out-Null
throw 'Public host startup timed out; stop requested. Inspect LANServer.log.'
