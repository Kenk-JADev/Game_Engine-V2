#requires -Version 5.1
<#
    Rune3D - Build-Skript
    Baut src/**.cpp mit dem MSYS2-GCC (ucrt64) zu build/Rune3D.exe und
    build/RuneEditor.exe.

    .\build.ps1                     nur bauen
    .\build.ps1 -Run                bauen und Rune3D starten
    .\build.ps1 -Clean              alles neu bauen
    .\build.ps1 -Test               Testsuite (C++-JSON-Test, Ruby-Tests, JSON-Bruecke)
    .\build.ps1 -RubyRoot C:\Ruby34-x64   andere Ruby-Installation benutzen

    Werkzeug-Suche (seit Etappe 19a):
      Ruby       -RubyRoot  >  $env:RUNE_RUBY_ROOT  >  ruby.exe im PATH (RbConfig)
                              >  C:\Ruby34-x64 / C:\Ruby33-x64 / C:\Ruby32-x64
      MSYS2-GCC  $env:RUNE_MSYS_ROOT\ucrt64  >  <Ruby>\msys64\ucrt64  >  C:\msys64\ucrt64
      Header, Bibliotheksverzeichnis, Importbibliothek (-l...) und DLL-Name
      werden aus RbConfig gelesen, nicht mehr hart aus "ruby-3.4.0" und
      "x64-ucrt-ruby340" geraten. Damit läuft dasselbe Skript auf dem
      Entwicklungsrechner, auf einem zweiten Rechner und in GitHub Actions
      (ruby/setup-ruby legt Ruby unter C:\hostedtoolcache\... ab).
#>
param(
    [switch]$Run,
    [switch]$Clean,
    [switch]$Test,
    [string]$RubyRoot = ''
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = $PSScriptRoot

function Write-Step {
    param([string]$Text)
    Write-Host $Text -ForegroundColor Cyan
}

# ------------------------------------------------------------ Werkzeug-Suche
function Resolve-RubyRoot {
    param([string]$Vorgabe)

    $kandidaten = @()
    if ($Vorgabe)             { $kandidaten += $Vorgabe }
    if ($env:RUNE_RUBY_ROOT)  { $kandidaten += $env:RUNE_RUBY_ROOT }

    # ruby.exe im PATH nach seinem Installationsprefix fragen.
    $imPath = Get-Command ruby.exe -ErrorAction SilentlyContinue
    if ($imPath) {
        $prefix = (& $imPath.Source -e "require 'rbconfig'; print RbConfig::CONFIG['prefix']") -join ''
        if ($prefix) { $kandidaten += $prefix }
    }
    $kandidaten += @('C:\Ruby34-x64', 'C:\Ruby33-x64', 'C:\Ruby32-x64')

    foreach ($k in $kandidaten) {
        if ($k -and (Test-Path (Join-Path $k 'bin\ruby.exe'))) {
            return (Resolve-Path $k).Path
        }
    }
    throw "Kein Ruby gefunden. Gesucht: $($kandidaten -join ' | '). " +
          'Abhilfe: .\build.ps1 -RubyRoot <pfad> oder $env:RUNE_RUBY_ROOT setzen.'
}

# Header-/Bibliotheks-Pfade aus RbConfig lesen (mit RubyInstaller-Fallback).
function Get-RubyInfo {
    param([string]$Root)

    $rubyExe = Join-Path $Root 'bin\ruby.exe'
    if (-not (Test-Path $rubyExe)) { throw "ruby.exe fehlt: $rubyExe" }

    $abfrage = "require 'rbconfig'; c = RbConfig::CONFIG; " +
               "print [c['rubyhdrdir'], c['rubyarchhdrdir'], c['libdir'], " +
               "c['LIBRUBYARG_SHARED'], c['RUBY_SO_NAME']].join('|')"
    $out = (& $rubyExe -e $abfrage) -join ''

    if ($out -and $out.Contains('|')) {
        $t = $out.Split('|')
        return [pscustomobject]@{
            Exe       = $rubyExe
            HeaderDir = $t[0]
            ArchDir   = $t[1]
            LibDir    = $t[2]
            LibArg    = $t[3]
            SoName    = $t[4]
        }
    }

    # Fallback: RubyInstaller-Standardlayout fuer Ruby 3.4 (ucrt).
    Write-Warning 'RbConfig-Abfrage leer — Standardpfade (Ruby 3.4, ucrt) werden benutzt.'
    $hdr = Join-Path $Root 'include\ruby-3.4.0'
    return [pscustomobject]@{
        Exe       = $rubyExe
        HeaderDir = $hdr
        ArchDir   = (Join-Path $hdr 'x64-mingw-ucrt')
        LibDir    = (Join-Path $Root 'lib')
        LibArg    = '-lx64-ucrt-ruby340'
        SoName    = 'x64-ucrt-ruby340'
    }
}

function Resolve-Toolchain {
    param([string]$Root)

    $kandidaten = @()
    if ($env:RUNE_MSYS_ROOT) { $kandidaten += (Join-Path $env:RUNE_MSYS_ROOT 'ucrt64') }
    $kandidaten += (Join-Path $Root 'msys64\ucrt64')
    $kandidaten += 'C:\msys64\ucrt64'
    $kandidaten += 'C:\msys2\ucrt64'

    foreach ($k in $kandidaten) {
        if (Test-Path (Join-Path $k 'bin\g++.exe')) { return (Resolve-Path $k).Path }
    }

    # Letzte Chance: ein ucrt64-g++ aus dem PATH.
    $imPath = Get-Command g++.exe -ErrorAction SilentlyContinue
    if ($imPath -and $imPath.Source -like '*ucrt64*') {
        return (Split-Path (Split-Path $imPath.Source))
    }
    throw 'Kein MSYS2-ucrt64-g++ gefunden. Gesucht: ' +
          "$($kandidaten -join ' | '). Abhilfe: `$env:RUNE_MSYS_ROOT setzen " +
          '(oder in GitHub Actions msys2/setup-msys2 benutzen).'
}

$RubyRootGefunden = Resolve-RubyRoot -Vorgabe $RubyRoot
$Ruby             = Get-RubyInfo -Root $RubyRootGefunden
$Toolchain        = Resolve-Toolchain -Root $RubyRootGefunden
$Gcc              = Join-Path $Toolchain 'bin\g++.exe'

foreach ($p in @($Gcc, $Ruby.HeaderDir, $Ruby.ArchDir, $Ruby.LibDir)) {
    if (-not (Test-Path $p)) { throw "Fehlt: $p" }
}

# g++ braucht seine eigenen DLLs im PATH (cc1plus, libstdc++, libwinpthread).
$env:PATH = "$Toolchain\bin;$RubyRootGefunden\bin;$env:PATH"

$BuildDir = Join-Path $ProjectRoot 'build'
$ObjDir   = Join-Path $BuildDir 'obj'
$Exe      = Join-Path $BuildDir 'Rune3D.exe'
$EdExe    = Join-Path $BuildDir 'RuneEditor.exe'

Write-Step "Ruby:      $RubyRootGefunden  ($($Ruby.SoName))"
Write-Step "Toolchain: $Toolchain"

# ------------------------------------------------------------ Tests (-Test)
# Bewusst VOR dem EXE-Bau: die Tests brauchen keine Grafik und laufen auch,
# wenn der Build gerade kaputt ist. tex_import_test.rb braucht allerdings
# build\RuneEditor.exe und wird sonst uebersprungen (tools/test_all.rb).
if ($Test) {
    New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
    $JsonTestExe = Join-Path $BuildDir 'json_test.exe'

    Write-Step 'Baue C++-Logiktest (src/editor/json.cpp + tools/json_test.cpp) ...'
    & $Gcc '-std=c++20', '-O1', '-Wall', '-Wextra', '-Wno-unused-parameter',
           "-I$ProjectRoot\src", "$ProjectRoot\src\editor\json.cpp",
           "$ProjectRoot\tools\json_test.cpp", '-o', $JsonTestExe
    if ($LASTEXITCODE -ne 0) { throw 'json_test.exe konnte nicht gebaut werden' }

    Write-Step 'C++-JSON-Test ...'
    & $JsonTestExe $ProjectRoot
    if ($LASTEXITCODE -ne 0) { throw 'C++-JSON-Test fehlgeschlagen' }

    Write-Step 'Ruby-Testsuite (tools/test_all.rb) ...'
    & $Ruby.Exe (Join-Path $ProjectRoot 'tools\test_all.rb')
    if ($LASTEXITCODE -ne 0) { throw 'Ruby-Testsuite fehlgeschlagen' }

    Write-Step 'JSON-Bruecke (C++-Schreiber gegen Ruby JSON.pretty_generate) ...'
    & $Ruby.Exe (Join-Path $ProjectRoot 'tools\json_bridge_check.rb'), $JsonTestExe, '--strict-format'
    if ($LASTEXITCODE -ne 0) { throw 'JSON-Brueckenpruefung fehlgeschlagen' }

    Write-Host 'ALLE TESTS GRUEN' -ForegroundColor Green
    return
}

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
    "-I$($Ruby.HeaderDir)", "-I$($Ruby.ArchDir)"
)

$Libs = @(
    # Wichtig: UCRT VOR der Ruby-Importbibliothek. Die Ruby-DLL exportiert
    # eigene CRT-Symbole (fclose, fwrite, ...); ohne -lucrtbase wuerden unsere
    # stdio-Aufrufe auf die Ruby-Exporte umgebogen und beim Schliessen von
    # Dateien mit Access Violation abstuerzen.
    '-lucrtbase',
    "-L$($Ruby.LibDir)", $Ruby.LibArg,
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
$laufzeitDlls = @(
    (Join-Path $RubyRootGefunden "bin\$($Ruby.SoName).dll"),
    (Join-Path $Toolchain 'bin\libgmp-10.dll'),
    (Join-Path $Toolchain 'bin\libwinpthread-1.dll')
)
foreach ($dll in $laufzeitDlls) {
    if (-not (Test-Path $dll)) {
        Write-Warning "Laufzeit-DLL fehlt: $dll (Doppelklick-Start koennte scheitern)"
        continue
    }
    Copy-Item $dll $BuildDir -Force
    Write-Host "kopiere $(Split-Path $dll -Leaf) nach build/" -ForegroundColor DarkGray
}
$builtin = Join-Path $RubyRootGefunden 'bin\ruby_builtin_dlls'
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
