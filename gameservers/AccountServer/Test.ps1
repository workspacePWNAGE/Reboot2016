$ErrorActionPreference='Stop'
Push-Location $PSScriptRoot
try { & "$PSScriptRoot\.venv\Scripts\python.exe" -m pytest -q; exit $LASTEXITCODE } finally { Pop-Location }
