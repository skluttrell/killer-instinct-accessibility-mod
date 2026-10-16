param([string]$VcVars)
$ErrorActionPreference = "Stop"
$kiTestRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if (-not $VcVars) {
    $VcVars = Get-ChildItem "${env:ProgramFiles}\Microsoft Visual Studio", "${env:ProgramFiles(x86)}\Microsoft Visual Studio" -Directory -ErrorAction SilentlyContinue |
        Get-ChildItem -Directory | ForEach-Object { Join-Path $_.FullName "VC\Auxiliary\Build\vcvars64.bat" } |
        Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}
if (-not $VcVars) { throw "MSVC x64 build tools not found; pass -VcVars with the vcvars64.bat path" }
$kiTestBuild = Join-Path $kiTestRoot "build\tests"
New-Item -ItemType Directory -Force $kiTestBuild | Out-Null
$kiTestSources = @("tests\training_test.cpp", "src\training.cpp", "src\narrator.cpp", "src\strings.cpp", "src\common.cpp") |
    ForEach-Object { '"' + (Join-Path $kiTestRoot $_) + '"' }
$kiTestCommand = "call `"$VcVars`" >nul 2>&1 && cd /d `"$kiTestBuild`" && cl /nologo /O2 /MT /EHsc /std:c++17 /W3 $($kiTestSources -join ' ') /Fe:training_test.exe /link user32.lib"
cmd /c $kiTestCommand
if ($LASTEXITCODE -ne 0) { throw "training test build failed" }
& (Join-Path $kiTestBuild "training_test.exe")
if ($LASTEXITCODE -ne 0) { throw "training tests failed" }
