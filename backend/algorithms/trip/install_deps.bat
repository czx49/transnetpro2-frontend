@echo off
setlocal

rem Switch to the current script directory (project root)
cd /d %~dp0

echo [STEP] Build DLL (dist_dll\trip_model.dll)
call build-dll.bat
if errorlevel 1 (
  echo [ERROR] Build DLL failed, abort.
  goto :END
)

echo [STEP] Install deps to dist_dll\pyhome\Lib\site-packages (pandas, psycopg2-binary)
call tools\vendor_deps.bat pandas psycopg2-binary
if errorlevel 1 (
  echo [ERROR] Install deps failed, abort.
  goto :END
)

echo [OK] DLL built and deps installed. You can now package / ship the dist_dll folder.

:END
pause