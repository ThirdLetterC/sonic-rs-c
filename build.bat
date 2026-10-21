@echo off
setlocal enabledelayedexpansion

REM ============================================================================
REM Native Windows MSVC Build & Test Script for sonic-rs-c
REM Run from a "Developer Command Prompt for VS" or "x64 Native Tools Command Prompt"
REM Usage:
REM   build.bat          (builds Rust static library + populates dist\)
REM   build.bat example  (builds dist\ + compiles & runs examples\example.c with cl.exe)
REM   build.bat test     (builds dist\ + compiles & runs tests\test_c_api.c with cl.exe)
REM   build.bat all      (builds dist\, example.exe, and test_c_api.exe and runs both)
REM ============================================================================

set "TARGET_CMD=%~1"
if "%TARGET_CMD%"=="" set "TARGET_CMD=dist"

if /I "%TARGET_CMD%"=="clean" (
    cargo clean
    if exist dist rmdir /s /q dist
    echo Cleaned build artifacts.
    exit /b 0
)

echo [1/2] Building sonic-rs-c release static library with Cargo (SIMD target-cpu=native)...
set "RUSTFLAGS=-C target-cpu=native"
cargo build --release
if errorlevel 1 (
    echo ERROR: Cargo build failed.
    exit /b 1
)

if not exist "dist\include" mkdir "dist\include"
if not exist "dist\lib" mkdir "dist\lib"

copy /Y "include\sonic_rs.h" "dist\include\sonic_rs.h" >nul

if exist "target\release\sonic_rs_c.lib" (
    copy /Y "target\release\sonic_rs_c.lib" "dist\lib\sonic_rs_c.lib" >nul
    copy /Y "target\release\sonic_rs_c.lib" "dist\lib\libsonic_rs_c.a" >nul
) else if exist "target\release\libsonic_rs_c.a" (
    copy /Y "target\release\libsonic_rs_c.a" "dist\lib\libsonic_rs_c.a" >nul
    copy /Y "target\release\libsonic_rs_c.a" "dist\lib\sonic_rs_c.lib" >nul
) else (
    echo ERROR: Static library not found in target\release\
    exit /b 1
)

echo ==> Produced embeddable artifacts:
echo     Header:          dist\include\sonic_rs.h
echo     MSVC static lib: dist\lib\sonic_rs_c.lib
echo     Archive (.a):    dist\lib\libsonic_rs_c.a

if /I "%TARGET_CMD%"=="dist" exit /b 0

where cl.exe >nul 2>nul
if errorlevel 1 (
    echo ERROR: cl.exe not found in PATH. Please run from a "x64 Native Tools Command Prompt for VS"
    echo        or run `powershell -ExecutionPolicy Bypass -File build.ps1 %TARGET_CMD%` which auto-detects Visual Studio.
    exit /b 1
)

set "SYS_LIBS=ws2_32.lib userenv.lib advapi32.lib bcrypt.lib ntdll.lib"

if /I "%TARGET_CMD%"=="example" goto :build_example
if /I "%TARGET_CMD%"=="test" goto :build_test
if /I "%TARGET_CMD%"=="all" goto :build_example

echo Unknown command: %TARGET_CMD%
exit /b 1

:build_example
echo [2/2] Compiling examples\example.c with MSVC cl.exe...
cl.exe /nologo /O2 /W4 /MD /I"dist\include" "examples\example.c" "dist\lib\sonic_rs_c.lib" %SYS_LIBS% /Fe:"dist\example.exe" /Fo:"dist\example.obj"
if errorlevel 1 exit /b 1
echo Running dist\example.exe:
"dist\example.exe"
if errorlevel 1 exit /b 1
if /I not "%TARGET_CMD%"=="all" exit /b 0

:build_test
echo [2/2] Compiling tests\test_c_api.c with MSVC cl.exe...
cl.exe /nologo /O2 /W4 /MD /I"dist\include" "tests\test_c_api.c" "dist\lib\sonic_rs_c.lib" %SYS_LIBS% /Fe:"dist\test_c_api.exe" /Fo:"dist\test_c_api.obj"
if errorlevel 1 exit /b 1
echo Running dist\test_c_api.exe:
"dist\test_c_api.exe"
if errorlevel 1 exit /b 1
exit /b 0
