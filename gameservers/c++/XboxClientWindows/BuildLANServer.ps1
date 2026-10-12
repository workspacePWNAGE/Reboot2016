$ErrorActionPreference='Stop'
$env:PATH = 'C:\Program Files (x86)\Windows Kits\10\bin\10.0.19041.0\x86;' + $env:PATH
& 'C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe' "$PSScriptRoot\LANServer\XboxLANServer.vcxproj" /t:Build /p:Configuration=NoOpt /p:Platform=Win32 '/p:VCTargetsPath=C:\Program Files (x86)\MSBuild\Microsoft.Cpp\v4.0\V140\' '/p:WindowsSdkDir=C:\Program Files (x86)\Windows Kits\10\' '/p:VCInstallDir=C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\' /m:2 /nologo /v:minimal
exit $LASTEXITCODE
