@echo off
REM Vellum Run Script for Windows (requires WSL2)
REM This script launches the Linux run.sh script in WSL2

echo === Vellum Hypervisor Setup ===
echo.

if "%WSL_DISTRO_NAME%"=="" (
    echo Error: WSL2 is required. Please install WSL2 and run this from within WSL2.
    echo.
    echo To install WSL2:
    echo 1. Open PowerShell as Administrator
    echo 2. Run: wsl --install
    echo 3. Restart your computer
    echo 4. Run: wsl to enter Linux environment
    echo.
    pause
    exit /b 1
)

echo Starting Vellum in WSL2...
echo.

REM Convert Windows path to WSL path
for /f "delims=" %%i in ("%~dp0") do set "WIN_PATH=%%i"
set "WSL_PATH=%WIN_PATH:\=/%"
set "WSL_PATH=%WSL_PATH::=%"
set "WSL_PATH=/mnt/%WSL_PATH%"

echo Running: wsl bash "%WSL_PATH%run.sh"
wsl bash "%WSL_PATH%run.sh"