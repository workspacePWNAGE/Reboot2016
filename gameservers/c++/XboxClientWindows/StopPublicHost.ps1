param([switch]$Force)
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
$record=Get-Content -LiteralPath "$root\bin\NoOpt\PublicServer.process.json" -Raw | ConvertFrom-Json
$hostProcess=Get-Process -Id ([int]$record.pid) -ErrorAction SilentlyContinue
if(!$hostProcess){Write-Output 'Public host is already stopped.';return}
$expected=(Resolve-Path -LiteralPath "$root\bin\NoOpt\XboxLANServer.exe").Path
if($hostProcess.Path -ne $expected -or $hostProcess.StartTime.ToUniversalTime().Ticks -ne ([datetime]$record.started).ToUniversalTime().Ticks){throw 'Process identity differs; refusing to stop an unrelated process.'}
Set-Content -LiteralPath "$root\bin\NoOpt\PublicServer.stop" -Value 'stop'
if(!$hostProcess.WaitForExit(25000)){
    if(!$Force){throw 'Host is hung during shutdown. To terminate this verified host, run StopPublicHost.ps1 -Force. This disconnects its players.'}
    $hostProcess.Refresh()
    if($hostProcess.Path -ne $expected -or $hostProcess.StartTime.ToUniversalTime().Ticks -ne ([datetime]$record.started).ToUniversalTime().Ticks){throw 'Process identity changed; refusing forced termination.'}
    Stop-Process -Id $hostProcess.Id -Force
    if(!$hostProcess.WaitForExit(10000)){throw 'Verified host could not be terminated.'}
    if(Test-Path -LiteralPath "$root\bin\NoOpt\PublicServer.stop"){Remove-Item -LiteralPath "$root\bin\NoOpt\PublicServer.stop"}
    $statusFile="$root\bin\NoOpt\PublicServer.status.json"
    if(Test-Path -LiteralPath $statusFile){
        $status=Get-Content -LiteralPath $statusFile -Raw | ConvertFrom-Json
        if($status.pid -eq $record.pid){$status.state='terminated';$status.error='Forced termination after graceful shutdown hung; session ended.';$status.players_json='[]';$status | ConvertTo-Json -Compress | Set-Content -LiteralPath $statusFile -Encoding ascii}
    }
    Write-Output 'Hung public host terminated. This was not a clean engine shutdown.'
    return
}
Write-Output 'Public host shut down.'
