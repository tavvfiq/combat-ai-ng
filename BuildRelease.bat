@echo off
setlocal enabledelayedexpansion

REM Parse command-line argument
set BUILD_SE=1
set BUILD_AE=1

if "%1"=="SE" (
    set BUILD_SE=1
    set BUILD_AE=0
)
if "%1"=="se" (
    set BUILD_SE=1
    set BUILD_AE=0
)
if "%1"=="AE" (
    set BUILD_SE=0
    set BUILD_AE=1
)
if "%1"=="ae" (
    set BUILD_SE=0
    set BUILD_AE=1
)

REM Display build configuration
echo ========================================
if !BUILD_SE!==1 if !BUILD_AE!==1 (
    echo Building EnhancedCombatAI for Skyrim SE and AE
    goto :skip_msg
)
if !BUILD_SE!==1 (
    echo Building EnhancedCombatAI for Skyrim SE only
    goto :skip_msg
)
if !BUILD_AE!==1 (
    echo Building EnhancedCombatAI for Skyrim AE only
)
:skip_msg
echo ========================================
echo.
echo Usage: BuildRelease.bat SE or AE
echo   No argument = Build both SE and AE
echo   SE = Build Skyrim SE only
echo   AE = Build Skyrim AE only
echo.

RMDIR dist /S /Q 2>nul

REM Generate project files
echo Generating project files...
xmake project -k vsxmake
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to generate project files
    pause
    exit /b 1
)

REM Initialize DLL found flags
set SE_DLL_FOUND=0
set AE_DLL_FOUND=0

REM Build Skyrim SE version (if requested)
if !BUILD_SE!==1 (
    echo.
    echo ========================================
    echo Building for Skyrim SE...
    echo ========================================
    xmake f -m releasedbg --skyrim_ae=n
    if %ERRORLEVEL% NEQ 0 (
        echo ERROR: Failed to configure Skyrim SE build
        pause
        exit /b 1
    )
    xmake
    if %ERRORLEVEL% NEQ 0 (
        echo ERROR: Failed to build Skyrim SE version
        pause
        exit /b 1
    )

    REM Copy SE build files
    echo Copying Skyrim SE build files...
    if exist "build\SkyrimSE\skse\plugins\EnhancedCombatAI.dll" (
        if not exist "dist\SE\SKSE\Plugins\" mkdir "dist\SE\SKSE\Plugins\"
        copy "build\SkyrimSE\skse\plugins\EnhancedCombatAI.dll" "dist\SE\SKSE\Plugins\EnhancedCombatAI.dll" /Y >nul
        if exist "build\SkyrimSE\skse\plugins\EnhancedCombatAI.pdb" (
            copy "build\SkyrimSE\skse\plugins\EnhancedCombatAI.pdb" "dist\SE\SKSE\Plugins\EnhancedCombatAI.pdb" /Y >nul
        )
        set SE_DLL_FOUND=1
        echo   Found SE DLL at: build\SkyrimSE\skse\plugins\
    ) else if exist "build\windows\x64\releasedbg\EnhancedCombatAI.dll" (
        REM Fallback to default xmake output location
        if not exist "dist\SE\SKSE\Plugins\" mkdir "dist\SE\SKSE\Plugins\"
        copy "build\windows\x64\releasedbg\EnhancedCombatAI.dll" "dist\SE\SKSE\Plugins\EnhancedCombatAI.dll" /Y >nul
        if exist "build\windows\x64\releasedbg\EnhancedCombatAI.pdb" (
            copy "build\windows\x64\releasedbg\EnhancedCombatAI.pdb" "dist\SE\SKSE\Plugins\EnhancedCombatAI.pdb" /Y >nul
        )
        set SE_DLL_FOUND=1
        echo   Found SE DLL at: build\windows\x64\releasedbg\ (fallback)
    )

    if !SE_DLL_FOUND!==0 (
        echo WARNING: Skyrim SE DLL not found in expected locations
    ) else (
        REM Copy package files to SE folder
        echo Copying package files to SE folder...
        xcopy "package" "dist\SE" /I /Y /E >nul
    )
)

REM Build Skyrim AE version (if requested)
if !BUILD_AE!==1 (
    echo.
    echo ========================================
    echo Building for Skyrim AE...
    echo ========================================
    xmake f -m releasedbg --skyrim_ae=y
    if %ERRORLEVEL% NEQ 0 (
        echo ERROR: Failed to configure Skyrim AE build
        pause
        exit /b 1
    )
    xmake
    if %ERRORLEVEL% NEQ 0 (
        echo ERROR: Failed to build Skyrim AE version
        pause
        exit /b 1
    )

    REM Copy AE build files
    echo Copying Skyrim AE build files...
    if exist "build\SkyrimAE\skse\plugins\EnhancedCombatAI.dll" (
        if not exist "dist\AE\SKSE\Plugins\" mkdir "dist\AE\SKSE\Plugins\"
        copy "build\SkyrimAE\skse\plugins\EnhancedCombatAI.dll" "dist\AE\SKSE\Plugins\EnhancedCombatAI.dll" /Y >nul
        if exist "build\SkyrimAE\skse\plugins\EnhancedCombatAI.pdb" (
            copy "build\SkyrimAE\skse\plugins\EnhancedCombatAI.pdb" "dist\AE\SKSE\Plugins\EnhancedCombatAI.pdb" /Y >nul
        )
        set AE_DLL_FOUND=1
        echo   Found AE DLL at: build\SkyrimAE\skse\plugins\
    ) else if exist "build\windows\x64\releasedbg\EnhancedCombatAI.dll" (
        REM Fallback to default xmake output location
        if not exist "dist\AE\SKSE\Plugins\" mkdir "dist\AE\SKSE\Plugins\"
        copy "build\windows\x64\releasedbg\EnhancedCombatAI.dll" "dist\AE\SKSE\Plugins\EnhancedCombatAI.dll" /Y >nul
        if exist "build\windows\x64\releasedbg\EnhancedCombatAI.pdb" (
            copy "build\windows\x64\releasedbg\EnhancedCombatAI.pdb" "dist\AE\SKSE\Plugins\EnhancedCombatAI.pdb" /Y >nul
        )
        set AE_DLL_FOUND=1
        echo   Found AE DLL at: build\windows\x64\releasedbg\ (fallback)
    )

    if !AE_DLL_FOUND!==0 (
        echo WARNING: Skyrim AE DLL not found in expected locations
    ) else (
        REM Copy package files to AE folder
        echo Copying package files to AE folder...
        xcopy "package" "dist\AE" /I /Y /E >nul
    )
)

echo.
echo ========================================
echo Build completed successfully!
echo ========================================
if !SE_DLL_FOUND!==1 (
    echo Skyrim SE build: dist\SE\SKSE\Plugins\EnhancedCombatAI.dll
    echo   Package files copied to: dist\SE\
)
if !AE_DLL_FOUND!==1 (
    echo Skyrim AE build: dist\AE\SKSE\Plugins\EnhancedCombatAI.dll
    echo   Package files copied to: dist\AE\
)
echo.
echo Each version folder contains the DLL and all package files.
echo.

pause
