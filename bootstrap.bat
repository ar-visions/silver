@echo off
:: bootstrap.bat -- bootstrap the import environment for silver (windows)
:: mirrors bootstrap.sh: same layout, same names, same python entry points

setlocal enabledelayedexpansion

:: ---------------- defaults + arg parse ----------------
set "SDK=native"
set "TYPE=release"
set "ASAN="
for %%A in (%*) do (
    if /i "%%~A"=="--debug" (
        set "TYPE=debug"
    ) else if /i "%%~A"=="--release" (
        set "TYPE=release"
    ) else if /i "%%~A"=="--asan" (
        set "ASAN=--asan"
    ) else (
        set "SDK=%%~A"
    )
)

:: ---------------- paths ----------------
:: PROJECT_PATH is the directory we were invoked from -- silver can bootstrap
:: another project (the silver build is just its own host)
if not defined PROJECT_PATH (
    set "PROJECT_PATH=%CD%"
)
for %%I in ("%PROJECT_PATH%") do set "PROJECT_NAME=%%~nxI"

for %%I in ("%~dp0.") do set "SILVER=%%~fI"

set "CHECKOUT=%PROJECT_PATH%\checkout"
set "NATIVE=%SILVER%\platform\native"
:: install is the platform-STABLE alias (junction -> platform\<plat>); all
:: build + source-map references must use it so they survive a platform change
set "IMPORT=%SILVER%\install"
set "BUILD=%IMPORT%\build"

:: ---------------- arch ----------------
if /i "%PROCESSOR_ARCHITECTURE%"=="ARM64" (
    set "ARCH=aarch64"
    set "TRIPLE=aarch64-pc-windows-msvc"
    set "VSARCH=arm64"
) else (
    set "ARCH=x86_64"
    set "TRIPLE=x86_64-pc-windows-msvc"
    set "VSARCH=amd64"
)

if /i not "%SDK%"=="native" (
    echo bootstrap: cross-SDK '%SDK%' is unix-host only ^(musl/glibc/gcc stages^).
    echo   bootstrap native on windows, or cross-compile from linux.
    exit /b 1
)

:: ---------------- install junction ----------------
:: this MUST precede any mkdir under %IMPORT%: creating install\build first
:: would make install\ a real directory, and every consumer reads install\
if not exist "%NATIVE%" mkdir "%NATIVE%"

set "INSTALL_LINK="
for /f "delims=" %%L in ('dir /a:l /b "%SILVER%" 2^>nul ^| findstr /i /x "install"') do set "INSTALL_LINK=1"

if defined INSTALL_LINK (
    rem already the junction -- nothing to do
) else if not exist "%IMPORT%" (
    mklink /J "%IMPORT%" "%NATIVE%" >nul || exit /b 1
) else (
    set "INSTALL_EMPTY=1"
    for /f "delims=" %%E in ('dir /b /a "%IMPORT%" 2^>nul') do set "INSTALL_EMPTY="
    if defined INSTALL_EMPTY (
        rmdir "%IMPORT%" && mklink /J "%IMPORT%" "%NATIVE%" >nul || exit /b 1
    ) else (
        echo bootstrap: %IMPORT% is a real directory, not the junction to
        echo   platform\native. move it aside and re-run:
        echo     move "%IMPORT%" "%IMPORT%.bak"
        exit /b 1
    )
)

for %%D in ("%NATIVE%" "%NATIVE%\include" "%NATIVE%\bin" "%NATIVE%\lib" "%NATIVE%\syntax" ^
            "%CHECKOUT%" "%BUILD%" "%SILVER%\checkout" "%SILVER%\private" "%SILVER%\private\silver") do (
    if not exist %%D mkdir %%D
)

:: real path, not the junction: tools self-locate from their exe dir
set "PATH=%BUILD%;%NATIVE%\bin;%NATIVE%\bin\perl\bin;%PATH%"

:: persist our bin paths in the USER environment so built apps resolve in new shells
powershell -NoProfile -Command ^
  "$p=[Environment]::GetEnvironmentVariable('PATH','User'); if ($null -eq $p) { $p='' }; $bin='%NATIVE%\bin'; if (-not $p.ToLower().Contains($bin.ToLower())) { [Environment]::SetEnvironmentVariable('PATH', '%BUILD%;'+$bin+';'+$p, 'User'); Write-Host 'added silver PATH to the user environment (open a new shell to pick it up)' }"

:: Au's ports.h must be reachable as <ports.h> by modules that build OUTSIDE the
:: src tree (e.g. foundry\dbg) -- copy it into the install include dir
copy /Y "%SILVER%\src\ports.h" "%NATIVE%\include\ports.h" >nul
copy /Y "%SILVER%\src\undefcpp.h" "%NATIVE%\include\undefcpp.h" >nul

:: ---------------- dbg ----------------
:: thin lldb shortcut: `dbg <app> [args...]` runs the app under the vendored
:: lldb and prints a backtrace on a crash (never sits at an interactive prompt)
set "DBG=%SILVER%\dbg.cmd"
> "%DBG%" echo @echo off
>>"%DBG%" echo setlocal
>>"%DBG%" echo set "HERE=%%~dp0"
>>"%DBG%" echo set "LLDB=%%HERE%%install\bin\lldb.exe"
>>"%DBG%" echo if not exist "%%LLDB%%" set "LLDB=lldb"
>>"%DBG%" echo "%%LLDB%%" --batch -o run -k "thread backtrace" -k quit -- %%*

:: ---------------- visual studio ----------------
set "VSWHERE=%NATIVE%\bin\vswhere.exe"
set "VSWHERE_URL=https://github.com/microsoft/vswhere/releases/download/3.1.7/vswhere.exe"
if not exist "%VSWHERE%" (
    echo downloading vswhere...
    powershell -NoProfile -Command "Invoke-WebRequest -Uri '%VSWHERE_URL%' -OutFile '%VSWHERE%'" || exit /b 1
)

:: the vendored toolchain wins; fall back to a registered VS 2022
set "VS_DIR=%NATIVE%\vs2022"
set "VS_INSTALL="
if exist "%VS_DIR%\VC\Auxiliary\Build\vcvarsall.bat" set "VS_INSTALL=%VS_DIR%"
if not defined VS_INSTALL (
    for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul`) do set "VS_INSTALL=%%I"
)

if not defined VS_INSTALL (
    echo Visual Studio 2022 not found. Installing into %VS_DIR% ...
    powershell -NoProfile -Command "Invoke-WebRequest -Uri 'https://aka.ms/vs/17/release/vs_community.exe' -OutFile '%CHECKOUT%\vs_community.exe'" || exit /b 1
    "%CHECKOUT%\vs_community.exe" --wait --norestart --nocache ^
      --installPath "%VS_DIR%" ^
      --add Microsoft.VisualStudio.Workload.NativeDesktop --includeRecommended ^
      --add Microsoft.VisualStudio.Workload.VCTools ^
      --add Microsoft.VisualStudio.Component.VC.Llvm.Clang ^
      --add Microsoft.VisualStudio.Component.VC.Llvm.ClangToolset ^
      --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 ^
      --add Microsoft.VisualStudio.Component.VC.ATL ^
      --add Microsoft.VisualStudio.Component.Windows11SDK.22621
    del "%CHECKOUT%\vs_community.exe"
    if exist "%VS_DIR%\VC\Auxiliary\Build\vcvarsall.bat" set "VS_INSTALL=%VS_DIR%"
)

if not defined VS_INSTALL (
    echo bootstrap: Visual Studio 2022 build tools not found after install.
    exit /b 1
)

:: no vcvarsall: our clang locates the windows sdk on its own. VS is here
:: only because clang needs the sdk headers/libs to exist somewhere.

:: ---------------- cmake ----------------
where cmake >nul 2>&1
if errorlevel 1 (
    echo bootstrap: cmake not found on PATH. install it, e.g.:
    echo     winget install Kitware.CMake
    exit /b 1
)

:: ---------------- perl (openssl and friends need it) ----------------
set "PERL_ZIP=strawberry-perl-5.38.2.2-64bit-portable.zip"
set "PERL_URL=https://github.com/StrawberryPerl/Perl-Dist-Strawberry/releases/download/SP_53822_64bit/%PERL_ZIP%"
if not exist "%NATIVE%\bin\perl\bin\perl.exe" (
    echo downloading perl...
    powershell -NoProfile -Command "Invoke-WebRequest -Uri '%PERL_URL%' -OutFile '%NATIVE%\bin\%PERL_ZIP%'" || exit /b 1
    powershell -NoProfile -Command "Expand-Archive -Path '%NATIVE%\bin\%PERL_ZIP%' -DestinationPath '%NATIVE%\bin' -Force" || exit /b 1
    del "%NATIVE%\bin\%PERL_ZIP%"
)

:: ---------------- ninja ----------------
set "NINJA_URL=https://github.com/ninja-build/ninja/releases/download/v1.13.1/ninja-win.zip"
if not exist "%NATIVE%\bin\ninja.exe" (
    echo downloading ninja...
    powershell -NoProfile -Command "Invoke-WebRequest -Uri '%NINJA_URL%' -OutFile '%NATIVE%\bin\ninja.zip'" || exit /b 1
    powershell -NoProfile -Command "Expand-Archive -Path '%NATIVE%\bin\ninja.zip' -DestinationPath '%NATIVE%\bin' -Force" || exit /b 1
    del "%NATIVE%\bin\ninja.zip"
)

:: ---------------- python ----------------
:: a real python, NOT the vendored embedded one: its python311._pth replaces
:: sys.path outright, so a script never gets its own directory on the path and
:: every build script that imports a sibling module fails (mbedtls, libcxx)
set "PYTHON="
for /f "delims=" %%Q in ('where python.exe python3.exe 2^>nul') do (
    if not defined PYTHON (
        echo %%Q | findstr /i /c:"%NATIVE%" >nul || set "PYTHON=%%Q"
    )
)
if not defined PYTHON if exist "%NATIVE%\bin\python.exe" set "PYTHON=%NATIVE%\bin\python.exe"
if not defined PYTHON (
    echo bootstrap: python not found. install it, e.g.:
    echo     winget install Python.Python.3.12
    exit /b 1
)
set "PYTHONPATH=%SILVER%\src"

:: that python must win over the vendored one for child builds too
for %%I in ("%PYTHON%") do set "PATH=%%~dpI;%PATH%"

:: ---------------- target.cmake ----------------
echo Generating target.cmake for %TRIPLE% (Windows)
set "TC=%IMPORT%\target.cmake"
> "%TC%" echo # Auto-generated by Silver bootstrap
>>"%TC%" echo # Toolchain for %SDK% (%TRIPLE%)
>>"%TC%" echo.
>>"%TC%" echo set(CMAKE_SYSTEM_NAME Windows)
>>"%TC%" echo set(CMAKE_SYSTEM_PROCESSOR %ARCH%)
>>"%TC%" echo.
>>"%TC%" echo get_filename_component(TARGET_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)
>>"%TC%" echo.
>>"%TC%" echo set(CMAKE_C_COMPILER   "%NATIVE:\=/%/bin/clang-cl.exe" CACHE STRING "")
>>"%TC%" echo set(CMAKE_CXX_COMPILER "%NATIVE:\=/%/bin/clang-cl.exe" CACHE STRING "")
>>"%TC%" echo set(CMAKE_LINKER       "%NATIVE:\=/%/bin/lld-link.exe" CACHE STRING "")
>>"%TC%" echo.
>>"%TC%" echo set(SILVER_TARGET_NAME "%SDK%")
>>"%TC%" echo set(SILVER_TARGET_TRIPLE "%TRIPLE%")

:: ---------------- generate ----------------
pushd "%SILVER%"
"%PYTHON%" src\import.py --import "%IMPORT%" --%TYPE% %ASAN% --project-path "%PROJECT_PATH%" --build-path "%BUILD%" --project-name "%PROJECT_NAME%" %SDK%
set "EXITCODE=%ERRORLEVEL%"
if "%EXITCODE%"=="0" (
    "%PYTHON%" src\gen.py --import "%IMPORT%" --%TYPE% %ASAN% --project-path "%PROJECT_PATH%" --build-path "%BUILD%" --project-name "%PROJECT_NAME%" %SDK%
    set "EXITCODE=!ERRORLEVEL!"
)
popd

endlocal & exit /b %EXITCODE%
