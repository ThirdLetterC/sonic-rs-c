<#
.SYNOPSIS
    Windows PowerShell build & test runner for sonic-rs-c with automatic MSVC (Visual Studio) detection.
.EXAMPLE
    .\build.ps1 all
    .\build.ps1 dist
    .\build.ps1 test
#>
param(
    [ValidateSet("dist", "example", "test", "all", "clean")]
    [string]$Target = "all"
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ProjectRoot

if ($Target -eq "clean") {
    cargo clean
    if (Test-Path "dist") { Remove-Item -Recurse -Force "dist" }
    Write-Host "Cleaned build artifacts."
    exit 0
}

Write-Host "==> Building sonic-rs-c release static library via Cargo..." -ForegroundColor Cyan
$env:RUSTFLAGS = "-C target-cpu=native"
cargo build --release
if ($LASTEXITCODE -ne 0) { throw "Cargo build failed." }

New-Item -ItemType Directory -Force -Path "dist/include" | Out-Null
New-Item -ItemType Directory -Force -Path "dist/lib" | Out-Null

Copy-Item -Force "include/sonic_rs.h" "dist/include/sonic_rs.h"

if (Test-Path "target/release/sonic_rs_c.lib") {
    Copy-Item -Force "target/release/sonic_rs_c.lib" "dist/lib/sonic_rs_c.lib"
    Copy-Item -Force "target/release/sonic_rs_c.lib" "dist/lib/libsonic_rs_c.a"
} elseif (Test-Path "target/release/libsonic_rs_c.a") {
    Copy-Item -Force "target/release/libsonic_rs_c.a" "dist/lib/libsonic_rs_c.a"
    Copy-Item -Force "target/release/libsonic_rs_c.a" "dist/lib/sonic_rs_c.lib"
} else {
    throw "Static library not found under target/release/"
}

Write-Host "==> Embeddable artifacts ready:" -ForegroundColor Green
Write-Host "    dist/include/sonic_rs.h"
Write-Host "    dist/lib/sonic_rs_c.lib"
Write-Host "    dist/lib/libsonic_rs_c.a"

if ($Target -eq "dist") { exit 0 }

# Ensure cl.exe is available; if not, locate Visual Studio via vswhere.exe and import its Developer environment
if (-not (Get-Command "cl.exe" -ErrorAction SilentlyContinue)) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($vsPath) {
            $vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
            if (Test-Path $vcvars) {
                Write-Host "==> Loading MSVC x64 environment from: $vcvars" -ForegroundColor Yellow
                $envVars = cmd.exe /c "`"$vcvars`" >nul && set"
                foreach ($line in $envVars) {
                    if ($line -match "^([^=]+)=(.*)$") {
                        [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
                    }
                }
            }
        }
    }
}

if (-not (Get-Command "cl.exe" -ErrorAction SilentlyContinue)) {
    throw "MSVC cl.exe not found. Install Visual Studio C++ Build Tools or run from a Developer PowerShell for VS."
}

$SysLibs = @("ws2_32.lib", "userenv.lib", "advapi32.lib", "bcrypt.lib", "ntdll.lib")

if ($Target -in @("example", "all")) {
    Write-Host "==> Compiling examples/example.c with MSVC cl.exe..." -ForegroundColor Cyan
    & cl.exe /nologo /O2 /W4 /MD /I"dist/include" "examples/example.c" "dist/lib/sonic_rs_c.lib" @SysLibs /Fe:"dist/example.exe" /Fo:"dist/example.obj"
    if ($LASTEXITCODE -ne 0) { throw "Failed to compile example.c" }
    Write-Host "==> Running dist/example.exe:" -ForegroundColor Green
    & "./dist/example.exe"
    if ($LASTEXITCODE -ne 0) { throw "example.exe exited with error" }
}

if ($Target -in @("test", "all")) {
    Write-Host "==> Compiling tests/test_c_api.c with MSVC cl.exe..." -ForegroundColor Cyan
    & cl.exe /nologo /O2 /W4 /MD /I"dist/include" "tests/test_c_api.c" "dist/lib/sonic_rs_c.lib" @SysLibs /Fe:"dist/test_c_api.exe" /Fo:"dist/test_c_api.obj"
    if ($LASTEXITCODE -ne 0) { throw "Failed to compile test_c_api.c" }
    Write-Host "==> Running dist/test_c_api.exe:" -ForegroundColor Green
    & "./dist/test_c_api.exe"
    if ($LASTEXITCODE -ne 0) { throw "test_c_api.exe exited with error" }
}
