@echo off
setlocal
cd /d "%~dp0..\.."

where pyw >nul 2>nul
if %errorlevel%==0 (
    start "" pyw -3 tools\pack_converter\pack_converter_app.pyw
    exit /b 0
)

where pythonw >nul 2>nul
if %errorlevel%==0 (
    start "" pythonw tools\pack_converter\pack_converter_app.pyw
    exit /b 0
)

python tools\pack_converter\pack_converter_app.py
if errorlevel 1 pause
