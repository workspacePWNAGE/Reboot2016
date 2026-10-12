$ErrorActionPreference='Stop'
$root=$PSScriptRoot
$source='D:\Roblox\Library\zlib\include\zlib'
$objectDir="$root\obj\zlib"
New-Item -ItemType Directory -Path $objectDir,"$root\lib" -Force | Out-Null
$env:INCLUDE='C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\include;C:\Program Files (x86)\Windows Kits\10\Include\10.0.19041.0\ucrt;C:\Program Files (x86)\Windows Kits\10\Include\10.0.19041.0\shared;C:\Program Files (x86)\Windows Kits\10\Include\10.0.19041.0\um'
# Keep the stdcall zlib embedded in graphics3D unchanged. The cdecl library
# used by curl has its three shared internal symbols namespaced here.
$files=@(Get-ChildItem -LiteralPath $source -Filter '*.c' -Force | ForEach-Object FullName)
& 'C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\bin\cl.exe' /nologo /c /O2 /MD /Dz_errmsg=xboxwin_z_errmsg /Dzcalloc=xboxwin_zcalloc /Dzcfree=xboxwin_zcfree "/I$source" "/Fo$objectDir\" @files
if ($LASTEXITCODE -ne 0) { throw 'Private zlib compilation failed' }
$objects=@(Get-ChildItem -LiteralPath $objectDir -Filter '*.obj' | ForEach-Object FullName)
& 'C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\bin\lib.exe' /nologo "/out:$root\lib\zlib.lib" @objects
if ($LASTEXITCODE -ne 0) { throw 'Private zlib archive failed' }
