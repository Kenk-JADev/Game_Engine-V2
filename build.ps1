#requires -Version 5.1
<#
    Rune3D - Build-Skript
    Baut src/**.cpp mit dem MSYS2-GCC (ucrt64) zu build/Rune3D.exe und
    build/RuneEditor.exe.

    .\build.ps1                     nur bauen
    .\build.ps1 -Run                bauen und Rune3D starten
    .\build.ps1 -Clean              alles neu bauen
    .\build.ps1 -Test               C++-/Datei- und Ruby-Tests, JSON-/Event-Brücke
    .\build.ps1 -WarpTest           D3D11-WARP und Ruby-Texturbrücke (Software)
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
    [switch]$WarpTest,
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

    # Base64 hält auch Unicode-Pfade beim Umweg über PowerShell 5.1 intakt.
    $imPath = Get-Command ruby.exe -ErrorAction SilentlyContinue
    if ($imPath) {
        $encoded = (& $imPath.Source -e "require 'rbconfig'; print [RbConfig::CONFIG['prefix'].to_s.encode('UTF-8')].pack('m0')") -join ''
        if ($LASTEXITCODE -eq 0 -and $encoded) {
            try { $kandidaten += [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($encoded)) }
            catch { Write-Warning 'Ruby-Prefix im PATH konnte nicht dekodiert werden.' }
        }
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

# Header, Laufzeitbibliothek und Architektur aus dem ausgewählten Ruby lesen.
function Get-RubyInfo {
    param([string]$Root)

    $rubyExe = Join-Path $Root 'bin\ruby.exe'
    if (-not (Test-Path $rubyExe)) { throw "ruby.exe fehlt: $rubyExe" }

    # Die gesamte Abfrage bleibt ASCII; Dateipfade werden in PowerShell erst
    # nach der Ausgabe aus Base64 als UTF-8 zurückgewandelt.
    $abfrage = "require 'rbconfig'; c = RbConfig::CONFIG; " +
               "print [c['rubyhdrdir'], c['rubyarchhdrdir'], c['libdir'], " +
               "c['LIBRUBYARG_SHARED'], c['RUBY_SO_NAME'], c['rubylibdir'], " +
               "c['rubyarchdir'], c['ruby_version'], c['DLEXT'], RUBY_PLATFORM]" +
               ".map { |v| [v.to_s.encode('UTF-8')].pack('m0') }.join('|')"
    $out = (& $rubyExe -e $abfrage) -join ''
    if ($LASTEXITCODE -ne 0 -or -not $out) {
        throw 'Ruby-Konfiguration konnte nicht ermittelt werden; Ruby-Installation und RUBYOPT prüfen.'
    }
    try {
        $t = @($out.Split('|') | ForEach-Object {
            [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($_))
        })
    } catch { throw 'Ruby-Konfiguration ist nicht lesbar.' }
    if ($t.Count -ne 10 -or @($t | Where-Object { [string]::IsNullOrWhiteSpace($_) }).Count -gt 0) {
        throw 'Ruby-Konfiguration ist unvollständig; es wird keine fremde Standardbibliothek geraten.'
    }
    return [pscustomobject]@{
        Exe          = $rubyExe
        HeaderDir    = $t[0]
        ArchDir      = $t[1]
        LibDir       = $t[2]
        LibArg       = $t[3]
        SoName       = $t[4]
        StdLibDir    = $t[5]
        StdArchDir   = $t[6]
        StdVersion   = $t[7]
        DlExt        = $t[8]
        Platform     = $t[9]
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

foreach ($p in @($Gcc, $Ruby.HeaderDir, $Ruby.ArchDir, $Ruby.LibDir, $Ruby.StdLibDir, $Ruby.StdArchDir)) {
    if (-not (Test-Path $p)) { throw "Fehlt: $p" }
}

# g++ braucht seine eigenen DLLs im PATH (cc1plus, libstdc++, libwinpthread).
$env:PATH = "$Toolchain\bin;$RubyRootGefunden\bin;$env:PATH"

$BuildDir = Join-Path $ProjectRoot 'build'
$ObjDir   = Join-Path $BuildDir 'obj'
$Exe      = Join-Path $BuildDir 'Rune3D.exe'
$EdExe    = Join-Path $BuildDir 'RuneEditor.exe'

# Die Ruby-DLL liegt neben den EXEs. Ihre Standardbibliothek muss daher zu
# dieser Kopie passen, nicht zu einem hart codierten Installationspfad.
# Nur die Standardbibliothek wird kopiert, keine fremden site_ruby-/Gem-Daten.
function Install-RubyRuntime {
    if ($Ruby.StdVersion -notmatch '^\d+\.\d+\.\d+$' -or
        $Ruby.Platform -notmatch '^[A-Za-z0-9_.-]+$' -or $Ruby.DlExt -notmatch '^[A-Za-z0-9]+$') {
        throw 'Ruby-Version oder Plattform für das Laufzeitpaket ist ungültig.'
    }
    if ($Ruby.DlExt -ne 'so') { throw 'Dieses Windows-Ruby benutzt keine unterstützten .so-Erweiterungen.' }
    $source = [IO.Path]::GetFullPath($Ruby.StdLibDir).TrimEnd([char[]]'\/')
    $arch = [IO.Path]::GetFullPath($Ruby.StdArchDir).TrimEnd([char[]]'\/')
    $expectedArch = [IO.Path]::GetFullPath((Join-Path $source $Ruby.Platform)).TrimEnd([char[]]'\/')
    if (-not [string]::Equals($arch, $expectedArch, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Ruby-Architektur liegt nicht unter der Standardbibliothek: $arch"
    }
    $dll = Join-Path $RubyRootGefunden "bin\$($Ruby.SoName).dll"
    if (-not (Test-Path -LiteralPath $dll -PathType Leaf)) { throw "Ruby-Laufzeit-DLL fehlt: $dll" }

    $libraryRoot = Join-Path $BuildDir 'lib\ruby'
    $destination = Join-Path $libraryRoot $Ruby.StdVersion
    $destFull = [IO.Path]::GetFullPath($destination).TrimEnd([char[]]'\/')
    if ($source.Equals($destFull, [StringComparison]::OrdinalIgnoreCase) -or
        $source.StartsWith($destFull + '\', [StringComparison]::OrdinalIgnoreCase) -or
        $destFull.StartsWith($source + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Ruby-Quelle und generierter Zielordner dürfen sich nicht überschneiden.'
    }
    New-Item -ItemType Directory -Force -Path $libraryRoot | Out-Null
    $stage = Join-Path $libraryRoot ('.rune-stage-' + [guid]::NewGuid().ToString('N'))
    $stagedVersion = Join-Path $stage $Ruby.StdVersion
    $markerName = '.rune-managed-runtime'
    New-Item -ItemType Directory -Path $stage | Out-Null
    try {
        Copy-Item -LiteralPath $source -Destination $stagedVersion -Recurse -Force
        $checks = @(
            (Join-Path $stagedVersion 'json.rb'),
            (Join-Path $stagedVersion 'fileutils.rb'),
            (Join-Path $stagedVersion "$($Ruby.Platform)\json\ext\parser.$($Ruby.DlExt)"),
            (Join-Path $stagedVersion "$($Ruby.Platform)\json\ext\generator.$($Ruby.DlExt)"),
            (Join-Path $stagedVersion "$($Ruby.Platform)\enc\encdb.$($Ruby.DlExt)")
        )
        foreach ($item in $checks) {
            if (-not (Test-Path -LiteralPath $item -PathType Leaf)) {
                throw "Ruby-Standardbibliothek unvollständig: $item"
            }
        }
        Set-Content -LiteralPath (Join-Path $stagedVersion $markerName) -Value 'Von build.ps1 erzeugt.' -Encoding Ascii
        $backup = $null
        if (Test-Path -LiteralPath $destination) {
            if (-not (Test-Path -LiteralPath (Join-Path $destination $markerName) -PathType Leaf)) {
                throw "Vorhandene Ruby-Bibliothek ist nicht als generiert markiert; nichts überschrieben: $destination"
            }
            $backup = Join-Path $libraryRoot ('.rune-old-' + [guid]::NewGuid().ToString('N'))
            Move-Item -LiteralPath $destination -Destination $backup
        }
        try {
            Move-Item -LiteralPath $stagedVersion -Destination $destination
        } catch {
            if ($backup -and -not (Test-Path -LiteralPath $destination)) {
                try { Move-Item -LiteralPath $backup -Destination $destination }
                catch { Write-Warning "Vorige Ruby-Bibliothek bleibt zur Wiederherstellung unter $backup" }
            }
            throw
        }
        if ($backup) {
            try { Remove-Item -LiteralPath $backup -Recurse -Force }
            catch { Write-Warning "Vorige Ruby-Bibliothek bleibt unter $backup" }
        }
    } finally {
        if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
    }

    $laufzeitDlls = @($dll, (Join-Path $Toolchain 'bin\libgmp-10.dll'),
                           (Join-Path $Toolchain 'bin\libwinpthread-1.dll'))
    foreach ($item in $laufzeitDlls) {
        if (-not (Test-Path -LiteralPath $item -PathType Leaf)) {
            Write-Warning "Laufzeit-DLL fehlt: $item (Doppelklick-Start könnte scheitern)"
            continue
        }
        Copy-Item -LiteralPath $item -Destination $BuildDir -Force
    }
    $builtin = Join-Path $RubyRootGefunden 'bin\ruby_builtin_dlls'
    if (Test-Path -LiteralPath $builtin -PathType Container) {
        $dst = Join-Path $BuildDir 'ruby_builtin_dlls'
        if (Test-Path -LiteralPath $dst) { Remove-Item -LiteralPath $dst -Recurse -Force }
        Copy-Item -LiteralPath $builtin -Destination $dst -Recurse -Force
    }
    Write-Host "Ruby-Standardbibliothek und Laufzeit-DLLs: build\lib\ruby\$($Ruby.StdVersion)" -ForegroundColor DarkGray
}

Write-Step "Ruby:      $RubyRootGefunden  ($($Ruby.SoName))"
Write-Step "Toolchain: $Toolchain"

# ------------------------------------------------------------ D3D11-WARP (optional)
# Software-Rendering mit verstecktem Fenster und eingebetteter Ruby-Brücke;
# kein interaktiver Hardware-/GUI-Test. Nur auf Wunsch ausführen.
if ($WarpTest) {
    New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
    $WarpExe = Join-Path $BuildDir 'renderer_warp_test.exe'
    Write-Step 'Baue D3D11-WARP-Test ...'
    $WarpFlags = @(
        '-std=c++20', '-O1', '-Wall', '-Wextra', '-Wno-unused-parameter',
        '-Wno-missing-field-initializers',
        '-DUNICODE', '-D_UNICODE', '-DNOMINMAX', '-DWIN32_LEAN_AND_MEAN',
        "-I$ProjectRoot\src",
        "$ProjectRoot\tools\renderer_warp_test.cpp",
        "$ProjectRoot\src\gfx\renderer.cpp",
        "$ProjectRoot\src\gfx\ui.cpp",
        '-o', $WarpExe, '-ld3d11', '-ldxgi', '-ld3dcompiler', '-ldxguid',
        '-luuid', '-luser32', '-lgdi32'
    )
    & $Gcc @WarpFlags
    if ($LASTEXITCODE -ne 0) { throw 'renderer_warp_test.exe konnte nicht gebaut werden' }
    & $WarpExe
    if ($LASTEXITCODE -ne 0) { throw 'D3D11-WARP-Test fehlgeschlagen' }

    # Echte CRuby-Bindings + derselbe WARP-Renderer; ohne Stubs, vollständig
    # getrennte Temp-Projekte. Kein vorausgebautes Rune3D.exe erforderlich.
    $EmbeddedExe = Join-Path $BuildDir 'embedded_texture_test.exe'
    Write-Step 'Baue und prüfe eingebettete Ruby-Texturbrücke ...'
    $EmbeddedFlags = @(
        '-std=c++20', '-O1', '-Wall', '-Wextra', '-Wno-unused-parameter',
        '-Wno-missing-field-initializers',
        '-DUNICODE', '-D_UNICODE', '-DNOMINMAX', '-DWIN32_LEAN_AND_MEAN',
        "-I$ProjectRoot\src", "-I$($Ruby.HeaderDir)", "-I$($Ruby.ArchDir)",
        "$ProjectRoot\tools\embedded_texture_test.cpp",
        "$ProjectRoot\src\app.cpp", "$ProjectRoot\src\ruby\vm.cpp",
        "$ProjectRoot\src\core\log.cpp", "$ProjectRoot\src\core\sound.cpp",
        "$ProjectRoot\src\core\window.cpp", "$ProjectRoot\src\gfx\camera.cpp",
        "$ProjectRoot\src\gfx\renderer.cpp", "$ProjectRoot\src\gfx\ui.cpp",
        "$ProjectRoot\src\world\physics.cpp", "$ProjectRoot\src\world\terrain.cpp",
        '-o', $EmbeddedExe,
        # UCRT muss wie beim eigentlichen Build vor der Ruby-Bibliothek stehen.
        '-lucrtbase', "-L$($Ruby.LibDir)", $Ruby.LibArg,
        '-ld3d11', '-ldxgi', '-ld3dcompiler', '-ldxguid', '-luuid',
        '-lole32', '-loleaut32', '-luxtheme', '-lcomctl32', '-luser32', '-lgdi32',
        '-ladvapi32', '-lshell32', '-lshlwapi', '-lwinmm', '-lws2_32',
        '-liphlpapi', '-lwindowscodecs', '-lcomdlg32',
        '-static-libgcc', '-static-libstdc++',
        '-Wl,-Bstatic,--whole-archive', '-lwinpthread', '-Wl,--no-whole-archive,-Bdynamic'
    )
    & $Gcc @EmbeddedFlags
    if ($LASTEXITCODE -ne 0) { throw 'embedded_texture_test.exe konnte nicht gebaut werden' }
    Install-RubyRuntime

    # Ein umgezogenes Paket mit Unicode-Pfad simuliert den Doppelklick auf
    # einem Rechner ohne Ruby-PATH. Es enthält nur die gebaute Laufzeit und
    # startet zunächst ohne RUBYLIB, danach mit einem fremden RUBYLIB.
    $temp = Join-Path ([IO.Path]::GetTempPath()) ('Rune-Ruby-ä-日-🚀-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $temp | Out-Null
    $names = @('PATH', 'RUBYLIB', 'RUBYOPT', 'RUBY_HOME', 'GEM_HOME', 'GEM_PATH',
               'BUNDLE_GEMFILE', 'RUNE_TEST_RUBYLIB')
    $saved = @{}
    foreach ($name in $names) { $saved[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
    $pushed = $false
    try {
        $copy = Join-Path $temp 'build'
        New-Item -ItemType Directory -Path $copy | Out-Null
        Copy-Item -LiteralPath $EmbeddedExe -Destination $copy
        Get-ChildItem -LiteralPath $BuildDir -Filter '*.dll' -File | ForEach-Object {
            Copy-Item -LiteralPath $_.FullName -Destination $copy
        }
        $builtin = Join-Path $BuildDir 'ruby_builtin_dlls'
        if (Test-Path -LiteralPath $builtin -PathType Container) {
            Copy-Item -LiteralPath $builtin -Destination $copy -Recurse
        }
        $copyRuby = Join-Path $copy 'lib\ruby'
        New-Item -ItemType Directory -Path $copyRuby -Force | Out-Null
        Copy-Item -LiteralPath (Join-Path $BuildDir "lib\ruby\$($Ruby.StdVersion)") -Destination $copyRuby -Recurse
        foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name, $null, 'Process') }
        $system = [Environment]::GetFolderPath('System')
        if (-not $system) { throw 'Windows-Systemverzeichnis für isolierten Test fehlt.' }
        $env:PATH = "$system;$env:SystemRoot"
        Push-Location $temp
        $pushed = $true
        $copyExe = Join-Path $copy 'embedded_texture_test.exe'
        Write-Step 'Prüfe kopierte Ruby-Laufzeit ohne Ruby-PATH und RUBYLIB ...'
        & $copyExe $ProjectRoot
        if ($LASTEXITCODE -ne 0) { throw 'Isolierte Ruby-Texturbrücke fehlgeschlagen' }

        $foreign = Join-Path $temp 'fremdes-rubylib'
        New-Item -ItemType Directory -Path $foreign | Out-Null
        Set-Content -LiteralPath (Join-Path $foreign 'json.rb') -Encoding Ascii -Value "raise 'falsches RUBYLIB verwendet'"
        $env:RUBYLIB = $foreign
        $env:RUNE_TEST_RUBYLIB = $foreign
        Write-Step 'Prüfe Vorrang der mitgelieferten Bibliothek vor fremdem RUBYLIB ...'
        & $copyExe $ProjectRoot --expect-foreign
        if ($LASTEXITCODE -ne 0) { throw 'Isolierte Ruby-Brücke mit fremdem RUBYLIB fehlgeschlagen' }

        # Fehlende Ruby- bzw. native Dateien dürfen nicht aus einem fremden
        # RUBYLIB ergänzt werden: nur diese Testkopie jeweils kurz verändern.
        $versionDir = Join-Path $copyRuby $Ruby.StdVersion
        $missingFiles = @((Join-Path $versionDir 'json.rb'),
                          (Join-Path $versionDir "$($Ruby.Platform)\json\ext\parser.$($Ruby.DlExt)"))
        foreach ($missing in $missingFiles) {
            $parked = $missing + '.rune-test'
            Move-Item -LiteralPath $missing -Destination $parked
            try {
                Write-Step "Prüfe Ablehnung bei fehlender Datei $(Split-Path $missing -Leaf) ..."
                & $copyExe $ProjectRoot --expect-incomplete
                if ($LASTEXITCODE -ne 0) { throw 'Unvollständiges Ruby-Paket wurde nicht sicher abgelehnt' }
            } finally {
                Move-Item -LiteralPath $parked -Destination $missing
            }
        }
    } finally {
        if ($pushed) { Pop-Location }
        foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name, $saved[$name], 'Process') }
        if (Test-Path -LiteralPath $temp) { Remove-Item -LiteralPath $temp -Recurse -Force }
    }
    if (-not $Test) { return }
}

# ------------------------------------------------------------ Tests (-Test)
# Bewusst VOR dem EXE-Bau: die Tests brauchen keine Grafik und laufen auch,
# wenn der Build gerade kaputt ist. tex_import_test.rb braucht allerdings
# build\RuneEditor.exe und wird sonst übersprungen (tools/test_all.rb).
if ($Test) {
    New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
    $JsonTestExe = Join-Path $BuildDir 'json_test.exe'

    Write-Step 'Baue C++-Logiktest (src/editor/json.cpp + tools/json_test.cpp) ...'
    # Argumente als Array und mit @ splatten: Im Argument-Modus von PowerShell
    # ist ein Komma KEIN Trenner, sondern bleibt am Token kleben (g++ bekommt
    # dann "-std=c++20," und der Linker sucht eine Datei dieses Namens).
    $JsonTestFlags = @(
        '-std=c++20', '-O1', '-Wall', '-Wextra', '-Wno-unused-parameter',
        "-I$ProjectRoot\src",
        "$ProjectRoot\src\editor\json.cpp",
        "$ProjectRoot\tools\json_test.cpp",
        '-o', $JsonTestExe, '-lcomctl32', '-luser32', '-lgdi32'
    )
    & $Gcc @JsonTestFlags
    if ($LASTEXITCODE -ne 0) { throw 'json_test.exe konnte nicht gebaut werden' }

    Write-Step 'C++-JSON-Test ...'
    & $JsonTestExe $ProjectRoot
    if ($LASTEXITCODE -ne 0) { throw 'C++-JSON-Test fehlgeschlagen' }

    Write-Step 'Script- und Karten-Arbeitsabläufe portabel prüfen ...'
    $WorkflowTestExe = Join-Path $BuildDir 'editor_workflow_test.exe'
    $WorkflowFlags = @(
        '-std=c++20', '-O1', '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter',
        "-I$ProjectRoot\src", "$ProjectRoot\src\editor\json.cpp",
        "$ProjectRoot\tools\editor_workflow_test.cpp", '-o', $WorkflowTestExe
    )
    & $Gcc @WorkflowFlags
    if ($LASTEXITCODE -ne 0) { throw 'editor_workflow_test.exe konnte nicht gebaut werden' }
    & $WorkflowTestExe
    if ($LASTEXITCODE -ne 0) { throw 'Editor-Arbeitsablauftest fehlgeschlagen' }

    Write-Step 'Echtes Terrain kopflos bauen und prüfen ...'
    $TerrainTestExe = Join-Path $BuildDir 'terrain_test.exe'
    $TerrainTestFlags = @(
        '-std=c++20', '-O1', '-Wall', '-Wextra', '-Wno-unused-parameter',
        "-I$ProjectRoot\src",
        "$ProjectRoot\src\world\terrain.cpp",
        "$ProjectRoot\tools\terrain_test.cpp",
        '-o', $TerrainTestExe
    )
    & $Gcc @TerrainTestFlags
    if ($LASTEXITCODE -ne 0) { throw 'terrain_test.exe konnte nicht gebaut werden' }
    & $TerrainTestExe
    if ($LASTEXITCODE -ne 0) { throw 'C++-Terrain-Test fehlgeschlagen' }

    Write-Step 'WAV-Leser und Pitch prüfen (Etappe 21) ...'
    $WavTestExe = Join-Path $BuildDir 'wav_test.exe'
    $WavTestFlags = @(
        '-std=c++20', '-O1', '-Wall', '-Wextra', '-Werror',
        "-I$ProjectRoot\src", "$ProjectRoot\tools\wav_test.cpp", '-o', $WavTestExe
    )
    & $Gcc @WavTestFlags
    if ($LASTEXITCODE -ne 0) { throw 'wav_test.exe konnte nicht gebaut werden' }
    & $WavTestExe
    if ($LASTEXITCODE -ne 0) { throw 'WAV-Test fehlgeschlagen' }

    Write-Step 'Materialdokument und Windows-Dateihandles prüfen ...'
    $MaterialTestExe = Join-Path $BuildDir 'material_test.exe'
    $MaterialTestFlags = @(
        '-std=c++20', '-O1', '-Wall', '-Wextra', '-Wno-unused-parameter',
        '-Wno-missing-field-initializers',
        '-DUNICODE', '-D_UNICODE', '-DNOMINMAX', '-DWIN32_LEAN_AND_MEAN',
        "-I$ProjectRoot\src",
        "$ProjectRoot\src\editor\json.cpp",
        "$ProjectRoot\tools\material_test.cpp",
        '-o', $MaterialTestExe
    )
    & $Gcc @MaterialTestFlags
    if ($LASTEXITCODE -ne 0) { throw 'material_test.exe konnte nicht gebaut werden' }
    & $MaterialTestExe
    if ($LASTEXITCODE -ne 0) { throw 'Material-/Dateitest fehlgeschlagen' }

    Write-Step 'Karten-/F9-Dateitransaktionen und Konfliktrennen prüfen ...'
    $DocumentTestExe = Join-Path $BuildDir 'document_store_test.exe'
    $DocumentTestFlags = @(
        '-std=c++20', '-O1', '-Wall', '-Wextra', '-Wno-unused-parameter',
        '-Wno-missing-field-initializers',
        '-DUNICODE', '-D_UNICODE', '-DNOMINMAX', '-DWIN32_LEAN_AND_MEAN',
        "-I$ProjectRoot\src", "$ProjectRoot\tools\document_store_test.cpp",
        '-o', $DocumentTestExe, '-luser32', '-lgdi32',
        '-static-libgcc', '-static-libstdc++'
    )
    & $Gcc @DocumentTestFlags
    if ($LASTEXITCODE -ne 0) { throw 'document_store_test.exe konnte nicht gebaut werden' }
    & $DocumentTestExe
    if ($LASTEXITCODE -ne 0) { throw 'Karten-/F9-Dateitransaktionen fehlgeschlagen' }

    Write-Step 'Windows-Projektpfade und Playtest-Kommando prüfen ...'
    $PathTestExe = Join-Path $BuildDir 'windows_path_test.exe'
    $PathTestFlags = @(
        '-std=c++20', '-O1', '-Wall', '-Wextra', '-Wno-unused-parameter',
        '-Wno-missing-field-initializers',
        '-DUNICODE', '-D_UNICODE', '-DNOMINMAX', '-DWIN32_LEAN_AND_MEAN',
        "-I$ProjectRoot\src", "$ProjectRoot\src\editor\json.cpp",
        "$ProjectRoot\tools\windows_path_test.cpp",
        '-o', $PathTestExe, '-lshell32', '-luser32', '-lgdi32',
        '-static-libgcc', '-static-libstdc++'
    )
    & $Gcc @PathTestFlags
    if ($LASTEXITCODE -ne 0) { throw 'windows_path_test.exe konnte nicht gebaut werden' }
    & $PathTestExe
    if ($LASTEXITCODE -ne 0) { throw 'Windows-Pfadtest fehlgeschlagen' }

    # Der ausführbare UI-Smoke benutzt später die isolierte Artefaktkopie,
    # nicht den Quell-Projektordner. Er wird im Smoke-Skript gestartet.
    Write-Step 'Windows-Editor-UI-Smoke vorbereiten ...'
    $UiSmokeExe = Join-Path $BuildDir 'editor_ui_smoke.exe'
    $UiSmokeFlags = @(
        '-std=c++20', '-O1', '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter',
        '-DUNICODE', '-D_UNICODE', '-DNOMINMAX', '-DWIN32_LEAN_AND_MEAN',
        "-I$ProjectRoot\src", "$ProjectRoot\src\editor\json.cpp",
        "$ProjectRoot\tools\editor_ui_smoke.cpp", '-o', $UiSmokeExe,
        '-static', '-lshell32', '-luser32'
    )
    & $Gcc @UiSmokeFlags
    if ($LASTEXITCODE -ne 0) { throw 'editor_ui_smoke.exe konnte nicht gebaut werden' }

    Write-Step 'Ruby-Testsuite (tools/test_all.rb) ...'
    $SuiteArgs = @((Join-Path $ProjectRoot 'tools\test_all.rb'))
    if ($Clean -or (Test-Path $EdExe)) { $SuiteArgs += '--strict' }   # mit EXE muss auch tex_import laufen
    & $Ruby.Exe @SuiteArgs
    if ($LASTEXITCODE -ne 0) { throw 'Ruby-Testsuite fehlgeschlagen' }

    Write-Step 'JSON-Brücke (C++-Schreiber gegen Ruby JSON.pretty_generate) ...'
    $BrueckeArgs = @((Join-Path $ProjectRoot 'tools\json_bridge_check.rb'), $JsonTestExe, '--strict-format')
    & $Ruby.Exe @BrueckeArgs
    if ($LASTEXITCODE -ne 0) { throw 'JSON-Brückenprüfung fehlgeschlagen' }

    Write-Step 'Event-Brücke (C++-Edits gegen Ruby-Laufzeit) ...'
    & $Ruby.Exe (Join-Path $ProjectRoot 'tools\event_bridge_check.rb') $JsonTestExe
    if ($LASTEXITCODE -ne 0) { throw 'Event-Brückenprüfung fehlgeschlagen' }

    Write-Host 'ALLE TESTS GRÜN' -ForegroundColor Green
    return
}

# Zwei Ziele: Rune3D.exe (Spiel, main.cpp) und RuneEditor.exe (Scene-Editor,
# eigenes Programm ohne Game-Engine, editor_main.cpp). Alle anderen Quellen
# sind gemeinsam (Renderer, Terrain, Mathe, Log, Ruby-Bindings).
$GameExcludes    = @('editor_main.cpp', 'json.cpp')
$EditorExcludes  = @('main.cpp')

if ($Clean -and (Test-Path $BuildDir)) {
    Remove-Item $BuildDir -Recurse -Force
    Write-Host 'build/ gelöscht' -ForegroundColor DarkGray
}
New-Item -ItemType Directory -Force -Path $ObjDir | Out-Null

# --whole-archive zieht andernfalls das VERSIONINFO-Objekt von winpthreads
# mit: Explorer nennt die EXE dann "POSIX WinThreads for Windows". Nur eine
# temporäre Kopie des statischen Archivs bearbeiten, NIE die Toolchain.
$Ar = Join-Path $Toolchain 'bin\ar.exe'
$Windres = Join-Path $Toolchain 'bin\windres.exe'
$Objdump = Join-Path $Toolchain 'bin\objdump.exe'
foreach ($tool in @($Ar, $Windres, $Objdump)) {
    if (-not (Test-Path -LiteralPath $tool -PathType Leaf)) { throw "Build-Werkzeug fehlt: $tool" }
}
$archiveSource = (& $Gcc '-print-file-name=libwinpthread.a' | Select-Object -First 1)
if ($LASTEXITCODE -ne 0 -or -not $archiveSource -or -not (Test-Path -LiteralPath $archiveSource -PathType Leaf)) {
    throw "Statisches winpthread-Archiv nicht gefunden: $archiveSource"
}
$WinThreadArchive = Join-Path $ObjDir 'rune-libwinpthread.a'
Copy-Item -LiteralPath $archiveSource -Destination $WinThreadArchive -Force
$members = @(& $Ar t $WinThreadArchive)
if ($LASTEXITCODE -ne 0) { throw 'winpthread-Archiv kann nicht gelesen werden' }
$resourceMembers = @($members | Where-Object { $_ -match '(^|[\\/])version\.o$' })
if ($resourceMembers.Count -gt 0) {
    & $Ar d $WinThreadArchive @resourceMembers
    if ($LASTEXITCODE -ne 0) { throw 'winpthread-Versionseintrag konnte nicht aus der Kopie entfernt werden' }
}

$CxxFlags = @(
    '-std=c++20', '-O2', '-Wall', '-Wextra', '-Wno-unused-parameter',
    '-Wno-missing-field-initializers',
    '-DUNICODE', '-D_UNICODE', '-DNOMINMAX', '-DWIN32_LEAN_AND_MEAN',
    "-I$ProjectRoot\src",
    "-I$($Ruby.HeaderDir)", "-I$($Ruby.ArchDir)"
)

$Libs = @(
    # Wichtig: UCRT VOR der Ruby-Importbibliothek. Die Ruby-DLL exportiert
    # eigene CRT-Symbole (fclose, fwrite, ...); ohne -lucrtbase würden unsere
    # stdio-Aufrufe auf die Ruby-Exporte umgebogen und beim Schliessen von
    # Dateien mit Access Violation abstürzen.
    '-lucrtbase',
    "-L$($Ruby.LibDir)", $Ruby.LibArg,
    '-ld3d11', '-ldxgi', '-ld3dcompiler', '-ldxguid',
    '-luuid', '-lole32', '-loleaut32', '-luxtheme', '-lcomctl32',
    '-luser32', '-lgdi32', '-ladvapi32', '-lshell32', '-lshlwapi',
    '-lwinmm', '-lws2_32', '-liphlpapi', '-lwindowscodecs', '-lcomdlg32',
    # libwinpthread statisch einlinken: Der g++-Driver hängt HINTER den
    # Nutzer-Flags noch -lpthread (dynamisch) an. --whole-archive erzwingt
    # die statische libwinpthread.a komplett, damit die späteren Treffer
    # nicht mehr an die DLL gehen. Dynamisch würde jede EXE
    # libwinpthread-1.dll aus der MSYS2-Toolchain im PATH verlangen
    # (Doppelklick ohne Ruby-PATH -> "Systemfehler").
    '-static-libgcc', '-static-libstdc++',
    '-Wl,-Bstatic,--whole-archive', $WinThreadArchive, '-Wl,--no-whole-archive,-Bdynamic'
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

    Write-Host "  [$name] $Built kompiliert, $Skipped übersprungen" -ForegroundColor DarkGray
    Write-Host "  linke $(Split-Path $outExe -Leaf)" -ForegroundColor DarkGray
    $rc = if ($name -eq 'editor') { Join-Path $ProjectRoot 'src\editor\RuneEditor.rc' }
          else { Join-Path $ProjectRoot 'src\Rune3D.rc' }
    $resource = Join-Path $ObjDir ($name + '_version.o')
    & $Windres -i $rc -o $resource -O coff
    if ($LASTEXITCODE -ne 0) { throw "Versionsressource fehlgeschlagen: $rc" }
    # Die Programmressource kommt vor Bibliotheken und hat Vorrang.
    & $Gcc $resource @Objects @Libs -o $outExe
    if ($LASTEXITCODE -ne 0) { throw 'Linken fehlgeschlagen' }
    $imports = @(& $Objdump -p $outExe)
    if ($LASTEXITCODE -ne 0) { throw 'EXE-Imports konnten nicht geprüft werden' }
    if (@($imports | Where-Object { $_ -match '(?i)DLL Name:\s*libwinpthread-1\.dll' }).Count -gt 0) {
        throw 'winpthread wurde unerwartet dynamisch in die EXE gelinkt'
    }

    $size = [math]::Round((Get-Item $outExe).Length / 1KB, 1)
    Write-Host "OK: $outExe ($size KB)" -ForegroundColor Green
}

Build-Target 'game'   $GameExcludes   $Exe
Build-Target 'editor' $EditorExcludes $EdExe

# Manifest für moderne Windows-Steuerelemente neben den Editor legen.
Copy-Item (Join-Path $ProjectRoot 'src\editor\RuneEditor.exe.manifest') $BuildDir -Force

# Ruby-DLLs und die passende Standardbibliothek neben den EXEs bereitstellen.
Install-RubyRuntime

if ($Run) {
    Write-Host '  starte Rune3D ...' -ForegroundColor DarkGray
    Push-Location $ProjectRoot
    try {
        & $Exe
    } finally {
        Pop-Location
    }
}
