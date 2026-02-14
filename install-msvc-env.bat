@echo off
setlocal enabledelayedexpansion

REM ======================================================
REM 1. Locate vswhere.exe
REM ======================================================
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
    set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
) else if exist "%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe" (
    set VSWHERE="%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
) else (
    echo ERROR: vswhere.exe not found
    exit /b 1
)

REM ======================================================
REM 2. Find latest VS 2022 installation
REM ======================================================
for /f "usebackq tokens=*" %%i in (`%VSWHERE% -latest -products * -requires Microsoft.Component.MSBuild -property installationPath -format value`) do (
    set VSINSTALL=%%i
)
if not defined VSINSTALL (
    echo ERROR: Visual Studio installation not found
    exit /b 1
)
echo Visual Studio path: %VSINSTALL%

REM ======================================================
REM 3. Check if any preset already has "environment"
REM ======================================================
set PRESET_FILE=CMakePresets.json
if not exist "%PRESET_FILE%" (
    echo ERROR: %PRESET_FILE% not found
    exit /b 1
)

set ENV_EXISTS=0
for /f "usebackq delims=" %%L in ("%PRESET_FILE%") do (
    echo %%L | findstr /c:"\"environment\"" >nul
    if !errorlevel! == 0 set ENV_EXISTS=1
)

if %ENV_EXISTS%==1 (
    echo Environment already exists in CMakePresets.json. Skipping script.
    exit /b 0
)

REM ======================================================
REM 4. Call VsDevCmd.bat to setup x64 environment
REM ======================================================
call "%VSINSTALL%\Common7\Tools\VsDevCmd.bat" -arch=amd64 -host_arch=amd64 >nul 2>&1

REM ======================================================
REM 5. Capture environment variables
REM ======================================================
set "ENV_PATH=%PATH%"
set "ENV_INCLUDE=%INCLUDE%"
set "ENV_LIB=%LIB%"
set "ENV_LIBPATH=%LIBPATH%"

REM ======================================================
REM 6. Convert backslashes to forward slashes
REM ======================================================
set "ENV_PATH=!ENV_PATH:\=/!"
set "ENV_INCLUDE=!ENV_INCLUDE:\=/!"
set "ENV_LIB=!ENV_LIB:\=/!"
set "ENV_LIBPATH=!ENV_LIBPATH:\=/!"

REM ======================================================
REM 7. Update CMakePresets.json
REM ======================================================
set TMP_FILE=%PRESET_FILE%.tmp
> "%TMP_FILE%" (
    for /f "usebackq delims=" %%L in ("%PRESET_FILE%") do (
        set "line=%%L"
        echo !line! | findstr /c:"\"name\":" >nul
        if !errorlevel! == 0 (
            REM insert environment block after "name" line
            echo !line!
            echo       "environment": {
            echo         "PATH": "!ENV_PATH!",
            echo         "INCLUDE": "!ENV_INCLUDE!",
            echo         "LIB": "!ENV_LIB!",
            echo         "LIBPATH": "!ENV_LIBPATH!"
            echo       },
        ) else (
            echo !line!
        )
    )
)
move /y "%TMP_FILE%" "%PRESET_FILE%"
echo SUCCESS: updated CMakePresets.json with / slashes and x64 environment
endlocal
exit /b 0
