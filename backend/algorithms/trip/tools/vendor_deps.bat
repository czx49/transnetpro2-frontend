@echo off
setlocal

rem Vendor third-party Python packages into dist_dll\pyhome\Lib\site-packages
rem Usage: tools\vendor_deps.bat [package1] [package2] ...

set SCRIPT_DIR=%~dp0
set PROJECT_DIR=%SCRIPT_DIR%..\
set SITEPKG=%PROJECT_DIR%dist_dll\pyhome\Lib\site-packages

if not exist "%PROJECT_DIR%dist_dll\pyhome" (
  echo [ERROR] dist_dll\pyhome not found. Please run tools\prepare_pyhome.bat to unpack embedded Python.
  exit /b 1
)

if not exist "%SITEPKG%" mkdir "%SITEPKG%"

set PKGS=%*
if "%PKGS%"=="" set PKGS=psycopg2-binary

echo [INFO] Installing deps to: %SITEPKG%
python -m pip install --upgrade --no-warn-script-location --target "%SITEPKG%" %PKGS%
if errorlevel 1 (
  echo [ERROR] Installing deps failed
  exit /b 2
)
echo [OK] Deps installed successfully
exit /b 0
