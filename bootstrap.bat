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
:: mingw-w64, never msvc: it is the same toolchain a cross build from linux
:: uses, and the itanium c++ abi silver's own c++ import mangles
if /i "%PROCESSOR_ARCHITECTURE%"=="ARM64" (
    set "ARCH=aarch64"
    set "TRIPLE=aarch64-w64-windows-gnu"
) else (
    set "ARCH=x86_64"
    set "TRIPLE=x86_64-w64-windows-gnu"
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

:: Au's posix.h must be reachable as <posix.h> by modules that build OUTSIDE the
:: src tree (e.g. foundry\dbg) -- copy it into the install include dir
copy /Y "%SILVER%\src\posix.h" "%NATIVE%\include\posix.h" >nul
copy /Y "%SILVER%\src\undefcpp.h" "%NATIVE%\include\undefcpp.h" >nul
copy /Y "%SILVER%\src\pwd.h" "%NATIVE%\include\pwd.h" >nul
copy /Y "%SILVER%\src\dbus-win.h" "%NATIVE%\include\dbus-win.h" >nul

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

:: ---------------- mingw-w64 sysroot ----------------
:: no visual studio, no windows SDK, no license: mingw-w64 supplies the
:: headers, import libraries and crt objects, and llvm-mingw ships them as a
:: plain zip. the compiler-rt builtins belong to clang, so they land in its
:: own resource dir rather than in the sysroot
set "MINGW_VER=20260616"
set "MINGW_NAME=llvm-mingw-%MINGW_VER%-ucrt-%ARCH%"
set "MINGW_URL=https://github.com/mstorsjo/llvm-mingw/releases/download/%MINGW_VER%/%MINGW_NAME%.zip"
set "SYSROOT=%NATIVE%\sysroot"

if not exist "%SYSROOT%\include\windows.h" (
    echo downloading mingw-w64 ^(%MINGW_NAME%^)...
    powershell -NoProfile -Command "Invoke-WebRequest -Uri '%MINGW_URL%' -OutFile '%CHECKOUT%\mingw.zip'" || exit /b 1
    powershell -NoProfile -Command "Expand-Archive -Path '%CHECKOUT%\mingw.zip' -DestinationPath '%CHECKOUT%\mingw' -Force" || exit /b 1
    set "MW=%CHECKOUT%\mingw\%MINGW_NAME%"
    robocopy "!MW!\generic-w64-mingw32\include" "%SYSROOT%\include" /E /NFL /NDL /NJH /NJS /NP >nul
    robocopy "!MW!\%ARCH%-w64-mingw32\lib"     "%SYSROOT%\lib"     /E /NFL /NDL /NJH /NJS /NP >nul
    robocopy "!MW!\%ARCH%-w64-mingw32\bin"     "%NATIVE%\bin"      /E /NFL /NDL /NJH /NJS /NP >nul
    robocopy "!MW!\lib\clang"                  "%NATIVE%\lib\clang" /E /NFL /NDL /NJH /NJS /NP >nul
    if not exist "%SYSROOT%\include\windows.h" (
        echo bootstrap: mingw-w64 did not lay out -- expected %SYSROOT%\include\windows.h
        exit /b 1
    )
    rmdir /S /Q "%CHECKOUT%\mingw" 2>nul
    del "%CHECKOUT%\mingw.zip" 2>nul
)

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

:: ---------------- gnu make ----------------
:: busybox's make cannot parse a gnu makefile, and the kernel build and
:: every autotools dependency need the real one. built from source with
:: our own clang: clang-cl IS a cl driver, and lld-link a link driver,
:: so no third-party binary is taken on trust
set "MAKE_VER=4.4.1"
set "MAKE_URL=https://ftp.gnu.org/gnu/make/make-%MAKE_VER%.tar.gz"
if not exist "%NATIVE%\bin\make-%MAKE_VER%.stamp" (
    echo building gnu make %MAKE_VER%...
    set "MK=%CHECKOUT%\make-%MAKE_VER%"
    if not exist "!MK!\build_w32.bat" (
        powershell -NoProfile -Command "Invoke-WebRequest -Uri '%MAKE_URL%' -OutFile '%CHECKOUT%\make.tar.gz'" || exit /b 1
        "%SystemRoot%\System32\tar.exe" -xf "%CHECKOUT%\make.tar.gz" -C "%CHECKOUT%" || exit /b 1
        del "%CHECKOUT%\make.tar.gz"
    )
    :: its script looks for cl/link by name; our drivers answer for both
    if not exist "%CHECKOUT%\clshim" mkdir "%CHECKOUT%\clshim"
    copy /Y "%NATIVE%\bin\clang-cl.exe" "%CHECKOUT%\clshim\cl.exe"   >nul || exit /b 1
    copy /Y "%NATIVE%\bin\lld-link.exe" "%CHECKOUT%\clshim\link.exe" >nul || exit /b 1
    pushd "!MK!"
    set "PATH=%CHECKOUT%\clshim;%PATH%"
    cmd /c ".\build_w32.bat" >nul 2>&1
    popd
    if not exist "!MK!\WinRel\gnumake.exe" (
        echo bootstrap: gnu make did not build -- see !MK!\WinRel
        exit /b 1
    )
    :: delete first, never copy INTO it: the vendored tools were hard
    :: links to one inode, so writing make.exe wrote all 178 of them
    if exist "%NATIVE%\bin\make.exe" del "%NATIVE%\bin\make.exe"
    copy /Y "!MK!\WinRel\gnumake.exe" "%NATIVE%\bin\make.exe" >nul || exit /b 1
    echo %MAKE_VER% > "%NATIVE%\bin\make-%MAKE_VER%.stamp"
)

:: ---------------- busybox: the posix userland ----------------
:: kbuild is shell, and so is every autotools configure. busybox-w32 is a
:: NATIVE windows shell -- it hands configure a ';' path and reads C:\ as a
:: path, where an msys shell translates both and breaks them.
:: AFTER gnu make deliberately: busybox carries a `make` applet that cannot
:: parse a gnu makefile, and the links below never overwrite what is there.
:: the applets are hard links to the one binary, so never copy INTO one --
:: that writes through every name at once
if not exist "%NATIVE%\bin\busybox.exe" (
    echo downloading busybox-w32...
    powershell -NoProfile -Command "Invoke-WebRequest -Uri 'https://frippery.org/files/busybox/busybox64.exe' -OutFile '%NATIVE%\bin\busybox.exe'" || exit /b 1
)
if not exist "%NATIVE%\bin\sh.exe" (
    echo linking busybox applets...
    for /f "delims=" %%A in ('"%NATIVE%\bin\busybox.exe" --list') do if /i not "%%A"=="make" if not exist "%NATIVE%\bin\%%A.exe" mklink /H "%NATIVE%\bin\%%A.exe" "%NATIVE%\bin\busybox.exe" >nul
)

:: ---------------- python ----------------
:: a real python, NOT the vendored embedded one: its python311._pth replaces
:: sys.path outright, so a script never gets its own directory on the path and
:: every build script that imports a sibling module fails (mbedtls, libcxx)
set "PYTHON="
for /f "delims=" %%Q in ('where python.exe python3.exe 2^>nul') do (
    if not defined PYTHON (
        rem BOTH names: install is a junction onto platform\native, so
        rem the vendored python answers to two paths and only one was
        rem excluded -- the embedded one then won and its _pth, whose
        rem entries are relative, resolved a level high and lost src\
        echo %%Q | findstr /i /c:"%NATIVE%" /c:"%IMPORT%" >nul || set "PYTHON=%%Q"
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

:: ---------------- meson ----------------
:: rnnoise and friends build with meson. it comes from pip, and the exe
:: is vendored into our own bin: CreateProcess resolves a .exe on PATH
:: but never a .cmd, and the scripts dir is not on PATH at all
:: a store python installs to the USER site, not its own read-only
:: scripts dir, so both candidates are tried
if not exist "%NATIVE%\bin\meson.exe" (
    echo installing meson...
    "%PYTHON%" -m pip install --quiet --upgrade --no-warn-script-location meson || exit /b 1
    "%PYTHON%" -c "import sysconfig;c=[sysconfig.get_path('scripts'),sysconfig.get_path('scripts','nt_user')];open(r'%TEMP%\silver_pyscripts.txt','w').write(chr(10).join(c))" || exit /b 1
    for /f "usebackq delims=" %%S in ("%TEMP%\silver_pyscripts.txt") do (
        if not exist "%NATIVE%\bin\meson.exe" if exist "%%S\meson.exe" copy /Y "%%S\meson.exe" "%NATIVE%\bin\meson.exe" >nul
    )
    del "%TEMP%\silver_pyscripts.txt" >nul 2>&1
    if not exist "%NATIVE%\bin\meson.exe" (
        echo bootstrap: meson installed but its meson.exe was not found
        exit /b 1
    )
)

:: ---------------- rust + cbindgen ----------------
:: a module's <name>.rs companion is compiled by rustc, and cbindgen
:: emits its extern "C" header -- silver calls both by name. vendored
:: under platform\native like every other tool, never the user profile,
:: and the two homes are persisted because rustup's shims read them
set "RUSTUP_HOME=%NATIVE%\rustup"
set "CARGO_HOME=%NATIVE%\cargo"
set "PATH=%CARGO_HOME%\bin;%PATH%"
set "RUSTUP_URL=https://static.rust-lang.org/rustup/dist/x86_64-pc-windows-msvc/rustup-init.exe"
if not exist "%CARGO_HOME%\bin\rustc.exe" (
    echo installing rust...
    powershell -NoProfile -Command "Invoke-WebRequest -Uri '%RUSTUP_URL%' -OutFile '%TEMP%\rustup-init.exe'" || exit /b 1
    :: minimal profile: rustc, cargo and std, no docs or clippy. msvc
    :: host to match our own target -- a gnu host would mix abis
    "%TEMP%\rustup-init.exe" -y --no-modify-path --profile minimal ^
        --default-toolchain stable --default-host x86_64-pc-windows-msvc || exit /b 1
    del "%TEMP%\rustup-init.exe" >nul 2>&1
    if not exist "%CARGO_HOME%\bin\rustc.exe" (
        echo bootstrap: rust installed but rustc.exe was not found
        exit /b 1
    )
)

:: cbindgen turns the rust companion into a header silver can import
if not exist "%NATIVE%\bin\cbindgen.exe" (
    echo installing cbindgen...
    "%CARGO_HOME%\bin\cargo.exe" install cbindgen --root "%NATIVE%" --quiet ^
        || echo warning: cbindgen install failed; rust imports build headerless
)

:: new shells need the homes, or the shims look under %USERPROFILE%
powershell -NoProfile -Command ^
  "foreach ($kv in @{'RUSTUP_HOME'='%NATIVE%\rustup';'CARGO_HOME'='%NATIVE%\cargo'}.GetEnumerator()) { if ([Environment]::GetEnvironmentVariable($kv.Key,'User') -ne $kv.Value) { [Environment]::SetEnvironmentVariable($kv.Key, $kv.Value, 'User') } }; $p=[Environment]::GetEnvironmentVariable('PATH','User'); if ($null -eq $p) { $p='' }; $cb='%NATIVE%\cargo\bin'; if (-not $p.ToLower().Contains($cb.ToLower())) { [Environment]::SetEnvironmentVariable('PATH', $cb+';'+$p, 'User') }"

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
>>"%TC%" echo set(CMAKE_C_COMPILER   "%NATIVE:\=/%/bin/clang.exe" CACHE STRING "")
>>"%TC%" echo set(CMAKE_CXX_COMPILER "%NATIVE:\=/%/bin/clang++.exe" CACHE STRING "")
>>"%TC%" echo set(CMAKE_RC_COMPILER  "%NATIVE:\=/%/bin/llvm-windres.exe" CACHE STRING "")
>>"%TC%" echo set(CMAKE_SYSROOT      "%NATIVE:\=/%/sysroot" CACHE STRING "")
>>"%TC%" echo.
>>"%TC%" echo set(CMAKE_C_FLAGS   "--target=%TRIPLE% -w" CACHE STRING "")
>>"%TC%" echo set(CMAKE_CXX_FLAGS "--target=%TRIPLE% -stdlib=libc++ -w" CACHE STRING "")
>>"%TC%" echo set(CMAKE_EXE_LINKER_FLAGS    "-fuse-ld=lld -rtlib=compiler-rt -unwindlib=libunwind" CACHE STRING "")
>>"%TC%" echo set(CMAKE_SHARED_LINKER_FLAGS "-fuse-ld=lld -rtlib=compiler-rt -unwindlib=libunwind" CACHE STRING "")
>>"%TC%" echo set(CMAKE_MODULE_LINKER_FLAGS "-fuse-ld=lld -rtlib=compiler-rt -unwindlib=libunwind" CACHE STRING "")
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
