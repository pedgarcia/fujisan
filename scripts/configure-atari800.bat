@echo off
REM configure-atari800.bat - Windows batch script for configuring atari800
REM This script is called by CMake ExternalProject on Windows

set ATARI800_SRC_PATH=%1

if "%ATARI800_SRC_PATH%"=="" (
    echo Error: ATARI800_SRC_PATH not provided
    exit /b 1
)

if not exist "%ATARI800_SRC_PATH%" (
    echo Error: ATARI800_SRC_PATH directory does not exist: %ATARI800_SRC_PATH%
    exit /b 1
)

echo Configuring atari800 at: %ATARI800_SRC_PATH%

REM Change to source directory
cd /d "%ATARI800_SRC_PATH%"

REM Apply patches if they exist
if exist "fujisan-patches" (
    if exist "fujisan-patches\apply-patches.sh" (
        echo Applying Fujisan patches...
        cd fujisan-patches
        REM Use bash from MSYS2 to run the patch script
        bash apply-patches.sh
        if errorlevel 1 (
            echo Error: Failed to apply patches
            exit /b 1
        )
        cd "%ATARI800_SRC_PATH%"
    ) else (
        echo Warning: apply-patches.sh not found
    )
) else (
    echo Warning: Fujisan patches directory not found
)

REM Generate configure script if needed using MSYS2 bash
if not exist "configure" (
    echo Generating configure script with autogen.sh...
    bash autogen.sh
    if errorlevel 1 (
        echo Error: Failed to generate configure script
        exit /b 1
    )
)

REM Configure for libatari800 using MSYS2 bash
echo Configuring libatari800...
bash configure --target=libatari800 --enable-netsio
if errorlevel 1 (
    echo Error: Failed to configure libatari800
    exit /b 1
)

echo atari800 configuration completed successfully
exit /b 0