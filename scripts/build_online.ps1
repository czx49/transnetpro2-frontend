param([string]$DependencyRoot)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'online_environment.ps1')
$root = Split-Path $PSScriptRoot -Parent
$deps = Get-OnlineDependencyRoot $DependencyRoot
Set-OnlineEnvironment $deps
$cmakeCommand = Get-Command cmake.exe -ErrorAction SilentlyContinue
if ($cmakeCommand) {
    $cmake = $cmakeCommand.Source
} else {
    $vswhere = Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Install Visual Studio 2022 with Desktop development with C++ and CMake.' }
    $cmake = @(& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe') | Select-Object -First 1
}
if (-not $cmake -or -not (Test-Path -LiteralPath $cmake)) { throw 'CMake was not found. Enable the Visual Studio CMake tools.' }
$build = Join-Path $root 'build-online'
& $cmake -S $root -B $build -G 'Visual Studio 17 2022' -A x64 "-DGDAL_DIR=$deps/gdal-dev/lib/cmake/gdal" "-DQGIS_DIR=$deps/qgis-ltr-dev" "-DCMAKE_PREFIX_PATH=$deps/Qt5" "-DWEBVIEW2_SDK_DIR=$deps/WebView2"
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
& $cmake --build $build --config Release --target tnp2_app --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Online build failed.' }
& (Join-Path $PSScriptRoot 'launch_online.ps1') -DependencyRoot $deps -CheckOnly
