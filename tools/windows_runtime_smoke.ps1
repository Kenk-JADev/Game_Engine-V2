#requires -Version 5.1
<#
    Startet die echten Editor- und Spiel-EXEs aus einer neuen temporären
    Unicode-Paketkopie, ohne Ruby im PATH oder RUBYLIB. Die EXEs laufen normal
    bis in ihre Hauptschleife, nur die Grafik nutzt den WARP-Software-Renderer.
    Keine Dateien der Quellinstallation oder ihrer Projekte werden verändert.

    .\build.ps1 -Clean
    .\tools\windows_runtime_smoke.ps1
    # Ein vollständig entpacktes Artefakt außerhalb des Repositorys prüfen:
    .\tools\windows_runtime_smoke.ps1 -PackagePath D:\RunePaket
#>
[CmdletBinding()]
param([string]$PackagePath, [switch]$RequirePackage)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if ($PackagePath) { $build = (Resolve-Path -LiteralPath $PackagePath).Path }
else { $build = Join-Path $root 'build' }
foreach ($relative in @('RuneEditor.exe', 'Rune3D.exe', 'lib\ruby')) {
    $path = Join-Path $build $relative
    if (-not (Test-Path -LiteralPath $path)) { throw "Build unvollständig: $path" }
}

# Die EXE trägt ihren eigenen Namen, nicht die VERSIONINFO von winpthreads.
foreach ($expect in @(@('RuneEditor.exe', 'Rune3D Editor'), @('Rune3D.exe', 'Rune3D Spiel'))) {
    $info = (Get-Item -LiteralPath (Join-Path $build $expect[0])).VersionInfo
    if ($info.FileDescription -ne $expect[1] -or $info.OriginalFilename -ne $expect[0] -or $info.ProductName -ne 'Rune3D') {
        throw "Falsche EXE-Metadaten für $($expect[0]): $($info.FileDescription) / $($info.OriginalFilename)"
    }
    Write-Host "Versionsinfo OK: $($expect[0]) — $($info.FileDescription)"
}

$game = Join-Path $build 'game'
$projects = Join-Path $build 'projekte'
if (-not ((Test-Path -LiteralPath $game -PathType Container) -and
          (Test-Path -LiteralPath $projects -PathType Container))) {
    if ($RequirePackage -or $PackagePath) { throw 'Artefakt muss game und projekte neben den EXEs enthalten.' }
    $game = Join-Path $root 'game'
    $projects = Join-Path $root 'projekte'
}
foreach ($path in @($game, $projects)) {
    if (-not (Test-Path -LiteralPath $path -PathType Container)) { throw "Laufzeitdaten fehlen: $path" }
}

# Nur eindeutig neu angelegte, eigene Dateien werden später wieder entfernt.
$temp = Join-Path ([IO.Path]::GetTempPath()) ('Rune-Start-ä-日-🚀-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $temp | Out-Null
$names = @('PATH', 'RUBYLIB', 'RUBYOPT', 'RUBY_HOME', 'GEM_HOME', 'GEM_PATH',
           'BUNDLE_GEMFILE', 'RUNE_PROJECT', 'RUNE_PLAYTEST_MAP', 'RUNE_D3D_WARP')
$saved = @{}
foreach ($name in $names) { $saved[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }

# Der Prozess schreibt das Log noch. Das lesende Handle muss auch
# gleichzeitige Schreibzugriffe erlauben (FileShare.ReadWrite).
function Read-SharedLog {
    param([string]$Path)
    $stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
    try {
        $reader = [IO.StreamReader]::new($stream, [Text.Encoding]::UTF8, $true)
        try { return $reader.ReadToEnd() }
        finally { $reader.Dispose() }
    } finally { $stream.Dispose() }
}

function Test-ProgrammStart {
    param([string]$Exe, [string]$Log, [string[]]$Erwartet, [string]$Startordner)

    Write-Host "Starte $([IO.Path]::GetFileName($Exe)) aus der isolierten Paketkopie ..."
    $process = Start-Process -FilePath $Exe -WorkingDirectory $Startordner -PassThru
    $text = ''
    try {
        $deadline = [DateTime]::UtcNow.AddSeconds(90)
        $fertig = $false
        do {
            if (Test-Path -LiteralPath $Log -PathType Leaf) {
                $text = (Read-SharedLog $Log)
                $fertig = @($Erwartet | Where-Object { -not $text.Contains($_) }).Count -eq 0
            }
            $process.Refresh()
            if ($process.HasExited) { throw "Programm vor der Hauptschleife beendet (Exit $($process.ExitCode))." }
            if (-not $fertig) { [void]$process.WaitForExit(250) }
        } while (-not $fertig -and [DateTime]::UtcNow -lt $deadline)
        if (-not $fertig) { throw 'Startmeldung innerhalb von 90 Sekunden nicht erreicht.' }

        # Mindestens ein kurzer Lauf nach dem Bootstrap; Ruby-Tickfehler dürfen
        # nicht durch eine sofortige Beendigung des Tests verdeckt werden.
        if ($process.WaitForExit(2000)) { throw "Programm nach dem Start beendet (Exit $($process.ExitCode))." }
        $text = (Read-SharedLog $Log)
        if ($text -match 'Ruby-Fehler|LoadError|Bootstrap fehlgeschlagen|Ruby konnte nicht gestartet') {
            throw 'Ruby- oder Bootstrap-Fehler im Startprotokoll.'
        }
        Write-Host "  OK: Ruby-Boot und Hauptschleife erreicht ($([IO.Path]::GetFileName($Exe)))" -ForegroundColor Green
    } catch {
        if (Test-Path -LiteralPath $Log -PathType Leaf) {
            Write-Host '--- Startprotokoll ---'
            Write-Host (Read-SharedLog $Log)
            Write-Host '--- Ende Startprotokoll ---'
        }
        throw
    } finally {
        $process.Refresh()
        if (-not $process.HasExited) {
            # Bei unerwarteten Dialogen nicht unbegrenzt auf CI warten.
            [void]$process.CloseMainWindow()
            if (-not $process.WaitForExit(10000)) {
                $process.Kill()
                [void]$process.WaitForExit(10000)
            }
        }
        $process.Dispose()
    }
}

try {
    $copy = Join-Path $temp 'Paket'
    $startordner = Join-Path $temp 'anderer-Startordner'
    New-Item -ItemType Directory -Path $copy, $startordner | Out-Null
    foreach ($relative in @('RuneEditor.exe', 'Rune3D.exe', 'RuneEditor.exe.manifest')) {
        $path = Join-Path $build $relative
        if (Test-Path -LiteralPath $path -PathType Leaf) { Copy-Item -LiteralPath $path -Destination $copy }
    }
    Get-ChildItem -LiteralPath $build -Filter '*.dll' -File | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $copy
    }
    $builtin = Join-Path $build 'ruby_builtin_dlls'
    if (Test-Path -LiteralPath $builtin -PathType Container) {
        Copy-Item -LiteralPath $builtin -Destination $copy -Recurse
    }
    $lib = Join-Path $copy 'lib'
    New-Item -ItemType Directory -Path $lib | Out-Null
    Copy-Item -LiteralPath (Join-Path $build 'lib\ruby') -Destination $lib -Recurse
    Copy-Item -LiteralPath $game -Destination (Join-Path $copy 'game') -Recurse
    Copy-Item -LiteralPath $projects -Destination (Join-Path $copy 'projekte') -Recurse

    foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name, $null, 'Process') }
    $system = [Environment]::GetFolderPath('System')
    if (-not $system) { throw 'Windows-Systemverzeichnis für isolierten Test fehlt.' }
    $env:PATH = "$system;$env:SystemRoot"
    $env:RUNE_D3D_WARP = '1'

    $common = @('D3D11: WARP-Software-Renderer', 'Ruby-VM: mitgelieferte Standardbibliothek', 'Ruby-VM gestartet')
    Test-ProgrammStart (Join-Path $copy 'RuneEditor.exe') (Join-Path $copy 'log\editor.log') `
        ($common + @('Editor-Texturen und Sounds registriert', 'Editor-Hauptschleife läuft')) $startordner
    Test-ProgrammStart (Join-Path $copy 'Rune3D.exe') (Join-Path $copy 'log\engine.log') `
        ($common + @('Bootstrap fertig, lade game/main.rb', 'Hauptschleife läuft')) $startordner
    Write-Host 'ISOLIERTER WINDOWS-STARTTEST OK (Editor und Spiel)' -ForegroundColor Green
} finally {
    foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name, $saved[$name], 'Process') }
    if (Test-Path -LiteralPath $temp) { Remove-Item -LiteralPath $temp -Recurse -Force }
}
