function Get-OnlineDependencyRoot([string]$RequestedRoot) {
    if (-not $RequestedRoot) { $RequestedRoot = $env:TNP_DEPS_ROOT }
    if (-not $RequestedRoot) {
        $RequestedRoot = Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) 'tnpsource_developmentKit\developenv'
    }
    foreach ($name in @('Qt5', 'gdal-dev', 'qgis-ltr-dev', 'WebView2')) {
        if (-not (Test-Path -LiteralPath (Join-Path $RequestedRoot $name) -PathType Container)) {
            throw "Missing dependency: $RequestedRoot\$name. Set TNP_DEPS_ROOT or pass -DependencyRoot."
        }
    }
    return (Resolve-Path -LiteralPath $RequestedRoot).Path
}

function Set-OnlineEnvironment([string]$DependencyRoot) {
    $env:PATH = @(
        "$DependencyRoot\Qt5\bin", "$DependencyRoot\gdal-dev\bin",
        "$DependencyRoot\qgis-ltr-dev\bin", "$DependencyRoot\WebView2",
        "$DependencyRoot\WebView2\lib", $env:PATH
    ) -join ';'
    $env:QT_PLUGIN_PATH = "$DependencyRoot\Qt5\plugins;$DependencyRoot\qgis-ltr-dev\qtplugins"
    $env:TNP_QGIS_PREFIX = Join-Path $DependencyRoot 'qgis-ltr-dev'
    $env:GDAL_DATA = Join-Path $DependencyRoot 'gdal-dev\share\gdal'
    $env:PROJ_LIB = Join-Path $DependencyRoot 'gdal-dev\share\proj'
    $env:PROJ_DATA = $env:PROJ_LIB
    # Clear inherited settings from retired launchers, including their WFS exclusion.
    foreach ($name in @('TNP_OFFLINE_CONFIG', 'TNP_OFFLINE_SCHEMA', 'TNP2_UI_TEST_OUTPUT', 'QGIS_PROVIDER_FILE')) {
        [Environment]::SetEnvironmentVariable($name, $null, 'Process')
    }
}
