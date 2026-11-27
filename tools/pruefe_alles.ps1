#requires -Version 5.1
<#
    Auto-Discovery-Prueflauf der Event-Command-Schicht.

    Neu eingebaute Dateien muessen nicht mehr in eine Liste eingetragen werden:

      tools/*_test.rb     wird von tools/test_all.rb per Glob gefunden
      tools/lint_*.rb     wird hier per Glob gefunden und ausgefuehrt
      tools/*_smoke.ps1   wird hier per Glob gefunden und ausgefuehrt
      src/**/*.cpp        wird von build.ps1 automatisch kompiliert

    Alle Schritte laufen auch nach einem Fehler weiter. Die Fehler werden
    gesammelt, am Ende gemeinsam angezeigt und in eine Logdatei geschrieben.
    Nur bei Fehlern wartet das Skript vor dem Schliessen auf ENTER.

    Das Skript startet sich bei Bedarf selbst als Administrator neu, damit
    Schritte mit eingeschraenkten Dateirechten nicht an Permission Denied
    scheitern. Die UAC-Abfrage erscheint einmal beim Start.

        .\tools\pruefe_alles.ps1

    Von cmd aus:

        powershell -ExecutionPolicy Bypass -File .\tools\pruefe_alles.ps1
#>
[CmdletBinding()]
param()

function Test-Administrator {
    $identitaet = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identitaet)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

if (-not (Test-Administrator)) {
    Write-Host 'Administrationsrechte werden benoetigt; starte neu als Administrator ...' -ForegroundColor Yellow
    try {
        $argumente = '-NoProfile -ExecutionPolicy Bypass -File "' + $PSCommandPath + '"'
        $shell = Join-Path $PSHOME 'powershell.exe'
        if (-not (Test-Path -LiteralPath $shell)) {
            $shell = (Get-Command powershell.exe -ErrorAction Stop).Source
        }
        $prozess = Start-Process -FilePath $shell -ArgumentList $argumente `
                                 -Verb RunAs -PassThru -Wait
        exit $prozess.ExitCode
    }
    catch {
        Write-Host "FEHLER: Neustart als Administrator fehlgeschlagen: $($_.Exception.Message)" -ForegroundColor Red
        Read-Host 'Druecke ENTER zum Beenden oder schliesse das Fenster'
        exit 1
    }
}

$root = Split-Path -Parent $PSScriptRoot
$werkzeuge = Join-Path $root 'tools'
$logVerzeichnis = Join-Path $root 'log'
if (-not (Test-Path -LiteralPath $logVerzeichnis)) {
    New-Item -ItemType Directory -Path $logVerzeichnis | Out-Null
}
$logPfad = Join-Path $logVerzeichnis ('pruefe_alles_{0}.log' -f (Get-Date -Format 'yyyyMMdd-HHmmss'))
$fehler = [System.Collections.Generic.List[string]]::new()

function Schreibe-Zeile {
    param(
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$Text,
        [string]$Farbe
    )

    if ($Farbe) { Write-Host $Text -ForegroundColor $Farbe } else { Write-Host $Text }
    Add-Content -LiteralPath $logPfad -Value $Text -Encoding UTF8
}

function Invoke-Schritt {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][scriptblock]$Aktion
    )

    Schreibe-Zeile "==> $Name" 'Cyan'
    $global:LASTEXITCODE = 0
    try {
        & $Aktion 2>&1 | ForEach-Object { Schreibe-Zeile ([string]$_) }
        $code = $global:LASTEXITCODE
        if ($code -ne 0) {
            $meldung = "Schritt '$Name' endete mit Code $code"
            $fehler.Add($meldung)
            Schreibe-Zeile "FEHLER: $meldung" 'Red'
        }
    }
    catch {
        $meldung = "Schritt '$Name': $($_.Exception.Message)"
        $fehler.Add($meldung)
        Schreibe-Zeile "FEHLER: $meldung" 'Red'
    }
}

# Der elevated Prozess hat nicht zwingend den Ruby-Pfad des Benutzers in PATH.
# Dieselbe Reihenfolge wie build.ps1: PATH, RUNE_RUBY_ROOT, bekannte Root-Ordner.
function Resolve-Ruby {
    $imPfad = Get-Command ruby.exe -ErrorAction SilentlyContinue
    if ($imPfad) { return $imPfad.Source }

    $kandidaten = @()
    if ($env:RUNE_RUBY_ROOT) { $kandidaten += (Join-Path $env:RUNE_RUBY_ROOT 'bin\ruby.exe') }
    $kandidaten += @('C:\Ruby34-x64', 'C:\Ruby33-x64', 'C:\Ruby32-x64') |
                   ForEach-Object { Join-Path $_ 'bin\ruby.exe' }
    foreach ($kandidat in $kandidaten) {
        if (Test-Path -LiteralPath $kandidat -PathType Leaf) { return $kandidat }
    }
    throw "Kein Ruby gefunden. Gesucht: PATH, RUNE_RUBY_ROOT, $($kandidaten -join ', ')"
}

Schreibe-Zeile "Prueflauf $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" 'Cyan'
Schreibe-Zeile "Logdatei: $logPfad" 'DarkGray'
Schreibe-Zeile ''

Push-Location -LiteralPath $root
try {
    # --- Ruby-Aufloesung ----------------------------------------------------
    $rubyExe = $null
    try {
        $rubyExe = Resolve-Ruby
        Schreibe-Zeile "Ruby: $rubyExe" 'DarkGray'
    }
    catch {
        $meldung = "Ruby nicht gefunden: $($_.Exception.Message)"
        $fehler.Add($meldung)
        Schreibe-Zeile "FEHLER: $meldung" 'Red'
    }

    # --- Entdeckung ---------------------------------------------------------
    $tests = @()
    if ($rubyExe) {
        $global:LASTEXITCODE = 0
        try {
            $ausgabe = @(& $rubyExe tools/test_all.rb --list 2>&1 | ForEach-Object { [string]$_ })
            if ($global:LASTEXITCODE -ne 0) {
                $meldung = "Entdeckung der Ruby-Tests endete mit Code $($global:LASTEXITCODE)"
                $fehler.Add($meldung)
                Schreibe-Zeile "FEHLER: $meldung" 'Red'
            }
            $tests = @($ausgabe | ForEach-Object { $_.Trim() } | Where-Object { $_ })
        }
        catch {
            $meldung = "Entdeckung der Ruby-Tests: $($_.Exception.Message)"
            $fehler.Add($meldung)
            Schreibe-Zeile "FEHLER: $meldung" 'Red'
        }
    }

    $lints = @(Get-ChildItem -LiteralPath $werkzeuge -Filter 'lint_*.rb' -File | Sort-Object Name)
    $smokes = @(Get-ChildItem -LiteralPath $werkzeuge -Filter '*_smoke.ps1' -File | Sort-Object Name)

    Schreibe-Zeile "Gefundene Ruby-Tests ($($tests.Count)):" 'DarkGray'
    foreach ($test in $tests) { Schreibe-Zeile "  - $test" 'DarkGray' }
    Schreibe-Zeile "Gefundene Lint-Skripte ($($lints.Count)):" 'DarkGray'
    foreach ($datei in $lints) { Schreibe-Zeile "  - $($datei.Name)" 'DarkGray' }
    Schreibe-Zeile "Gefundene Smoke-Skripte ($($smokes.Count)):" 'DarkGray'
    foreach ($datei in $smokes) { Schreibe-Zeile "  - $($datei.Name)" 'DarkGray' }
    Schreibe-Zeile ''

    # --- Ausfuehrung --------------------------------------------------------
    Invoke-Schritt 'build.ps1 (src/**/*.cpp automatisch)' {
        & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $root 'build.ps1')
    }
    if ($rubyExe) {
        Invoke-Schritt 'test_all.rb --strict (tools/*_test.rb automatisch)' {
            & $rubyExe tools/test_all.rb --strict
        }
        foreach ($datei in $lints) {
            Invoke-Schritt "lint: $($datei.Name)" {
                & $rubyExe -I game/engine "tools/$($datei.Name)"
            }
        }
    }
    else {
        Schreibe-Zeile 'Ruby-Tests und Lint uebersprungen (Ruby nicht gefunden).' 'DarkGray'
    }
    foreach ($datei in $smokes) {
        Invoke-Schritt "smoke: $($datei.Name)" {
            & powershell -NoProfile -ExecutionPolicy Bypass -File $datei.FullName
        }
    }

    # --- Ergebnis -----------------------------------------------------------
    if ($fehler.Count -gt 0) {
        Schreibe-Zeile ''
        Schreibe-Zeile "$($fehler.Count) Fehler im Prueflauf:" 'Red'
        foreach ($eintrag in $fehler) {
            Schreibe-Zeile "- $eintrag" 'Red'
        }
        Schreibe-Zeile "Alle Fehler stehen in $logPfad" 'Red'
        Read-Host 'Druecke ENTER zum Beenden oder schliesse das Fenster'
        exit 1
    }

    Schreibe-Zeile 'Alle gefundenen Pruefungen erfolgreich.' 'Green'
    Schreibe-Zeile "Logdatei: $logPfad" 'DarkGray'
}
finally {
    Pop-Location
}

exit 0
