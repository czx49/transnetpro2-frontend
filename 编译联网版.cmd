@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\build_online.ps1"
if errorlevel 1 pause
