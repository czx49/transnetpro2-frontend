param([switch]$SkipTests)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$deps=Join-Path (Split-Path $root -Parent) 'tnpsource_developmentKit\developenv'
$build=Join-Path $root '_audit\2026-10-25\build'
$cmake='C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$env:PATH="$deps\Qt5\bin;$deps\gdal-dev\bin;$deps\qgis-ltr-dev\bin;$env:PATH"
& $cmake -S $root -B $build -G 'Visual Studio 17 2022' -A x64 "-DGDAL_DIR=$deps/gdal-dev/lib/cmake/gdal" "-DQGIS_DIR=$deps/qgis-ltr-dev" "-DCMAKE_PREFIX_PATH=$deps/Qt5" "-DWEBVIEW2_SDK_DIR=$deps/WebView2" '-DTNP_OFFLINE_BUILD=ON'
if($LASTEXITCODE -ne 0){throw 'CMake configuration failed.'}
& $cmake --build $build --config Release --target tnp2_app tnp2_offline_tests --parallel 4
if($LASTEXITCODE -ne 0){throw 'Phase2 offline build failed.'}
if(-not $SkipTests){
    & (Join-Path $PSScriptRoot 'launch_offline.ps1') -CheckOnly
    & (Join-Path (Split-Path $cmake -Parent) 'ctest.exe') --test-dir $build -C Release --output-on-failure
    if($LASTEXITCODE -ne 0){throw 'Phase2 offline data tests failed.'}
}
