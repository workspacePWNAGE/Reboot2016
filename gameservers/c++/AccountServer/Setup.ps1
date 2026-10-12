$ErrorActionPreference='Stop'
if (!(Test-Path "$PSScriptRoot\.venv\Scripts\python.exe")) {
    & py -3.12 -m venv "$PSScriptRoot\.venv"
    if ($LASTEXITCODE) { throw 'Python environment creation failed' }
}
& "$PSScriptRoot\.venv\Scripts\python.exe" -m pip install -r "$PSScriptRoot\requirements.lock.txt"
if ($LASTEXITCODE) { throw 'Dependency installation failed' }
