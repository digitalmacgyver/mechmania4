@echo off
REM MechMania IV Docker Launcher for Windows
REM Makes it easy to run MechMania IV with Docker

setlocal EnableDelayedExpansion

REM Configuration
set IMAGE_NAME=mechmania4
set WEB_IMAGE_NAME=mechmania4-web
set ALPINE_IMAGE_NAME=mechmania4-alpine

REM Check for Docker
docker --version >nul 2>&1
if %errorlevel% neq 0 (
    echo Docker is not installed or not in PATH!
    echo Please install Docker Desktop from https://docs.docker.com/desktop/windows/install/
    pause
    exit /b 1
)

:menu
cls
echo ========================================
echo   MechMania IV: The Vinyl Frontier
echo ========================================
echo.
echo Choose an option:
echo   1) Web Browser Game (recommended for Windows)
echo   2) Headless Test (no graphics)
echo   3) Tournament Mode
echo   4) Build Docker Images
echo   5) Exit
echo.
set /p choice=Enter choice [1-5]:

if "%choice%"=="1" goto web_game
if "%choice%"=="2" goto headless_test
if "%choice%"=="3" goto tournament
if "%choice%"=="4" goto build
if "%choice%"=="5" goto end
echo Invalid choice!
pause
goto menu

:web_game
echo Starting web-based game...
echo.
echo After the game starts, open your browser to:
echo   http://localhost:6080/vnc.html
echo.
docker run -d --rm --name mechmania4-web -p 6080:6080 -p 5900:5900 -p 2323:2323 %WEB_IMAGE_NAME%
timeout /t 3 >nul
start http://localhost:6080/vnc.html
echo.
echo Press any key to stop the game...
pause >nul
docker stop mechmania4-web
goto menu

:headless_test
echo Running headless test...
docker run -it --rm %ALPINE_IMAGE_NAME% ./quick_test.sh
pause
goto menu

:tournament
echo Starting tournament mode...
docker-compose --profile tournament up
pause
goto menu

:build
echo Building Docker images...
docker build -t %IMAGE_NAME% -f Dockerfile .
docker build -t %WEB_IMAGE_NAME% -f Dockerfile.web .
docker build -t %ALPINE_IMAGE_NAME% -f Dockerfile.alpine .
echo Build complete!
pause
goto menu

:end
echo Goodbye!
exit /b 0