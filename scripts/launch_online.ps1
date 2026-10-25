param([string]$DependencyRoot, [string]$ServerUrl, [switch]$CheckOnly, [switch]$SmokeTest)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'online_environment.ps1')
$root = Split-Path $PSScriptRoot -Parent
$deps = Get-OnlineDependencyRoot $DependencyRoot
$app = Join-Path $root 'out\bin'
$exe = Join-Path $app 'tnp2_app.exe'
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw 'Run scripts/build_online.ps1 first.' }
foreach ($dir in @('logs', 'data')) {
    New-Item -ItemType Directory -Path (Join-Path $app $dir) -Force | Out-Null
}
$serverConfig = Join-Path $app 'httpserver.ini'
if ($ServerUrl) {
    $parsedUrl = [uri]$ServerUrl
    if (-not $parsedUrl.IsAbsoluteUri -or $parsedUrl.Scheme -notin @('http', 'https') -or $parsedUrl.UserInfo -or $parsedUrl.Query -or $parsedUrl.Fragment) {
        throw 'ServerUrl must be an HTTP(S) server URL without credentials, query or fragment.'
    }
    [IO.File]::WriteAllText($serverConfig, "[Config]`r`nHostName=$($ServerUrl.TrimEnd('/'))`r`n")
} elseif (-not (Test-Path -LiteralPath $serverConfig)) {
    Copy-Item -LiteralPath (Join-Path $root 'config\httpserver.ini.example') -Destination $serverConfig
}
$hostLine = Get-Content -LiteralPath $serverConfig -Encoding UTF8 | Where-Object { $_ -match '^\s*HostName\s*=' } | Select-Object -First 1
$hostName = ($hostLine -replace '^\s*HostName\s*=\s*', '').Trim()
if (-not $hostName -or $hostName -match 'offline\.invalid') { throw 'Configure the online API in out/bin/httpserver.ini.' }
Copy-Item -LiteralPath (Join-Path $root 'config\log4qt.properties') -Destination (Join-Path $app 'log4qt.properties') -Force
$gslDirectories = @(
    $env:TNP_GSL_BIN, "$deps\qgis-ltr-dev\bin", "$deps\gdal-dev\bin",
    (Join-Path (Split-Path $root -Parent) 'tnpsource_developmentKit\transnetproCode\out\bin\algorithm\ctap')
)
foreach ($dll in @('gsl.dll', 'cblas.dll')) {
    $destination = Join-Path $app $dll
    if (Test-Path -LiteralPath $destination -PathType Leaf) { continue }
    $source = $null
    foreach ($directory in $gslDirectories) {
        if ($directory -and (Test-Path -LiteralPath (Join-Path $directory $dll) -PathType Leaf)) {
            $source = Join-Path $directory $dll
            break
        }
    }
    if (-not $source) { throw "Missing $dll required by QGIS Analysis. Set TNP_GSL_BIN to the matching x64 GSL runtime directory." }
    Copy-Item -LiteralPath $source -Destination $destination
}
Set-OnlineEnvironment $deps
foreach ($required in @(
    "$deps\Qt5\plugins\platforms\qwindows.dll",
    "$deps\qgis-ltr-dev\plugins\provider_wfs.dll",
    "$deps\gdal-dev\share\proj\proj.db", "$deps\WebView2\WebView2Loader.dll"
)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Missing runtime dependency: $required" }
}
if ($CheckOnly) { Write-Output "READY: $exe"; Write-Output "ONLINE API: $hostName"; return }
if ($SmokeTest) {
    $env:QT_QPA_PLATFORM = 'offscreen'
    $stdout = Join-Path $app 'logs\startup-stdout.txt'
    $stderr = Join-Path $app 'logs\startup-stderr.txt'
    $process = Start-Process -FilePath $exe -WorkingDirectory $app -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    try {
        if ($process.WaitForExit(8000)) { throw "Application exited during startup with code $($process.ExitCode). See $stderr" }
        Write-Output 'PASS: Online application remained running for 8 seconds. Backend login requires a software account.'
    } finally {
        if (-not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
    }
    return
}
$env:QT_QPA_PLATFORM = 'windows'
Start-Process -FilePath $exe -WorkingDirectory $app -WindowStyle Hidden
