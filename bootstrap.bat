@echo off
:: bootstrap.bat -- bootstrap import environment for silver

setlocal enabledelayedexpansion

set "SDK=native"
set "TYPE=debug"
for %%A in (%*) do (
    if /i "%%~A"=="--debug" (
        set "TYPE=debug"
    ) else if /i "%%~A"=="--release" (
        set "TYPE=release"
    ) else (
        set "SDK=%%~A"
    )
)

:: ----------- Config ------------
set "PERL_ZIP=strawberry-perl-5.38.2.2-64bit-portable.zip"
set "NINJA_URL=https://github.com/ninja-build/ninja/releases/download/v1.13.1/ninja-win.zip"
set "VSWHERE_URL=https://github.com/microsoft/vswhere/releases/download/3.1.7/vswhere.exe"
set "PERL_URL=https://github.com/StrawberryPerl/Perl-Dist-Strawberry/releases/download/SP_53822_64bit/%PERL_ZIP%"

for %%I in ("%~dp0.") do set "SCRIPT_DIR=%%~fI"
pushd "%SCRIPT_DIR%"
set "SILVER=%CD%"
popd

set "PROJECT=%SILVER%"
set "CHECKOUT=%SILVER%\checkout"
set "IMPORT=%SILVER%\sdk\%SDK%"
set "BUILD=%IMPORT%\%TYPE%"
set "PERL_DIR=%IMPORT%\bin"
set "PERL_EXE=%PERL_DIR%\perl\bin\perl.exe"
set "NINJA_DIR=%IMPORT%\bin"
set "NINJA_EXE=%NINJA_DIR%\ninja.exe"
set "VSUTIL_DIR=%IMPORT%\bin"

if not exist "%IMPORT%"      mkdir "%IMPORT%"
if not exist "%CHECKOUT%"    mkdir "%CHECKOUT%"
if not exist "%BUILD%"       mkdir "%BUILD%"
if not exist "%VSUTIL_DIR%"  mkdir "%VSUTIL_DIR%"
if not exist "%NINJA_DIR%"   mkdir "%NINJA_DIR%"

:: todo: if cmake is not found, download and unzip it
set "CMAKE_EXE=cmake"

:: download vswhere.exe if not found
if not exist "%VSUTIL_DIR%\vswhere.exe" (
    powershell -Command "Invoke-WebRequest -Uri %VSWHERE_URL% -OutFile %VSUTIL_DIR%\vswhere.exe"
)

%VSUTIL_DIR%\vswhere -products * -version 17.0 -property installationPath > temp_vs.txt
set /p VS_INSTALL=<temp_vs.txt
del temp_vs.txt

if defined VS_INSTALL (
    echo "Visual Studio 2022 found at %VS_INSTALL%, skipping install.."
    goto :after_vs
)

:: Download Visual Studio 2022 Community bootstrapper
echo Visual Studio 2022 not found. Proceeding with install...
powershell -Command "Invoke-WebRequest -Uri https://aka.ms/vs/17/release/vs_community.exe -OutFile vs_community.exe"

:: Install with selected workloads and components
vs_community.exe --wait --norestart --nocache ^
  --installPath "%ProgramFiles%\Microsoft Visual Studio\2022\Community" ^
  --add Microsoft.VisualStudio.Workload.NativeDesktop --includeRecommended ^
  --add Microsoft.VisualStudio.Workload.VCTools ^
  --add Microsoft.VisualStudio.Component.VC.Libraries.CppRuntime ^
  --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 ^
  --add Microsoft.VisualStudio.Component.VC.Modules.x86.x64 ^
  --add Microsoft.VisualStudio.Component.VC.ATL ^
  --add Microsoft.VisualStudio.Component.Windows11SDK.22621 ^
  --add Microsoft.VisualStudio.Component.Windows10SDK.20348

:: Optional cleanup
del vs_community.exe
echo [silver] loading Visual Studio 2022 environment
:after_vs

:: call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64

if not exist "%PERL_EXE%" (
    echo "downloading perl..."
    if not exist "%PERL_DIR%" mkdir "%PERL_DIR%"
    powershell -Command "Invoke-WebRequest -Uri '%PERL_URL%' -OutFile '%PERL_DIR%\%PERL_ZIP%'"
    powershell -Command "Expand-Archive -Path '%PERL_DIR%\%PERL_ZIP%' -DestinationPath '%PERL_DIR%' -Force"
    del %PERL_DIR%\%PERL_ZIP%
)

:: ----------- Ensure Ninja Installed ------------
if not exist "%NINJA_EXE%" (
    echo "downloading ninja..."
    if not exist "%NINJA_DIR%" mkdir "%NINJA_DIR%"
    powershell -Command "Invoke-WebRequest -Uri '%NINJA_URL%' -OutFile '%NINJA_DIR%\ninja.zip'"
    powershell -Command "Expand-Archive -Path '%NINJA_DIR%\ninja.zip' -DestinationPath '%NINJA_DIR%' -Force"
    del %NINJA_DIR%\ninja.zip
)

set "PATH=%~dp0sdk\%SDK%\bin\perl\bin;%~dp0sdk\%SDK%\bin;%PATH%"

pushd "%~dp0"
for %%I in ("%CD%") do set "TOPDIR=%%~nxI"
echo python3 import.py --import %IMPORT% --%TYPE% --project-path %CD% --build-path %BUILD% --project %TOPDIR% %SDK%
python3 import.py --import %IMPORT% --%TYPE% --project-path %CD% --build-path %BUILD% --project %TOPDIR% %SDK%
python3 gen.py    --import %IMPORT% --%TYPE% --project-path %CD% --build-path %BUILD% --project %TOPDIR% %SDK%
set EXITCODE=%ERRORLEVEL%
popd

:: return what python returns
exit /b %EXITCODE%

endlocal