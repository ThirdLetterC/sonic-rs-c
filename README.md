# sonic-rs-c

`sonic-rs-c` wraps the [`sonic-rs`](https://github.com/cloudwego/sonic-rs) SIMD-accelerated Rust JSON crate in a zero-cost C99/C11/C++ FFI API for **Windows (MSVC)**, **Linux**, and **macOS**.

Building the project produces:
- **Embeddable C header**: `dist/include/sonic_rs.h`
- **Static library**:
  - `dist/lib/libsonic_rs_c.a` (produced on all platforms, including Windows MSVC where the COFF archive is staged under both names)
  - `dist/lib/sonic_rs_c.lib` (native Windows MSVC static library)

---

## Building & Running on Windows with MSVC

### Prerequisites
- Rust toolchain with the `x86_64-pc-windows-msvc` (or `aarch64-pc-windows-msvc`) target installed (`rustup default stable-x86_64-pc-windows-msvc`)
- Visual Studio 2019/2022 with **Desktop development with C++** (or Build Tools for Visual Studio)

### Option A: PowerShell (`build.ps1` — Auto-Detects MSVC `cl.exe`)
You can run `build.ps1` from any standard PowerShell window; it uses `vswhere.exe` to automatically load the Visual Studio x64 environment if `cl.exe` is not already in `PATH`:

```powershell
# Build static library + header, compile example.exe and test_c_api.exe with cl.exe, and run both:
powershell -ExecutionPolicy Bypass -File .\build.ps1 all

# Or build only the dist/include + dist/lib artifacts:
powershell -ExecutionPolicy Bypass -File .\build.ps1 dist
```

### Option B: Developer Command Prompt (`build.bat`)
From an **x64 Native Tools Command Prompt for VS**:

```cmd
REM Build dist\include\sonic_rs.h, dist\lib\sonic_rs_c.lib, and dist\lib\libsonic_rs_c.a:
build.bat dist

REM Compile and run the C example and test suite with cl.exe:
build.bat all
```

### Option C: CMake with Visual Studio or Ninja
```cmd
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

### Option D: Manual MSVC `cl.exe` Command Line
Because `include/sonic_rs.h` includes `#pragma comment(lib, ...)` directives for Rust's required Windows system libraries (`ws2_32.lib`, `userenv.lib`, `advapi32.lib`, `bcrypt.lib`, `ntdll.lib`), you can compile and link your C application in a single `cl.exe` command:

```cmd
cl /nologo /O2 /MD /Idist\include my_app.c dist\lib\sonic_rs_c.lib
```
*(Note: pass `/MD` to match Rust's default dynamic Universal CRT `msvcrt.lib`. You can pass either `dist\lib\sonic_rs_c.lib` or `dist\lib\libsonic_rs_c.a` to `cl.exe` / `link.exe`.)*

---

## Building & Running on Linux / macOS

```bash
make dist
# Produces:
#   dist/include/sonic_rs.h
#   dist/lib/libsonic_rs_c.a

# Compile and run the C example and test suite:
make example
make test-c
```

To link manually with `gcc` or `clang`:
```bash
cc -O2 -std=c11 -Idist/include my_app.c dist/lib/libsonic_rs_c.a -lpthread -ldl -lm -o my_app
```
