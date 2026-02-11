@echo off
powershell -NoProfile -ExecutionPolicy Bypass -Command "& '%~dp0src\multimodal_engine\scripts\update-quant-types.ps1' %*"
