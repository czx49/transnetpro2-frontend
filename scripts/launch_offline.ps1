param([switch]$CheckOnly,[switch]$SmokeTest)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$deps=Join-Path (Split-Path $root -Parent) 'tnpsource_developmentKit\developenv'
$offline=Join-Path $root 'offline'
$app=Join-Path $offline 'bin'
$exe=Join-Path $app 'tnp2_app.exe'
if(-not(Test-Path -LiteralPath $exe)){throw '请先运行 scripts/build_offline.ps1 编译二期离线版。'}
foreach($dir in @($app,(Join-Path $app 'logs'),(Join-Path $offline 'logs'),(Join-Path $offline 'data'))){New-Item -ItemType Directory -Path $dir -Force | Out-Null}
Copy-Item -LiteralPath (Join-Path $root 'src\offline\schema.sql') -Destination (Join-Path $offline 'config\schema.sql') -Force
Copy-Item -LiteralPath (Join-Path $root 'src\offline\log4qt.properties') -Destination (Join-Path $app 'log4qt.properties') -Force
$gsl=Join-Path (Split-Path $root -Parent) 'tnpsource_developmentKit\transnetproCode\out\bin\algorithm\ctap\gsl.dll'
foreach($name in @('gsl.dll','cblas.dll')){if(-not(Test-Path -LiteralPath (Join-Path $app $name))){Copy-Item -LiteralPath (Join-Path (Split-Path $gsl -Parent) $name) -Destination (Join-Path $app $name)}}
[IO.File]::WriteAllText((Join-Path $app 'httpserver.ini'),"[Config]`r`nHostName=http://offline.invalid`r`n")
$env:PATH="$deps\Qt5\bin;$deps\gdal-dev\bin;$deps\qgis-ltr-dev\bin;$deps\WebView2;$env:PATH"
$env:QT_PLUGIN_PATH="$deps\Qt5\plugins;$deps\qgis-ltr-dev\qtplugins"
$env:TNP_QGIS_PREFIX=Join-Path $deps 'qgis-ltr-dev'
$env:TNP_OFFLINE_CONFIG=Join-Path $offline 'config\database.json'
$env:TNP_OFFLINE_SCHEMA=Join-Path $offline 'config\schema.sql'
$env:GDAL_DATA=Join-Path $deps 'gdal-dev\share\gdal'
$env:PROJ_LIB=Join-Path $deps 'gdal-dev\share\proj'
$env:PROJ_DATA=$env:PROJ_LIB
$env:QGIS_PROVIDER_FILE='provider_(postgres|spatialite|delimitedtext)\.dll$'
foreach($required in @("$deps\Qt5\plugins\platforms\qwindows.dll","$deps\Qt5\plugins\sqldrivers\qsqlpsql.dll","$deps\qgis-ltr-dev\plugins\provider_postgres.dll","$deps\WebView2\WebView2Loader.dll",$env:TNP_OFFLINE_CONFIG,$env:TNP_OFFLINE_SCHEMA)){
    if(-not(Test-Path -LiteralPath $required)){throw "缺少启动文件：$required"}
}
if($CheckOnly){Write-Output "READY: $exe";return}
if($SmokeTest){
    $env:QT_QPA_PLATFORM='offscreen';$env:QT_QPA_FONTDIR=Join-Path $env:WINDIR 'Fonts'
    $env:TNP2_UI_TEST_OUTPUT=Join-Path $offline 'logs\ui_check'
    New-Item -ItemType Directory -Path $env:TNP2_UI_TEST_OUTPUT -Force | Out-Null
    $process=Start-Process -FilePath $exe -ArgumentList '--offline-ui-test' -WorkingDirectory $app -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $env:TNP2_UI_TEST_OUTPUT 'stdout.txt') -RedirectStandardError (Join-Path $env:TNP2_UI_TEST_OUTPUT 'stderr.txt')
    $null=$process.Handle
    if(-not $process.WaitForExit(55000)){$process.Kill();$process.WaitForExit();throw '二期界面验收超时。'}
    if($process.ExitCode -ne 0){throw "二期界面验收失败，退出码 $($process.ExitCode)。"}
    Write-Output 'PASS: Phase2 offline UI.';return
}
$env:QT_QPA_PLATFORM='windows'
Start-Process -FilePath $exe -WorkingDirectory $app
