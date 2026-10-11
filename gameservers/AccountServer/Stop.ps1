$ErrorActionPreference='Stop'
$stateFile=Join-Path $PSScriptRoot 'data\service-process.json'
if (!(Test-Path -LiteralPath $stateFile)) { throw 'No recorded background server. For a foreground Start.ps1 server, press Ctrl+C in its terminal.' }
$state=Get-Content -LiteralPath $stateFile -Raw | ConvertFrom-Json
$server=Get-CimInstance Win32_Process -Filter "ProcessId=$($state.server_id)"
if (!$server) { Write-Output 'Recorded account server is already stopped.'; exit 0 }
if ($server.CreationDate.ToUniversalTime().Ticks -ne ([datetime]$state.started).ToUniversalTime().Ticks -or $server.CommandLine -notmatch 'uvicorn account_server\.app:create_app') { throw 'Recorded PID no longer belongs to this server; nothing stopped.' }
$parent=Get-CimInstance Win32_Process -Filter "ProcessId=$($state.parent_id)"
$expectedPython=Join-Path $PSScriptRoot '.venv\Scripts\python.exe'
if ($parent -and $parent.ExecutablePath -ne $expectedPython) { throw 'Server parent is not this project Python environment; nothing stopped.' }
Stop-Process -Id $server.ProcessId
Write-Output 'Background account server stopped. SQLite data retained.'
