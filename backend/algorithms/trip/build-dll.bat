@echo off
setlocal

rem Standalone builder for dist_dll\wrapper.cpp
rem Usage: build-dll.bat [out_name]

where cl >nul 2>nul
if errorlevel 1 (
  echo [ERROR] cl not found. Please run this script inside "x64 Native Tools Command Prompt for VS".
  exit /b 1
)

where python >nul 2>nul
if errorlevel 1 (
  echo [ERROR] python not found. Please install Python and ensure it is on PATH.
  exit /b 2
)

set SCRIPT_DIR=%~dp0
set DLL_DIR=%SCRIPT_DIR%dist_dll
if not exist "%DLL_DIR%\wrapper.cpp" (
  echo [ERROR] %DLL_DIR%\wrapper.cpp not found
  exit /b 3
)

rem Support clean command
if /I "%~1"=="clean" goto :CLEAN

set OUT_DLL=trip_model.dll
if not "%~1"=="" set OUT_DLL=%~1

rem Query Python include/lib (Windows CMD friendly)
python -c "import sys,sysconfig as sc; inc=sc.get_paths().get('include'); base=sys.prefix; libname='python{}{}'.format(sys.version_info[0],sys.version_info[1]); print('set PY_INC='+inc); print('set PY_LIBDIR='+base+'\libs'); print('set PY_LIB='+libname+'.lib'); print('set PY_BASE='+base)" > "%DLL_DIR%\_py_env.bat"
if errorlevel 1 (
  echo [ERROR] Failed to query Python include/lib paths
  exit /b 4
)
call "%DLL_DIR%\_py_env.bat"
del /f /q "%DLL_DIR%\_py_env.bat" >nul 2>nul

echo [INFO] Python version:
python -V
echo [INFO] PY_INC=%PY_INC%
echo [INFO] PY_LIBDIR=%PY_LIBDIR%
echo [INFO] PY_LIB=%PY_LIB%

if not exist "%PY_INC%\Python.h" (
  echo [ERROR] Python.h not found, PY_INC=%PY_INC%
  exit /b 5
)
if not exist "%PY_LIBDIR%\%PY_LIB%" (
  echo [ERROR] %PY_LIB% not found, PY_LIBDIR=%PY_LIBDIR%
  exit /b 6
)

pushd "%DLL_DIR%" >nul
if exist "%OUT_DLL%" del /f /q "%OUT_DLL%" >nul 2>nul
if exist "trip_model.lib" del /f /q "trip_model.lib" >nul 2>nul
if exist "trip_model.exp" del /f /q "trip_model.exp" >nul 2>nul

cl /nologo /LD /std:c++17 /O2 /EHsc /source-charset:utf-8 /execution-charset:utf-8 ^
  /I"%PY_INC%" wrapper.cpp ^
  /link /OUT:"%OUT_DLL%" /LIBPATH:"%PY_LIBDIR%" "%PY_LIB%"
set ERR=%ERRORLEVEL%
if %ERR% NEQ 0 (
  echo [ERROR] Build failed (%ERR%)
  popd >nul
  exit /b %ERR%
)
echo [OK] Built %OUT_DLL%

rem Try copy the runtime Python DLL (e.g., python311.dll) next to the built DLL for client deployment
set PY_DLL=%PY_LIB:.lib=.dll%
rem Clean other python3*.dll to avoid mixing different versions
for %%F in ("python3*.dll") do (
  if /I not "%%~nxF"=="%PY_DLL%" del /f /q "%%~fF" >nul 2>nul
)
if exist "%PY_BASE%\%PY_DLL%" (
  copy /Y "%PY_BASE%\%PY_DLL%" . >nul
  echo [OK] Copied %PY_DLL% to %CD%
) else (
  echo [WARN] %PY_BASE%\%PY_DLL% not found. Please ensure client PATH contains the Python runtime directory.
)
popd >nul
exit /b 0

:CLEAN
echo [INFO] Cleaning build outputs in "%DLL_DIR%"

pushd "%DLL_DIR%" >nul
del /f /q "trip_model.dll" >nul 2>nul
del /f /q "trip_model.lib" >nul 2>nul
del /f /q "trip_model.exp" >nul 2>nul
for %%F in ("python3*.dll") do del /f /q "%%~fF" >nul 2>nul
popd >nul
echo [OK] Clean done.
exit /b 0