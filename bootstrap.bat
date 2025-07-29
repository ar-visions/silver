@echo off
:: deps.bat - LLVM downloader/builder for Windows
:: Always returns linker flags after optional build

setlocal enabledelayedexpansion

:: ----------- Config ------------
set "LLVM_URL=https://github.com/LLVM/llvm-project"
set "LLVM_COMMIT=bd7db75"
set "PYTHON3_URL=https://www.python.org/ftp/python/3.11.9/python-3.11.9-embed-amd64.zip"
set "NINJA_URL=https://github.com/ninja-build/ninja/releases/download/v1.13.1/ninja-win.zip"
set "VSWHERE_URL=https://github.com/microsoft/vswhere/releases/download/3.1.7/vswhere.exe"

for %%I in ("%~dp0.") do set "SCRIPT_DIR=%%~fI"
pushd "%SCRIPT_DIR%"
set "SILVER=%CD%"
popd

set "CHECKOUT=%SILVER%\checkout"
set "IMPORT=%SILVER%"
set "BUILD=%IMPORT%\build"
set "LLVM_SRC=%CHECKOUT%\llvm-project"
set "LLVM_BUILD=%BUILD%\llvm-project"
set "NINJA_DIR=%IMPORT%\bin"
set "NINJA_EXE=%NINJA_DIR%\ninja.exe"
set "VSUTIL_DIR=%IMPORT%\bin"
set "PYTHON3_DIR=%IMPORT%\bin"
set "PYTHON3_EXE=%PYTHON3_DIR%\python"

if not exist "%CHECKOUT%"    mkdir "%CHECKOUT%"
if not exist "%PYTHON3_DIR%" mkdir "%PYTHON3_DIR%"
if not exist "%BUILD%"       mkdir "%BUILD%"
if not exist "%VSUTIL_DIR%"  mkdir "%VSUTIL_DIR%"

:: todo: if cmake is not found, download and unzip it
set "CMAKE_EXE=cmake"

if not exist "%PYTHON3_DIR%\python.exe" (
    powershell -Command "Invoke-WebRequest -Uri %PYTHON3_URL% -OutFile %PYTHON3_DIR%\python3.zip"
    powershell -Command "Expand-Archive -Path '%PYTHON3_DIR%\python3.zip' -DestinationPath '%PYTHON3_DIR%' -Force"
    del %PYTHON3_DIR%\python3.zip
)

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

:: ----------- Ensure Ninja Installed ------------
if not exist "%NINJA_EXE%" (
    echo "downloading ninja..."
    if not exist "%NINJA_DIR%" mkdir "%NINJA_DIR%"
    powershell -Command "Invoke-WebRequest -Uri '%NINJA_URL%' -OutFile '%NINJA_DIR%\ninja.zip'"
    powershell -Command "Expand-Archive -Path '%NINJA_DIR%\ninja.zip' -DestinationPath '%NINJA_DIR%' -Force"
    del %NINJA_DIR%\ninja.zip
)

:: ----------- Check if LLVM built ------------
if exist "%IMPORT%\bin\clang.exe" (
    goto :echo_flags
)

:: ----------- Clone or update LLVM ------------
if exist "%LLVM_SRC%" (
    pushd "%LLVM_SRC%"
    git fetch origin
    git checkout %LLVM_COMMIT%
    git pull origin %LLVM_COMMIT%
    popd
) else (
    git clone %LLVM_URL% "%LLVM_SRC%"
    pushd "%LLVM_SRC%"
    git checkout %LLVM_COMMIT%
    popd
)

:: ----------- Configure LLVM ------------
if not exist "%LLVM_BUILD%" mkdir "%LLVM_BUILD%"
pushd "%LLVM_BUILD%"
"%NINJA_EXE%" --version >nul 2>&1 || (echo ninja not found && exit /b 1)
%CMAKE_EXE% -G Ninja ^
  -S "%LLVM_SRC%\llvm" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_INSTALL_PREFIX=%IMPORT% ^
  -DLLVM_ENABLE_ASSERTIONS=ON ^
  -DCMAKE_CXX_STANDARD=17 ^
  -DCMAKE_CXX_STANDARD_REQUIRED=ON ^
  -DCMAKE_CXX_EXTENSIONS=OFF ^
  -DLLVM_ENABLE_PROJECTS=clang;lld;lldb ^
  -DLLVM_ENABLE_FFI=OFF ^
  -DLLVM_ENABLE_RTTI=OFF ^
  -DBUILD_SHARED_LIBS=OFF ^
  -DLLVM_LINK_LLVM_DYLIB=OFF ^
  -DLLVM_TARGETS_TO_BUILD=host;X86;AArch64 ^
  -DCMAKE_MAKE_PROGRAM="%NINJA_EXE%" ^
  -DLLVM_BUILD_LLVM_DYLIB=ON ^
  -DLLVM_LINK_LLVM_DYLIB=ON

:: ----------- Build and Install ------------
"%NINJA_EXE%" -j16
"%NINJA_EXE%" install
popd

:: ----------- Output linker flags ------------
:echo_flags

pushd "%~dp0"
"%PYTHON3_EXE%" gen-ninja.py %1
set EXITCODE=%ERRORLEVEL%
popd

:: return what python returns
exit /b %EXITCODE%

endlocal