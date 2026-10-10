# Build kiaccess.dll (x64, static CRT) with the MSVC Build Tools. Usage: powershell -ExecutionPolicy Bypass -File build.ps1
# Output: build\kiaccess.dll (+ build\dinput8.dll, the same file under the proxy name) and build\kiaccess\ (data, ini, prism.dll).
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$build = Join-Path $root "build"
New-Item -ItemType Directory -Force $build | Out-Null
$vcvars = "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvars)) { $vcvars = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" }
$src = @("src\dllmain.cpp", "src\common.cpp", "src\gfx.cpp", "src\strings.cpp", "src\speech.cpp", "src\snapshot.cpp", "src\narrator.cpp", "src\hooks.cpp", "src\pak.cpp", "src\radar.cpp",
         "deps\minhook\src\buffer.c", "deps\minhook\src\hook.c", "deps\minhook\src\trampoline.c", "deps\minhook\src\hde\hde64.c")
$srcList = ($src | ForEach-Object { '"' + (Join-Path $root $_) + '"' }) -join " "
$cmd = "call `"$vcvars`" >nul 2>&1 && cd /d `"$build`" && cl /nologo /O2 /MT /EHsc /std:c++17 /W3 /DWIN32_LEAN_AND_MEAN /DNDEBUG /I`"$root\deps\minhook\include`" $srcList /link /DLL /MAP:kiaccess.map /OUT:kiaccess.dll /DEF:`"$root\src\exports.def`" user32.lib bcrypt.lib"
cmd /c $cmd
if ($LASTEXITCODE -ne 0) { Write-Error "build failed"; exit 1 }
Copy-Item (Join-Path $build "kiaccess.dll") (Join-Path $build "dinput8.dll") -Force
# runtime folder next to the DLL
$kd = Join-Path $build "kiaccess"
New-Item -ItemType Directory -Force (Join-Path $kd "data") | Out-Null
$proj = Split-Path -Parent $root
# strings_en.tsv is not copied: the DLL generates it from the game's PAK\DX11\GLOBAL.PAK on first run (src\pak.cpp)
if (Test-Path (Join-Path $proj "data\fighter_names.json")) { Copy-Item (Join-Path $proj "data\fighter_names.json") (Join-Path $kd "data\fighter_names.json") -Force }
Copy-Item (Join-Path $root "data\fighter_appearance.json") (Join-Path $kd "data\fighter_appearance.json") -Force
Copy-Item (Join-Path $root "data\sl_prompts.json") (Join-Path $kd "data\sl_prompts.json") -Force
$prism = "$env:APPDATA\Python\Python310\site-packages\prism\_native\prism.dll"
if (Test-Path $prism) { Copy-Item $prism (Join-Path $kd "prism.dll") -Force }
if (-not (Test-Path (Join-Path $kd "kiaccess.ini"))) { Copy-Item (Join-Path $root "kiaccess.ini") (Join-Path $kd "kiaccess.ini") }
Write-Output "built $build\kiaccess.dll"
