@echo off
REM One-click flash launcher for furble M5StickS3.
REM ASCII-only content, CRLF line endings, to avoid cmd.exe codepage issues.
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0FLASH_M5S3.ps1"
