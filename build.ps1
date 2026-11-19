#requires -Version 5.1
<#
    Rune3D - Build-Skript
    Baut src/**.cpp mit dem MSYS2-GCC (ucrt64) zu build/Rune3D.exe.

    .\build.ps1           nur bauen
    .\build.ps1 -Run      bauen und starten
    .\build.ps1 -Clean    alles neu bauen
#>
param(
    [switch]$Run,
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = $PSScriptRoot

$RubyRoot  = 'C:\Ruby34-x64'
$Msys      = Join-Path $RubyRoot 'msys64'
$Toolchain = Join-Path $Msys 'ucrt64'
$Gcc       = Join-Path $Toolchain 'bin\g++.exe'
$Make      = Join-Path $Msys 'usr\bin\make.exe'

$RubyInc   = Join-Path $RubyRoot 'include\ruby-3.4.0'
$RubyArch  = Join-Path $RubyInc 'x64-mingw-ucrt'
$RubyLib   = Join-Path $RubyRoot 'lib'

foreach ($p in @($Gcc, $RubyInc, $RubyArch, $RubyLib)) {
    if (-not (Test-Path $p)) { throw "Fehlt: $p" }
}

# g++ braucht seine eigenen DLLs im PATH (cc1plus, libstdc++, libwinpthread).
$env:PATH = "$Toolchain\bin;$RubyRoot\bin;$env:PATH"

$BuildDir = Join-Path $ProjectRoot 'build'
$ObjDir   = Join-Path $BuildDir 'obj'
$Exe      = Join-Path $BuildDir 'Rune3D.exe'
$EdExe    = Join-Path $BuildDir 'RuneEditor.exe'

# Zwei Ziele: Rune3D.exe (Spiel, main.cpp) und RuneEditor.exe (Scene-Editor,
# eigenes Programm ohne Game-Engine, editor_main.cpp). Alle anderen Quellen
# sind gemeinsam (Renderer, Terrain, Mathe, Log, Ruby-Bindings).
$GameExcludes    = @('editor_main.cpp', 'json.cpp')
$EditorExcludes  = @('main.cpp')

if ($Clean -and (Test-Path $BuildDir)) {
    Remove-Item $BuildDir -Recurse -Force
    Write-Host 'build/ geloescht' -ForegroundColor DarkGray
}
New-Item -ItemType Directory -Force -Path $ObjDir | Out-Null

$CxxFlags = @(
    '-std=c++20', '-O2', '-Wall', '-Wextra', '-Wno-unused-parameter',
    '-Wno-missing-field-initializers',
    '-DUNICODE', '-D_UNICODE', '-DNOMINMAX', '-DWIN32_LEAN_AND_MEAN',
    "-I$ProjectRoot\src",
    "-I$RubyInc", "-I$RubyArch"
)

$Libs = @(
    # Wichtig: UCRT VOR der Ruby-Importbibliothek. Die Ruby-DLL exportiert
    # eigene CRT-Symbole (fclose, fwrite, ...); ohne -lucrtbase wuerden unsere
    # stdio-Aufrufe auf die Ruby-Exporte umgebogen und beim Schliessen von
    # Dateien mit Access Violation abstuerzen.
    '-lucrtbase',
    "-L$RubyLib", '-lx64-ucrt-ruby340',
    '-ld3d11', '-ldxgi', '-ld3dcompiler', '-ldxguid',
    '-luuid', '-lole32', '-loleaut32', '-luxtheme', '-lcomctl32',
    '-luser32', '-lgdi32', '-ladvapi32', '-lshell32', '-lshlwapi',
    '-lwinmm', '-lws2_32', '-liphlpapi', '-lwindowscodecs', '-lcomdlg32',
    # libwinpthread statisch einlinken: Der g++-Driver haengt HINTER den
    # Nutzer-Flags noch -lpthread (dynamisch) an. --whole-archive erzwingt
    # die statische libwinpthread.a komplett, damit die spaeteren Treffer
    # nicht mehr an die DLL gehen. Dynamisch wuerde jede EXE
    # libwinpthread-1.dll aus der MSYS2-Toolchain im PATH verlangen
    # (Doppelklick ohne Ruby-PATH -> "Systemfehler").
    '-static-libgcc', '-static-libstdc++',
    '-Wl,-Bstatic,--whole-archive', '-lwinpthread', '-Wl,--no-whole-archive,-Bdynamic'
)

$Sources = Get-ChildItem -Path (Join-Path $ProjectRoot 'src') -Recurse -Filter *.cpp | Sort-Object FullName
if (-not $Sources) { throw 'Keine Quelldateien in src/ gefunden' }

$Headers = Get-ChildItem -Path (Join-Path $ProjectRoot 'src') -Recurse -Filter *.h
$NewestHeader = ($Headers | Sort-Object LastWriteTime -Descending | Select-Object -First 1).LastWriteTime

function Build-Target($name, $excludes, $outExe) {
    $Objects = @()
    $Built = 0
    $Skipped = 0

    foreach ($src in $Sources) {
        $rel = $src.FullName.Substring((Join-Path $ProjectRoot 'src').Length + 1)
        $base = Split-Path $rel -Leaf
        if ($excludes -contains $base) { continue }
        $objName = ($rel -replace '[\\/]', '_') -replace '\.cpp$', '.o'
        $obj = Join-Path $ObjDir "$($name)_$objName"
        $Objects += $obj

        $needsBuild = $true
        if ((Test-Path $obj) -and -not $Clean) {
            $objTime = (Get-Item $obj).LastWriteTime
            if ($src.LastWriteTime -le $objTime -and $NewestHeader -le $objTime) {
                $needsBuild = $false
            }
        }

        if ($needsBuild) {
            Write-Host "  kompiliere [$name] $rel" -ForegroundColor DarkGray
            & $Gcc @CxxFlags -c $src.FullName -o $obj
            if ($LASTEXITCODE -ne 0) { throw "Kompilierung fehlgeschlagen: $rel" }
            $Built++
        } else {
            $Skipped++
        }
    }

    Write-Host "  [$name] $Built kompiliert, $Skipped uebersprungen" -ForegroundColor DarkGray
    Write-Host "  linke $(Split-Path $outExe -Leaf)" -ForegroundColor DarkGray
    & $Gcc @Objects @Libs -o $outExe
    if ($LASTEXITCODE -ne 0) { throw 'Linken fehlgeschlagen' }

    $size = [math]::Round((Get-Item $outExe).Length / 1KB, 1)
    Write-Host "OK: $outExe ($size KB)" -ForegroundColor Green
}

Build-Target 'game'   $GameExcludes   $Exe
Build-Target 'editor' $EditorExcludes $EdExe

# Manifest fuer moderne Windows-Steuerelemente neben den Editor legen.
Copy-Item (Join-Path $ProjectRoot 'src\editor\RuneEditor.exe.manifest') $BuildDir -Force

# Laufzeit-DLLs neben die EXEs kopieren (XCopy-Deployment): Die Ruby-DLL
# importiert libgmp-10.dll und libwinpthread-1.dll und fordert per Manifest
# die private SxS-Assembly "ruby_builtin_dlls" an (RubyInstaller-Standards).
# Ohne diesen Ordner startet nichts per Doppelklick (0xc0150002 bzw.
# "libwinpthread-1.dll nicht gefunden"); im Shell-Test lief es nur, weil der
# Ruby-PATH die DLLs verbirgt.
foreach ($dll in @(
    (Join-Path $RubyRoot 'bin\x64-ucrt-ruby340.dll'),
    (Join-Path $Toolchain 'bin\libgmp-10.dll'),
    (Join-Path $Toolchain 'bin\libwinpthread-1.dll')
)) {
    Copy-Item $dll $BuildDir -Force
    Write-Host "kopiere $(Split-Path $dll -Leaf) nach build/" -ForegroundColor DarkGray
}
$builtin = Join-Path $RubyRoot 'bin\ruby_builtin_dlls'
if (Test-Path $builtin) {
    $dst = Join-Path $BuildDir 'ruby_builtin_dlls'
    if (Test-Path $dst) { Remove-Item $dst -Recurse -Force }   # idempotent: kein Schachteln
    Copy-Item $builtin $dst -Recurse -Force
    Write-Host 'kopiere ruby_builtin_dlls\ nach build/' -ForegroundColor DarkGray
}

if ($Run) {
    Write-Host '  starte Rune3D ...' -ForegroundColor DarkGray
    Push-Location $ProjectRoot
    try {
        & $Exe
    } finally {
        Pop-Location
    }
}
