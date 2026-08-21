@echo off
rem Same thing from cmd.exe, or by double-click. Args pass straight through.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0go.ps1" %*
