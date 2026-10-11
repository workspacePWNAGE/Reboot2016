param([string]$ConfigPath="$PSScriptRoot\RevivalServer.config.json",[switch]$ValidateOnly)
$ErrorActionPreference='Stop'
function Test-ServerConfig($config) {
 $fields=@('server_name','bind_address','port','max_players','game_id')
 if (@($config.PSObject.Properties.Name).Count -ne 5 -or @($config.PSObject.Properties.Name | Where-Object {$_ -notin $fields}).Count) {throw 'Unknown/missing configuration field'}
 if ($config.server_name -isnot [string] -or !$config.server_name.Trim() -or $config.server_name.Length -gt 64 -or $config.server_name -match '[\x00-\x1f]') {throw 'Server name must contain 1-64 printable characters'}
 $address=$null
 if ($config.bind_address -notmatch '^\d{1,3}(\.\d{1,3}){3}$' -or ![Net.IPAddress]::TryParse($config.bind_address,[ref]$address) -or $address.AddressFamily -ne [Net.Sockets.AddressFamily]::InterNetwork) {throw 'Bind address must be numeric IPv4'}
 foreach($item in @(@('port',65534),@('max_players',128),@('game_id',2147483647))) {
  $value=$config.($item[0]);if ($value -isnot [int] -and $value -isnot [long]) {throw "Invalid integer: $($item[0])"}
  if ($value -lt 1 -or $value -gt $item[1]) {throw "Out of range: $($item[0])"}
 }
 return $config
}
$config=Test-ServerConfig (Get-Content -LiteralPath $ConfigPath -Raw | ConvertFrom-Json)
if ($ValidateOnly) {Write-Output 'PASS: configuration valid';exit 0}
Add-Type -AssemblyName System.Windows.Forms,System.Drawing
$root=$PSScriptRoot
# Catalog IDs come from the existing trusted library scanner.
$priorRoot=$env:XBOX_LIBRARY_TEST_ROOT
try {$env:XBOX_LIBRARY_TEST_ROOT=$root;& "$root\bin\NoOpt\XboxClientWindows.exe" --libraryCatalogTest;if($LASTEXITCODE){throw 'Game library scan failed'}}
finally {$env:XBOX_LIBRARY_TEST_ROOT=$priorRoot}
$catalog=Get-Content -LiteralPath "$root\LibraryCatalogTest.json" -Raw | ConvertFrom-Json
$form=New-Object Windows.Forms.Form
$form.Text='Reboot 16 Dedicated LAN Server';$form.Size=New-Object Drawing.Size(690,590);$form.StartPosition='CenterScreen'
$controls=@{}
function Field($key,$label,$value,$y) {
 $caption=New-Object Windows.Forms.Label;$caption.Text=$label;$caption.SetBounds(20,$y,180,28);$form.Controls.Add($caption)
 $box=New-Object Windows.Forms.TextBox;$box.Text=[string]$value;$box.SetBounds(210,$y,430,28);$form.Controls.Add($box);$controls[$key]=$box
}
Field 'server_name' 'Server display name' $config.server_name 20
Field 'bind_address' 'Bind IPv4 (0.0.0.0 = all)' $config.bind_address 60
Field 'port' 'Game UDP port (TCP +1)' $config.port 100
Field 'max_players' 'Player limit (1-128)' $config.max_players 140
$games=New-Object Windows.Forms.ComboBox;$games.DropDownStyle='DropDownList';$games.SetBounds(210,180,430,28);$form.Controls.Add($games)
foreach($game in $catalog.games){[void]$games.Items.Add("$($game.game_id) - $($game.title)");if($game.game_id -eq $config.game_id){$games.SelectedIndex=$games.Items.Count-1}}
$gameLabel=New-Object Windows.Forms.Label;$gameLabel.Text='Local game';$gameLabel.SetBounds(20,180,180,28);$form.Controls.Add($gameLabel)
$start=New-Object Windows.Forms.Button;$start.Text='Save configuration and Start';$start.SetBounds(20,225,290,35);$form.Controls.Add($start)
$stop=New-Object Windows.Forms.Button;$stop.Text='Stop';$stop.SetBounds(330,225,150,35);$stop.Enabled=$false;$form.Controls.Add($stop)
$status=New-Object Windows.Forms.TextBox;$status.Multiline=$true;$status.ReadOnly=$true;$status.ScrollBars='Vertical';$status.SetBounds(20,280,620,235);$form.Controls.Add($status)
$status.Text='Stopped. Account service must run on this host. Remote account login requires HTTPS.'
$script:server=$null;$script:statusPath=Join-Path $root 'RevivalServer.status.json';$script:stopPath=Join-Path $root 'RevivalServer.stop'
$script:stderrPath=Join-Path $root 'RevivalServer.stderr.log'
function Request-Stop {
 if($script:server -and !$script:server.HasExited){[IO.File]::WriteAllText($script:stopPath,'stop');$status.Text='Stopping server; connected players will disconnect.'}
}
$start.Add_Click({
 try {
  if($script:server -and !$script:server.HasExited){throw 'This manager already owns a running server'}
  if($games.SelectedIndex -lt 0){throw 'Select an available local game'}
  $chosen=$catalog.games[$games.SelectedIndex]
  $cfg=[pscustomobject]@{server_name=$controls.server_name.Text;bind_address=$controls.bind_address.Text;port=[int]$controls.port.Text;max_players=[int]$controls.max_players.Text;game_id=[int]$chosen.game_id}
  [void](Test-ServerConfig $cfg)
  $cfg | ConvertTo-Json | Set-Content -LiteralPath $ConfigPath -Encoding UTF8
  Remove-Item -LiteralPath $script:stopPath -ErrorAction SilentlyContinue
  $startInfo=New-Object Diagnostics.ProcessStartInfo
  $startInfo.FileName="$root\bin\NoOpt\XboxLANServer.exe";$startInfo.WorkingDirectory=$root;$startInfo.UseShellExecute=$false;$startInfo.CreateNoWindow=$true
  $startInfo.EnvironmentVariables['LAN_SERVER_NAME']=$cfg.server_name
  $startInfo.EnvironmentVariables['LAN_SERVER_BIND']=$cfg.bind_address
  $startInfo.EnvironmentVariables['LAN_SERVER_PORT']=[string]$cfg.port
  $startInfo.EnvironmentVariables['LAN_SERVER_MAX_PLAYERS']=[string]$cfg.max_players
  $startInfo.EnvironmentVariables['LAN_SERVER_GAME_ID']=[string]$cfg.game_id
  $startInfo.EnvironmentVariables['LAN_STATUS_PATH']=$script:statusPath
  $startInfo.EnvironmentVariables['LAN_STOP_PATH']=$script:stopPath
  $script:server=[Diagnostics.Process]::Start($startInfo)
  $status.Text='Starting: checking account service, loading place, binding network/catalog...';$start.Enabled=$false;$stop.Enabled=$true
 }catch{$status.Text=$_.Exception.Message}
})
$stop.Add_Click({Request-Stop})
$timer=New-Object Windows.Forms.Timer;$timer.Interval=500
$timer.Add_Tick({
 if(!$script:server){return}
 try {
  if(Test-Path -LiteralPath $script:statusPath){
   $state=Get-Content -LiteralPath $script:statusPath -Raw | ConvertFrom-Json
   if($state.pid -eq $script:server.Id){
    $players=@($state.players_json | ConvertFrom-Json)
    $status.Text="$($state.server_name): $($state.state)`r`nBind: $($state.bind_address):$($state.port) UDP; catalog TCP $($state.catalog_port)`r`nPlayers: $($players.Count)/$($state.max_players)`r`n$($players.username -join ', ')`r`n$($state.error)`r`nClients must be on the same reachable network. No public-internet reachability is implied."
    if($state.bind_address -eq '0.0.0.0'){
     $ips=@([Net.NetworkInformation.NetworkInterface]::GetAllNetworkInterfaces() | Where-Object {$_.OperationalStatus -eq 'Up' -and $_.NetworkInterfaceType -ne 'Loopback'} | ForEach-Object {$_.GetIPProperties().UnicastAddresses} | Where-Object {$_.Address.AddressFamily -eq 'InterNetwork' -and $_.Address.ToString() -notlike '169.254.*'} | ForEach-Object {$_.Address.ToString()})
     $status.AppendText("`r`nHost interface addresses: $($ips -join ', ')")
    }
   }
  }
  if($script:server.HasExited){$start.Enabled=$true;$stop.Enabled=$false;if($script:server.ExitCode -ne 0){$status.AppendText("`r`nProcess exited with code $($script:server.ExitCode); see bin\NoOpt\LANServer.log.")}}
 }catch{$status.Text='Reading startup/status: '+$_.Exception.Message}
})
$timer.Start()
$form.Add_FormClosing({
 if($script:server -and !$script:server.HasExited){Request-Stop;if(!$script:server.WaitForExit(30000)){$_.Cancel=$true;$status.Text='Shutdown still pending; manager remains open to avoid orphaning its server.'}}
})
[void]$form.ShowDialog();$timer.Stop();$timer.Dispose();$form.Dispose()
