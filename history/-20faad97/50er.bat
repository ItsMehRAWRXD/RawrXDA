@echo off
REM Universal Compiler - Portable Wrapper
pwsh.exe -ExecutionPolicy Bypass -NoProfile -File "%~dp0ucc.ps1" -- %*
