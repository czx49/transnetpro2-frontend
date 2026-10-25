@echo off
setlocal EnableDelayedExpansion

rem Prepare embedded Python under dist_dll\pyhome for portable deployment
rem Usage: tools\prepare_pyhome.bat [path_to_python_embed_zip]

set SCRIPT_DIR=%~dp0
set PROJECT_DIR=%SCRIPT_DIR%..\
set DIST_DLL=%PROJECT_DIR%dist_dll
set PYHOME=%DIST_DLL%\pyhome

set ZIP_PATH=%~1
if "%ZIP_PATH%"=="" set ZIP_PATH=%SCRIPT_DIR%python-embed.zip

if not exist "%ZIP_PATH%" (
  rem Try to auto-detect common embed zip name under tools
  rem 1) prefer embed matching current Python major.minor
  set PY_TAG=
  for /f "delims=" %%V in ('python -c "import sys; print(str(sys.version_info[0])+'.'+str(sys.version_info[1]))"') do set PY_TAG=%%V
  if not "!PY_TAG!"=="" (
    for %%Z in ("%SCRIPT_DIR%python-!PY_TAG!-embed-amd64*.zip") do (
      set ZIP_PATH=%%~fZ
      goto :AFTER_DETECT
    )
  )
  rem 2) fallback to any embed zip
  for %%Z in ("%SCRIPT_DIR%python*-embed-amd64*.zip") do (
    set ZIP_PATH=%%~fZ
    goto :AFTER_DETECT
  )
)
:AFTER_DETECT

if not exist "%ZIP_PATH%" (
  echo [ERROR] Embedded Python zip not found: %ZIP_PATH%
  echo Download "Windows embeddable package (64-bit)" matching your Python version from:
  echo   https://www.python.org/downloads/windows/
  echo Then place it as tools\python-3.xx.x-embed-amd64.zip or pass the zip path to this script.
  exit /b 1
)

echo [INFO] Using embed zip: %ZIP_PATH%

if exist "%PYHOME%" (
  echo [INFO] Clean existing pyhome: %PYHOME%
  rmdir /s /q "%PYHOME%" >nul 2>nul
  if exist "%PYHOME%" (
    echo [WARN] rmdir failed, try PowerShell Remove-Item
    powershell -NoProfile -ExecutionPolicy Bypass -Command "Remove-Item -LiteralPath '%PYHOME%' -Recurse -Force -ErrorAction SilentlyContinue"
  )
)
mkdir "%PYHOME%" >nul 2>nul

echo [INFO] Extract: %ZIP_PATH% -> %PYHOME%
powershell -NoProfile -ExecutionPolicy Bypass -Command "Expand-Archive -LiteralPath '%ZIP_PATH%' -DestinationPath '%PYHOME%' -Force" || (
  echo [ERROR] Extract failed
  exit /b 2
)

rem Ensure Lib and DLLs dirs exist (some embeds rely on _pth entries)
if not exist "%PYHOME%\Lib" mkdir "%PYHOME%\Lib" >nul 2>nul
if not exist "%PYHOME%\DLLs" mkdir "%PYHOME%\DLLs" >nul 2>nul

rem Patch pythonXY._pth: enable import site; add .\Lib and .\DLLs entries
echo [INFO] Patch _pth config
powershell -NoProfile -ExecutionPolicy Bypass -Command "$pth = Get-ChildItem -LiteralPath '%PYHOME%' -Filter 'python*._pth' | Select-Object -First 1; if (-not $pth) { Write-Host '[WARN] _pth file not found, skip'; exit 0 }; $lines = Get-Content -LiteralPath $pth.FullName -Encoding UTF8; $out = @(); foreach($l in $lines){ $out += ($l -replace '^(\\s*)#\\s*import site(\\s*)$','${1}import site${2}') }; if ($out -notcontains '.\\Lib') { $out += '.\\Lib' }; if ($out -notcontains '.\\DLLs') { $out += '.\\DLLs' }; Set-Content -LiteralPath $pth.FullName -Encoding UTF8 -Value $out; Write-Host ('[OK] Patched ' + $pth.FullName)"

rem Copy pythonXY.dll to dist_dll root to help loader find runtime DLL
echo [INFO] Copy python*.dll to dist_dll root
for %%F in ("%PYHOME%\python*.dll") do (
  copy /Y "%%~fF" "%DIST_DLL%" >nul
)

echo [OK] Embedded Python prepared: %PYHOME%
echo Ensure release includes: dist_dll\trip_model.dll, dist_dll\pythonXY.dll, dist_dll\pyhome\(all files)
exit /b 0
